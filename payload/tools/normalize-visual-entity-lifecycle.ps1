param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

# UpdateBotHighlights historically used the entity runtime embedded in the
# resolved highlight structure and returned WAITING_MAP.  The modular visual
# pipeline owns stale-target reset logic and expects the already pre-resolved
# entity runtime instead. Normalize this one lifecycle block before the visual
# generator runs; refuse any source shape other than the known current/target
# forms.
$current = @'
    void* entitySystem = CurrentEntitySystem(g_botHighlightRuntime.entity);
    if (!entitySystem)
        return BOT_HIGHLIGHT_WAITING_MAP;
'@
$normalized = @'
    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
        return BOT_HIGHLIGHT_ERR_ENTITY_SYSTEM;
'@

$currentCount = ([regex]::Matches($source, [regex]::Escape($current))).Count
$normalizedCount = ([regex]::Matches($source, [regex]::Escape($normalized))).Count
if ($currentCount -eq 1 -and $normalizedCount -eq 0) {
    $source = $source.Replace($current, $normalized)
}
elseif ($currentCount -eq 0 -and $normalizedCount -eq 1) {
    # Already normalized by a future source cleanup; keep it idempotent.
}
else {
    throw "Visual entity lifecycle normalization expected one known block (current=$currentCount normalized=$normalizedCount). Refusing to patch blindly."
}

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Normalized Visuals entity-system lifecycle anchor: $InputPath"
