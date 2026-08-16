param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_visual_refresh.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Inventory visual refresh module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory visual-refresh frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $source.Substring($hookIndex)

# On first observation the projected definition has already been written. Kick
# UpdateSubclass once for a newly tracked, catalog-confirmed weapon so knife
# definition swaps get a chance to rebuild their subclass/model state.
$newRecordAnchor = @'
        record->refreshBudget = 3;
        record->refreshCooldown = 0;
        record->seen = true;
        return record;
'@
$newRecordReplacement = @'
        record->refreshBudget = 3;
        record->refreshCooldown = 0;
        record->seen = true;
        if (g_inventoryVisualRefreshRuntime.subclassReady)
        {
            g_inventoryVisualRefreshRuntime.updateSubclass(entity);
            ++g_inventorySubclassRefreshCalls;
        }
        return record;
'@
if (-not $source.Contains($newRecordAnchor)) {
    throw 'Inventory visual-refresh new-record anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($newRecordAnchor, $newRecordReplacement)

$frameAnchor = @'
        UpdateInventoryChanger();
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
        UpdateInventoryChanger();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
if (-not $source.Contains($frameAnchor)) {
    throw 'Inventory visual-refresh update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($frameAnchor, $frameReplacement)

$installAnchor = @'
    BotHighlightRuntime resolvedBot{};
    if (ResolveBotHighlightRuntime(clientModule, &resolvedBot))
    {
        g_botHighlightRuntime = resolvedBot;
        g_botHighlightRuntimeReady = true;
    }
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
'@
$installReplacement = @'
    BotHighlightRuntime resolvedBot{};
    if (ResolveBotHighlightRuntime(clientModule, &resolvedBot))
    {
        g_botHighlightRuntime = resolvedBot;
        g_botHighlightRuntimeReady = true;
    }
    ResolveInventoryVisualRefreshRuntime(clientModule);
    auto createInterface = reinterpret_cast<CreateInterfaceFn>(
'@
if (-not $source.Contains($installAnchor)) {
    throw 'Inventory visual-refresh install anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($installAnchor, $installReplacement)

$shutdownAnchor = @'
    ShutdownInventoryChanger();
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
'@
$shutdownReplacement = @'
    ShutdownInventoryChanger();
    ResetInventoryVisualRefresh();
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
'@
if (-not $source.Contains($shutdownAnchor)) {
    throw 'Inventory visual-refresh shutdown anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($shutdownAnchor, $shutdownReplacement)

# Add compact diagnostics to the Inventory panel without changing the base
# runtime struct or worker synchronization.
$diagAnchor = @'
    DrawTextW(hdc, L"Local virtual inventory only - Steam/GC state is untouched.",
        -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
'@
$diagReplacement = @'
    DrawTextW(hdc, L"Local virtual inventory only - Steam/GC state is untouched.",
        -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT refreshDiag = { 598, 444, 735, 463 };
    if (g_inventoryVisualRefreshRuntime.skinReady)
        DrawTextW(hdc, L"refresh: skin/subclass", -1, &refreshDiag,
            DT_RIGHT | DT_SINGLELINE);
    else
        DrawTextW(hdc, L"refresh: fallback only", -1, &refreshDiag,
            DT_RIGHT | DT_SINGLELINE);
'@
if (-not $source.Contains($diagAnchor)) {
    throw 'Inventory visual-refresh diagnostics anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($diagAnchor, $diagReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected guarded inventory visual refresh backend: $OutputPath"
