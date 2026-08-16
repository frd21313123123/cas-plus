param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

# The visual pipeline owns its own shutdown cleanup and inserts it immediately
# before RemoveFrameStageBridge(). The inventory patcher deliberately looks for
# the worker-loop -> frame-bridge boundary so it can restore game-facing econ
# state before that bridge disappears. Normalize that boundary without changing
# any visual cleanup statements: only move RemoveFrameStageBridge() in front of
# the visual teardown block. This keeps both patchers independent and gives the
# inventory patcher one stable lifecycle anchor.
$visualShutdown = @'
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
    const bool meshRemoved = RemoveMeshRenderBackend();
    if (meshRemoved)
    {
        ResetMaterialManagerState();
        ResetVisualTargets();
        g_meshRenderBackend.drawObjectTarget = nullptr;
        g_meshRenderBackend.resolved = false;
    }
    RemoveFrameStageBridge();
    return 0;
'@

$normalizedShutdown = @'
    RemoveFrameStageBridge();
    if (g_botHighlightRuntimeReady && g_originalBotHighlightCount > 0)
        RestoreAllBotHighlights(nullptr);
    const bool meshRemoved = RemoveMeshRenderBackend();
    if (meshRemoved)
    {
        ResetMaterialManagerState();
        ResetVisualTargets();
        g_meshRenderBackend.drawObjectTarget = nullptr;
        g_meshRenderBackend.resolved = false;
    }
    return 0;
'@

if (-not $source.Contains($visualShutdown)) {
    throw 'Visual shutdown block was not found. Refusing to normalize an unknown generated payload.'
}

$source = $source.Replace($visualShutdown, $normalizedShutdown)
Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Normalized inventory lifecycle injection points: $InputPath"
