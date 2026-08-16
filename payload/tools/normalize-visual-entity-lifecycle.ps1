param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

$current = @'
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
'@
$count = ([regex]::Matches($source, [regex]::Escape($current))).Count
if ($count -ne 1) {
    throw "Visual entity lifecycle guard expected exactly one current block, found $count. Refusing to continue blindly."
}

# Normalize the structural renderer generator to the current modular function
# names and preserve the target-control block until its full replacement anchor.
$rendererPatcher = Join-Path $PSScriptRoot 'apply-chams-render-pipeline.ps1'
$patcherSource = Get-Content -LiteralPath $rendererPatcher -Raw -Encoding UTF8
$conflictingLine = 'if ($source.Contains($oldTargetToggle)) { $source = $source.Replace($oldTargetToggle, '''') }'
$collectorNeedle = @'
    CollectExtendedVisualTargets(g_botHighlightRuntime, entitySystem,
        localController, localPawn, localHandle, localTeam);
'@
$collectorReplacement = @'
    CollectSupplementalVisualTargets(g_botHighlightRuntime, entitySystem,
        controllers, controllerCount, localController, localPawn, localTeam);
'@
$replacements = @(
    @($conflictingLine,
        '# Compatibility normalizer: preserve target controls for the full replacement anchor below.'),
    @('EnsureSelectedChamsMaterialsReady();', 'EnsureMaterialManagerReady();'),
    @($collectorNeedle, $collectorReplacement)
)
foreach ($pair in $replacements) {
    $needle = [string]$pair[0]
    $replacement = [string]$pair[1]
    $matches = ([regex]::Matches($patcherSource, [regex]::Escape($needle))).Count
    if ($matches -ne 1) {
        throw "Visual renderer compatibility anchor expected exactly once, found $matches: $needle"
    }
    $patcherSource = $patcherSource.Replace($needle, $replacement)
}
Set-Content -LiteralPath $rendererPatcher -Value $patcherSource -Encoding UTF8 -NoNewline

# Published fast-table records are already filtered by VisualTargetKindEnabled
# in AddVisualTarget. Avoid calling that helper before its definition in the
# generated registry; the next FrameStage publication reflects UI toggle changes.
$hotPathPatcher = Join-Path $PSScriptRoot 'apply-visual-hotpath-fix.ps1'
$hotPathSource = Get-Content -LiteralPath $hotPathPatcher -Raw -Encoding UTF8
$hotNeedle = '    return VisualTargetKindEnabled(kind) ? kind : VISUAL_TARGET_NONE;'
$hotReplacement = @'
    return (kind > VISUAL_TARGET_NONE && kind <= VISUAL_TARGET_BOMB) ?
        kind : VISUAL_TARGET_NONE;
'@
$hotCount = ([regex]::Matches($hotPathSource, [regex]::Escape($hotNeedle))).Count
if ($hotCount -ne 1) {
    throw "Visual hot-path compatibility anchor expected exactly once, found $hotCount."
}
$hotPathSource = $hotPathSource.Replace($hotNeedle, $hotReplacement)
Set-Content -LiteralPath $hotPathPatcher -Value $hotPathSource -Encoding UTF8 -NoNewline

Write-Host "Validated Visuals lifecycle and normalized current renderer module names/order: $InputPath"
