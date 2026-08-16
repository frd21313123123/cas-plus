param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

$replacements = @(
    @(
        '"-dx11 -insecure -allow_third_party_software -novid -nojoy\\n"',
        '"-dx11 -allow_third_party_software -novid -nojoy\\n"'
    ),
    @(
        'std::wstring launchArgs = L"-dx11 -insecure -allow_third_party_software -novid -nojoy";',
        'std::wstring launchArgs = L"-dx11 -allow_third_party_software -novid -nojoy";'
    ),
    @(
@'
            std::string argStr = argv[i];
            launchArgs += std::wstring(argStr.begin(), argStr.end()) + L" ";
'@,
@'
            std::string argStr = argv[i];
            std::wstring wideArg(argStr.begin(), argStr.end());
            // Never allow the loader to launch CS2 with -insecure, even when
            // a caller explicitly passes it on loader.exe's command line.
            if (_wcsicmp(wideArg.c_str(), L"-insecure") == 0)
                continue;
            launchArgs += wideArg + L" ";
'@
    ),
    @(
        '"  [*] Launching Counter-Strike 2 with flags (-dx11 -insecure ...)\\n"',
        '"  [*] Launching Counter-Strike 2 without -insecure (-dx11 ...)\\n"'
    )
)

foreach ($pair in $replacements) {
    $old = $pair[0]
    $new = $pair[1]
    $count = ([regex]::Matches($source, [regex]::Escape($old))).Count
    if ($count -ne 1) {
        throw "Secure-loader anchor mismatch (expected exactly one match, got $count): $old"
    }
    $source = $source.Replace($old, $new)
}

# The generated translation unit is the source of truth for the binary. Refuse
# to build if a future edit reintroduces the launch flag anywhere outside the
# intentional command-line filter/comment added above.
$unsafe = $source -replace 'L"-insecure"', 'L"<filtered>"'
$unsafe = $unsafe -replace 'without -insecure', 'without <filtered>'
$unsafe = $unsafe -replace 'with -insecure', 'with <filtered>'
$unsafe = $unsafe -replace 'Never allow the loader to launch CS2 with -insecure', 'Never allow the loader to launch CS2 with <filtered>'
if ($unsafe.Contains('-insecure')) {
    throw 'Generated loader source still contains an unfiltered -insecure launch flag.'
}

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Generated secure loader source (no -insecure launch flag): $OutputPath"
