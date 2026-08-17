param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Secure-loader anchor '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

$defaultFlags = '-dx11 -insecure -allow_third_party_software -novid -nojoy'
$secureFlags = '-dx11 -allow_third_party_software -novid -nojoy'
$defaultCount = ([regex]::Matches($source, [regex]::Escape($defaultFlags))).Count
if ($defaultCount -ne 2) {
    throw "Secure-loader default flag anchor mismatch (expected 2 matches, got $defaultCount)."
}
$source = $source.Replace($defaultFlags, $secureFlags)

$oldArgBlock = @'
            std::string argStr = argv[i];
            launchArgs += std::wstring(argStr.begin(), argStr.end()) + L" ";
'@
$newArgBlock = @'
            std::string argStr = argv[i];
            std::wstring wideArg(argStr.begin(), argStr.end());
            // Never allow the loader to launch CS2 with -insecure, even when
            // a caller explicitly passes it on loader.exe's command line.
            if (_wcsicmp(wideArg.c_str(), L"-insecure") == 0)
                continue;
            launchArgs += wideArg + L" ";
'@
Replace-Required $oldArgBlock $newArgBlock 'custom argument filter'

$oldLaunchStatus = 'with flags (-dx11 -insecure ...)'
$newLaunchStatus = 'without -insecure (-dx11 ...)'
Replace-Required $oldLaunchStatus $newLaunchStatus 'secure launch status'

$includeAnchor = '#include "game_catalog.h"'
$includeReplacement = @'
#include "game_catalog.h"
#include "attachment_catalog.h"
'@
Replace-Required $includeAnchor $includeReplacement.TrimEnd() 'attachment catalog include'

$injectAnchor = @'
    // 4. Inject Payload DLL
'@
$attachmentBuild = @'
    // Build real current-game sticker/patch/keychain metadata alongside the
    // weapon/paint catalog. The payload fails closed when this cache is absent,
    // so arbitrary attachment IDs can never be invented by the editor.
    cas_catalog::AttachmentBuildStats attachmentStats{};
    if (cas_catalog::buildAttachmentCatalogFromRunningGame(
        cs2Pid, &attachmentStats))
    {
        std::cout << colors::green << "  [+] Attachments ready: "
                  << attachmentStats.stickers << " stickers, "
                  << attachmentStats.patches << " patches, "
                  << attachmentStats.keychains << " keychains, "
                  << attachmentStats.duplicatesRemoved << " duplicates removed"
                  << colors::reset << "\n";
        std::wcout << colors::gray << L"      cache : "
                   << attachmentStats.outputPath.wstring()
                   << colors::reset << L"\n";
    }
    else
    {
        std::cout << colors::yellow
                  << "  [!] Could not build the real attachment catalog. Sticker/patch/charm selection will fail closed.\n"
                  << colors::reset;
    }

    // 4. Inject Payload DLL
'@
Replace-Required $injectAnchor $attachmentBuild 'attachment catalog build route'

# The generated translation unit is the source of truth for the binary. Refuse
# to build if a future edit reintroduces the launch flag anywhere outside the
# explicit command-line filter and the informational status text.
$unsafe = $source -replace 'L"-insecure"', 'L"<filtered>"'
$unsafe = $unsafe -replace 'without -insecure', 'without <filtered>'
$unsafe = $unsafe -replace 'Never allow the loader to launch CS2 with -insecure', 'Never allow the loader to launch CS2 with <filtered>'
if ($unsafe.Contains('-insecure')) {
    throw 'Generated loader source still contains an unfiltered -insecure launch flag.'
}

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Generated secure loader source with real cosmetics/attachment catalogs: $OutputPath"