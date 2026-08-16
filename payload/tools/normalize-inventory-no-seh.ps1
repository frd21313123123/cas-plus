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

# Keep the vcxproj pipeline stable: final local-only inventory layers are
# chained from this already-terminal stage. Both children use exact anchors and
# fail closed if a preceding patch changes their expected source shape.
$localOps = Join-Path $PSScriptRoot 'apply-inventory-local-ops.ps1'
$econLifecycle = Join-Path $PSScriptRoot 'normalize-inventory-econ-lifecycle.ps1'

& $localOps -InputPath $InputPath -OutputPath $InputPath
if ($LASTEXITCODE -ne 0) {
    throw "Local inventory operations stage failed with exit code $LASTEXITCODE."
}

& $econLifecycle -InputPath $InputPath
if ($LASTEXITCODE -ne 0) {
    throw "Inventory econ lifecycle normalization failed with exit code $LASTEXITCODE."
}
