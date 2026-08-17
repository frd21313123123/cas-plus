param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$tryCount = ([regex]::Matches($source, [regex]::Escape('__try'))).Count
$exceptToken = '__except (EXCEPTION_EXECUTE_HANDLER)'
$exceptCount = ([regex]::Matches($source, [regex]::Escape($exceptToken))).Count

if ($tryCount -ne 8 -or $exceptCount -ne 8) {
    throw "Unexpected inventory SEH wrapper count: try=$tryCount except=$exceptCount. Refusing to normalize blindly."
}

$source = $source.Replace('__try', 'if (true)')
$source = $source.Replace($exceptToken, 'else')

if ($source.Contains('__try') -or $source.Contains('__except')) {
    throw 'SEH tokens remain after inventory no-CRT normalization.'
}

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Normalized extended inventory guards for no-CRT payload: $InputPath"

$localOps = Join-Path $PSScriptRoot 'apply-inventory-local-ops.ps1'
$econLifecycle = Join-Path $PSScriptRoot 'normalize-inventory-econ-lifecycle.ps1'
$runtimeDiagnostics = Join-Path $PSScriptRoot 'apply-inventory-runtime-diagnostics.ps1'
$nativeEcon = Join-Path $PSScriptRoot 'fix-inventory-econ-native.ps1'
$viewFallback = Join-Path $PSScriptRoot 'fix-inventory-econ-view-fallback.ps1'
$uiRedesign = Join-Path $PSScriptRoot 'apply-ui-redesign.ps1'
$uiStickerV2 = Join-Path $PSScriptRoot 'apply-ui-sticker-v2.ps1'
$uiInteractionVisualsV2 = Join-Path $PSScriptRoot 'apply-ui-interaction-visuals-v2.ps1'
$uiInventoryTooltipsV3 = Join-Path $PSScriptRoot 'apply-ui-inventory-tooltips-v3.ps1'
$gameCatalog = Join-Path $PSScriptRoot 'apply-inventory-game-catalog.ps1'
$gameCatalogOrder = Join-Path $PSScriptRoot 'normalize-inventory-game-catalog-order.ps1'
$gameCatalogRuntimeFix = Join-Path $PSScriptRoot 'fix-inventory-game-catalog-runtime.ps1'
$attachmentsPatcherNormalize = Join-Path $PSScriptRoot 'normalize-inventory-attachments-v2-patcher.ps1'
$attachmentsV2 = Join-Path $PSScriptRoot 'apply-inventory-attachments-v2.ps1'

& $localOps -InputPath $InputPath -OutputPath $InputPath
& $econLifecycle -InputPath $InputPath
& $runtimeDiagnostics -InputPath $InputPath
& $nativeEcon -InputPath $InputPath
& $viewFallback -InputPath $InputPath
& $uiRedesign -InputPath $InputPath
& $uiStickerV2 -InputPath $InputPath
& $uiInteractionVisualsV2 -InputPath $InputPath
& $uiInventoryTooltipsV3 -InputPath $InputPath
& $gameCatalog -InputPath $InputPath
& $gameCatalogOrder -InputPath $InputPath
& $gameCatalogRuntimeFix -InputPath $InputPath
& $attachmentsPatcherNormalize
& $attachmentsV2 -InputPath $InputPath
