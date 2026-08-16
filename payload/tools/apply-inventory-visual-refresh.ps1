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

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected current composite-material inventory refresh backend: $OutputPath"
