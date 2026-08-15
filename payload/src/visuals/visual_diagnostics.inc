// Compact renderer diagnostics appended to the existing Visuals status label.
// Counters are intentionally monotonic: they make it obvious whether the scene
// hook is executing even when a particular material/target classification fails.

static void AppendVisualRendererDiagnostics(WideStatusBuilder* status)
{
    if (!status)
        return;
    AppendStatusText(status, L" | mesh ");
    AppendStatusText(status, g_meshRenderBackend.installed ? L"on" : L"fallback");
    AppendStatusText(status, L", mat ");
    AppendStatusText(status, g_materialManager.materialsReady ? L"on" : L"fallback");
    AppendStatusText(status, L", target draws ");
    AppendStatusUnsigned(status, g_meshRenderBackend.targetDraws);
    AppendStatusText(status, L", iz ");
    AppendStatusUnsigned(status, g_meshRenderBackend.occludedPasses);
    AppendStatusText(status, L", vis ");
    AppendStatusUnsigned(status, g_meshRenderBackend.visiblePasses);
    AppendStatusText(status, L", fb ");
    AppendStatusUnsigned(status, g_meshRenderBackend.materialFallbacks);
}
