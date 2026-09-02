param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $PSScriptRoot '..\..\.tmp\catalog-regression'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$loaderRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$generatedHeader = Join-Path $OutputDirectory 'game_catalog.prefab.h'
& (Join-Path $loaderRoot 'tools\fix-game-catalog-prefabs.ps1') `
    -InputPath (Join-Path $loaderRoot 'src\game_catalog.h') -OutputPath $generatedHeader
& (Join-Path $loaderRoot 'tools\fix-game-catalog-vpk-io.ps1') -InputPath $generatedHeader
& (Join-Path $loaderRoot 'tools\fix-game-catalog-icon-compat.ps1') -InputPath $generatedHeader

# Match the actual payload manifest order. The order normalizer adds another
# Ready declaration, so diagnostic injection must use a unique contextual anchor.
$payloadTools = Join-Path $loaderRoot '..\payload\tools'
$patcherFixture = Join-Path $OutputDirectory 'catalog-patcher-stages.cpp'
$fixtureText = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'catalog_patcher_fixture.cpp')).Replace("`r`n", "`n").Replace("`n", "`r`n")
[IO.File]::WriteAllText($patcherFixture, $fixtureText, [Text.UTF8Encoding]::new($false))
& (Join-Path $payloadTools 'normalize-inventory-game-catalog-order.ps1') -InputPath $patcherFixture
& (Join-Path $payloadTools 'fix-inventory-game-catalog-runtime.ps1') -InputPath $patcherFixture
$patchedFixture = [IO.File]::ReadAllText($patcherFixture)
if (([regex]::Matches($patchedFixture, 'static bool InventoryGameCatalogReady\(\);')).Count -ne 2 -or
    ([regex]::Matches($patchedFixture, 'static bool ReloadInventoryGameCatalog\(\);')).Count -ne 1 -or
    $patchedFixture.Contains('cas_plus_game_catalog_v1.bin')) {
    throw 'Catalog declaration/diagnostic stage-order regression.'
}
Write-Host 'Catalog patcher stage order: passed.'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC C++ build tools were not found.' }
$msvc = Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Tools\MSVC') -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdk = Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'Include') -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvc -or -not $sdk) { throw 'MSVC or Windows SDK headers were not found.' }
$includePaths = @((Join-Path $msvc.FullName 'include'),
    (Join-Path $sdk.FullName 'ucrt'), (Join-Path $sdk.FullName 'shared'), (Join-Path $sdk.FullName 'um'))
$libPaths = @((Join-Path $msvc.FullName 'lib\x64'),
    (Join-Path $sdkRoot "Lib\$($sdk.Name)\ucrt\x64"),
    (Join-Path $sdkRoot "Lib\$($sdk.Name)\um\x64"))
$compiler = Join-Path $msvc.FullName 'bin\Hostx64\x64\cl.exe'
$arguments = @('/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/wd4505', '/utf-8',
    "/I$OutputDirectory", "/Fo$OutputDirectory\catalog_regression.obj",
    "/Fe$OutputDirectory\catalog_regression.exe")
foreach ($path in $includePaths) { $arguments += "/I$path" }
$arguments += (Join-Path $PSScriptRoot 'catalog_regression.cpp')
$arguments += '/link'
foreach ($path in $libPaths) { $arguments += "/LIBPATH:$path" }
& $compiler @arguments
if ($LASTEXITCODE -ne 0) { throw "Catalog regression compile failed ($LASTEXITCODE)." }
# A unique fixture directory makes repeated runs deterministic without deleting
# or overwriting any previous artifact or the user's actual catalog sidecar.
$fixtureDirectory = Join-Path $OutputDirectory ('fixtures-' + [guid]::NewGuid().ToString('N'))
& (Join-Path $OutputDirectory 'catalog_regression.exe') $fixtureDirectory
if ($LASTEXITCODE -ne 0) { throw "Catalog regression failed ($LASTEXITCODE)." }
