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

# The legacy renderer generator contains an old compatibility removal that
# deletes the exact target-control substring later required by its own full UI
# replacement anchor. Normalize that one build-tool line in the checkout before
# invoking the generator. Keep this exact and fail closed so it cannot become a
# broad/self-modifying rewrite after future generator edits.
$rendererPatcher = Join-Path $PSScriptRoot 'apply-chams-render-pipeline.ps1'
$patcherSource = Get-Content -LiteralPath $rendererPatcher -Raw -Encoding UTF8
$conflictingLine = 'if ($source.Contains($oldTargetToggle)) { $source = $source.Replace($oldTargetToggle, '''') }'
$conflictCount = ([regex]::Matches($patcherSource,
    [regex]::Escape($conflictingLine))).Count
if ($conflictCount -ne 1) {
    throw "Visual renderer patch-order compatibility line expected exactly once, found $conflictCount. Refusing to continue blindly."
}
$patcherSource = $patcherSource.Replace($conflictingLine,
    '# Compatibility normalizer: preserve target controls for the full replacement anchor below.')
Set-Content -LiteralPath $rendererPatcher -Value $patcherSource -Encoding UTF8 -NoNewline

Write-Host "Validated current Visuals lifecycle and normalized renderer patch order: $InputPath"
