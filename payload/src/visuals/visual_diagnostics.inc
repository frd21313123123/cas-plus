// Compact renderer diagnostics appended to the existing Visuals status label.
// Counters are monotonic so a runtime test can distinguish hook resolution,
// target classification, material creation and mesh-layout failures.

static void AppendVisualRendererDiagnostics(WideStatusBuilder* status)
{
    if (!status)
        return;
    AppendStatusText(status, L" | mesh ");
    AppendStatusText(status, g_meshRenderBackend.installed ? L"on" : L"fallback");
    AppendStatusText(status, L", mat ");
    AppendStatusText(status, g_materialManager.materialsReady ? L"on" : L"fallback");
    AppendStatusText(status, L", target ");
    AppendStatusUnsigned(status, g_meshRenderBackend.targetDraws);
    AppendStatusText(status, L", p ");
    AppendStatusUnsigned(status, g_meshRenderBackend.pawnDraws);
    AppendStatusText(status, L", w ");
    AppendStatusUnsigned(status, g_meshRenderBackend.weaponDraws);
    AppendStatusText(status, L", h ");
    AppendStatusUnsigned(status, g_meshRenderBackend.armsDraws);
    AppendStatusText(status, L", world ");
    AppendStatusUnsigned(status, g_meshRenderBackend.worldDraws);
    AppendStatusText(status, L", iz ");
    AppendStatusUnsigned(status, g_meshRenderBackend.occludedPasses);
    AppendStatusText(status, L", vis ");
    AppendStatusUnsigned(status, g_meshRenderBackend.visiblePasses);
    AppendStatusText(status, L", scan ");
    AppendStatusUnsigned(status, g_visualTargetCollectorStats.worldScans);
    AppendStatusText(status, L", fb ");
    AppendStatusUnsigned(status, g_meshRenderBackend.materialFallbacks);
    AppendStatusText(status, L", mf ");
    AppendStatusUnsigned(status, g_materialManager.resolveFailures +
        g_materialManager.createFailures);
    AppendStatusText(status, L", lr ");
    AppendStatusUnsigned(status, g_meshRenderBackend.layoutRejects);
}
