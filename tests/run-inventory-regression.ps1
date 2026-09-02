param(
    [string]$BuildDirectory,
    [switch]$SkipCatalogTests,
    [switch]$SkipGenerationRepeat
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot 'artifacts\inventory-regression'
}
$buildRoot = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

# Hash the maintained files, not intermediate outputs. Builds must be repeatable
# and must not rewrite their own generators or the user's source files.
$maintainedFiles = @('payload\src', 'payload\tools', 'loader\src', 'loader\tools') |
    ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $projectRoot $_) -File -Recurse }
$beforeHashes = @{}
foreach ($file in $maintainedFiles) {
    $beforeHashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
}

$prepare = Join-Path $projectRoot 'payload\tools\prepare-inventory-payload.ps1'
$inputSource = Join-Path $projectRoot 'payload\src\dllmain.cpp'
$generatedSource = Join-Path $buildRoot 'dllmain.chams.cpp'
& $prepare -InputPath $inputSource -OutputPath $generatedSource
$firstHash = (Get-FileHash -LiteralPath $generatedSource -Algorithm SHA256).Hash
if (-not $SkipGenerationRepeat) {
    & $prepare -InputPath $inputSource -OutputPath $generatedSource
    $secondHash = (Get-FileHash -LiteralPath $generatedSource -Algorithm SHA256).Hash
    if ($firstHash -ne $secondHash) {
        throw 'Payload source generation is not deterministic.'
    }
    Write-Host "PASS: repeat generation SHA256 $firstHash"
}

if (-not $SkipCatalogTests) {
    & (Join-Path $projectRoot 'loader\tests\run-catalog-tests.ps1') `
        -OutputDirectory (Join-Path $buildRoot 'catalog')
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio C++ tools are required for the offline integration tests.'
}
$vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
    Select-Object -First 1
if (-not $vsRoot) { throw 'MSVC x64 tools were not found.' }
$toolset = Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC\Tools\MSVC') -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h') } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (-not $toolset -or -not $sdkVersion) { throw 'The MSVC/Windows SDK include directories were not found.' }

$compiler = Join-Path $toolset.FullName 'bin\Hostx64\x64\cl.exe'
$testExecutable = Join-Path $buildRoot 'inventory-integration-tests.exe'
$compilerArgs = @(
    '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/MT', '/W3', '/O2', '/Gy',
    '/DEXCEPTION_EXECUTE_HANDLER=1',
    "/I$($toolset.FullName)\include",
    "/I$($sdkVersion.FullName)\ucrt",
    "/I$($sdkVersion.FullName)\shared",
    "/I$($sdkVersion.FullName)\um",
    "/I$buildRoot",
    (Join-Path $PSScriptRoot 'inventory_integration_tests.cpp'),
    "/Fo$(Join-Path $buildRoot 'inventory-integration-tests.obj')",
    "/Fe$testExecutable", '/link', '/OPT:REF', '/INCREMENTAL:NO',
    "/LIBPATH:$($toolset.FullName)\lib\x64",
    "/LIBPATH:$sdkRoot\Lib\$($sdkVersion.Name)\ucrt\x64",
    "/LIBPATH:$sdkRoot\Lib\$($sdkVersion.Name)\um\x64",
    'kernel32.lib', 'user32.lib', 'gdi32.lib'
)
& $compiler @compilerArgs
if ($LASTEXITCODE -ne 0) { throw "Inventory integration test compilation failed: $LASTEXITCODE" }

# This executable does not call DllMain, create the payload worker, launch CS2,
# inject a DLL, or touch any real inventory file. It exercises the compiled
# production functions with test-owned memory and offscreen GDI surfaces.
& $testExecutable $buildRoot
if ($LASTEXITCODE -ne 0) { throw "Inventory integration tests failed: $LASTEXITCODE" }

foreach ($file in $maintainedFiles) {
    $afterHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    if ($beforeHashes[$file.FullName] -ne $afterHash) {
        throw "A build/test unexpectedly modified a maintained source: $($file.FullName)"
    }
}
Write-Host 'PASS: no maintained source files were modified by generation or tests.'
Write-Host "Offline inventory regression results and PNG renders: $buildRoot"
