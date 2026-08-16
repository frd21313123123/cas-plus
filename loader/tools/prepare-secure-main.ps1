param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

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
$argCount = ([regex]::Matches($source, [regex]::Escape($oldArgBlock))).Count
if ($argCount -ne 1) {
    throw "Secure-loader custom-argument anchor mismatch (expected 1 match, got $argCount)."
}
$source = $source.Replace($oldArgBlock, $newArgBlock)

$oldLaunchStatus = 'with flags (-dx11 -insecure ...)'
$newLaunchStatus = 'without -insecure (-dx11 ...)'
$statusCount = ([regex]::Matches($source, [regex]::Escape($oldLaunchStatus))).Count
if ($statusCount -ne 1) {
    throw "Secure-loader status anchor mismatch (expected 1 match, got $statusCount)."
}
$source = $source.Replace($oldLaunchStatus, $newLaunchStatus)

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
Write-Host "Generated secure loader source (no -insecure launch flag): $OutputPath"
