param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

# Guard the current lifecycle shape before the structural renderer patcher runs.
# This stage intentionally does not rewrite runtime result semantics anymore;
# apply-chams-render-pipeline.ps1 handles WAITING_MAP/WAITING_LOCAL directly.
$current = @'
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
'@
$count = ([regex]::Matches($source, [regex]::Escape($current))).Count
if ($count -ne 1) {
    throw "Visual entity lifecycle guard expected exactly one current block, found $count. Refusing to continue blindly."
}

Write-Host "Validated current Visuals entity-system lifecycle: $InputPath"
