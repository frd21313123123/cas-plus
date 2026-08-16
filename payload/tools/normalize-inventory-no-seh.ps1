param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$tryCount = ([regex]::Matches($source, [regex]::Escape('__try'))).Count
$exceptToken = '__except (EXCEPTION_EXECUTE_HANDLER)'
$exceptCount = ([regex]::Matches($source, [regex]::Escape($exceptToken))).Count

# inventory_full.inc currently owns exactly eight guarded runtime calls. The
# base payload is deliberately no-CRT, so compiling MSVC SEH would introduce
# __C_specific_handler. Normalize only when the generated source has the exact
# expected pair count; otherwise refuse to make a broad textual rewrite.
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

# Keep the vcxproj pipeline stable: final local-only inventory/UI layers are
# chained from this already-terminal stage. Children use exact anchors and
# fail closed if a preceding patch changes their expected source shape.
$localOps = Join-Path $PSScriptRoot 'apply-inventory-local-ops.ps1'
$econLifecycle = Join-Path $PSScriptRoot 'normalize-inventory-econ-lifecycle.ps1'
$runtimeDiagnostics = Join-Path $PSScriptRoot 'apply-inventory-runtime-diagnostics.ps1'
$nativeEcon = Join-Path $PSScriptRoot 'fix-inventory-econ-native.ps1'
$viewFallback = Join-Path $PSScriptRoot 'fix-inventory-econ-view-fallback.ps1'
$uiRedesign = Join-Path $PSScriptRoot 'apply-ui-redesign.ps1'
$uiStickerV2 = Join-Path $PSScriptRoot 'apply-ui-sticker-v2.ps1'
$uiInteractionVisualsV2 = Join-Path $PSScriptRoot 'apply-ui-interaction-visuals-v2.ps1'

# These are PowerShell scripts, not native processes. With ErrorActionPreference
# set to Stop any child throw terminates this stage; $LASTEXITCODE is deliberately
# not consulted because successful .ps1 invocation does not define it.
& $localOps -InputPath $InputPath -OutputPath $InputPath
& $econLifecycle -InputPath $InputPath
& $runtimeDiagnostics -InputPath $InputPath
& $nativeEcon -InputPath $InputPath
& $viewFallback -InputPath $InputPath
& $uiRedesign -InputPath $InputPath
& $uiStickerV2 -InputPath $InputPath
& $uiInteractionVisualsV2 -InputPath $InputPath
