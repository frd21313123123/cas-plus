param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "VPK catalog IO anchor '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Loose text files such as items_game/localization can grow across game updates.
# Keep a bounded read, but do not use the old 128 MiB ceiling as a hidden schema gate.
Replace-Required `
    '128ull * 1024ull * 1024ull' `
    '512ull * 1024ull * 1024ull' `
    'loose game-file size ceiling'

$openAnchor = @'
        entries_.clear();
        directoryBytes_.clear();
        directoryPath_ = directoryVpk;
        if (!readWholeFile(directoryVpk, directoryBytes_) || directoryBytes_.size() < 12)
            return false;
        if (readU32(directoryBytes_, 0) != 0x55AA1234u)
            return false;
        const std::uint32_t version = readU32(directoryBytes_, 4);
        if (version != 1 && version != 2)
            return false;
        headerSize_ = version == 2 ? 28u : 12u;
        treeSize_ = readU32(directoryBytes_, 8);
        if (treeSize_ == 0 || headerSize_ + treeSize_ > directoryBytes_.size())
            return false;

        std::size_t cursor = headerSize_;
'@
$openReplacement = @'
        entries_.clear();
        directoryBytes_.clear();
        directoryPath_ = directoryVpk;

        // pak01_dir.vpk may contain much more than the directory tree. The
        // catalog parser only needs the VPK header/tree (including preload
        // bytes); inline payload bytes are read later by offset from the file.
        // Reading the entire directory VPK made catalog availability depend on
        // an arbitrary file-size ceiling and could silently empty Browse.
        std::ifstream directoryStream(directoryVpk, std::ios::binary);
        if (!directoryStream)
            return false;

        directoryBytes_.resize(28u);
        directoryStream.read(reinterpret_cast<char*>(directoryBytes_.data()),
            static_cast<std::streamsize>(directoryBytes_.size()));
        const std::streamsize initialRead = directoryStream.gcount();
        if (initialRead < 12)
            return false;
        if (readU32(directoryBytes_, 0) != 0x55AA1234u)
            return false;
        const std::uint32_t version = readU32(directoryBytes_, 4);
        if (version != 1 && version != 2)
            return false;
        headerSize_ = version == 2 ? 28u : 12u;
        if (initialRead < static_cast<std::streamsize>(headerSize_))
            return false;
        treeSize_ = readU32(directoryBytes_, 8);
        constexpr std::size_t kMaxVpkTreeBytes = 512ull * 1024ull * 1024ull;
        if (treeSize_ == 0 || treeSize_ > kMaxVpkTreeBytes ||
            headerSize_ + treeSize_ < headerSize_)
            return false;

        const std::size_t directoryReadSize = headerSize_ + treeSize_;
        directoryBytes_.resize(directoryReadSize);
        directoryStream.clear();
        directoryStream.seekg(0, std::ios::beg);
        if (!directoryStream)
            return false;
        directoryStream.read(reinterpret_cast<char*>(directoryBytes_.data()),
            static_cast<std::streamsize>(directoryReadSize));
        if (directoryStream.gcount() !=
            static_cast<std::streamsize>(directoryReadSize))
            return false;

        std::size_t cursor = headerSize_;
'@
Replace-Required $openAnchor $openReplacement 'stream directory header and tree only'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Upgraded game-catalog VPK IO to stream only header/tree: $InputPath"
