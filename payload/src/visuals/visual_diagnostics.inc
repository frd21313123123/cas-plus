#pragma auto_inline(on)

// Compact renderer diagnostics appended to the existing Visuals status label.
// Counters are monotonic so a runtime test can distinguish hook resolution,
// target classification, ownership confirmation and patch safety deferrals.

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
    AppendStatusText(status, L", mix ");
    AppendStatusUnsigned(status, g_meshRenderBackend.mixedBatches);
    AppendStatusText(status, L", rip ");
    AppendStatusUnsigned(status, g_meshRenderBackend.ripDeferrals);
    AppendStatusText(status, L", ownrej ");
    AppendStatusUnsigned(status, g_meshRenderBackend.ownershipRejects);
    AppendStatusText(status, L", snap ");
    AppendStatusUnsigned(status, g_meshRenderBackend.snapshotMisses);
    AppendStatusText(status, L", pf ");
    AppendStatusUnsigned(status, g_meshRenderBackend.patchFailures);
    AppendStatusText(status, L", fb ");
    AppendStatusUnsigned(status, g_meshRenderBackend.materialFallbacks);
    AppendStatusText(status, L", mf ");
    AppendStatusUnsigned(status, g_materialManager.resolveFailures +
        g_materialManager.createFailures);
    AppendStatusText(status, L", lr ");
    AppendStatusUnsigned(status, g_meshRenderBackend.layoutRejects);
}

// Always-visible renderer state inside the Chams modal. This intentionally
// fits in the small gap between target toggles and material selectors so no
// compositor/layout migration is needed just to diagnose runtime behavior.
static void DrawChamsRuntimeDiagnostics(HDC hdc)
{
    WideStatusBuilder status;
    status.length = 0;
    status.text[0] = 0;
    AppendStatusText(&status, g_meshRenderBackend.installed ? L"MESH" : L"LEGACY");
    AppendStatusText(&status, g_materialManager.materialsReady ? L" MAT" : L" noMAT");
    AppendStatusText(&status, L" | t ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.targetDraws);
    AppendStatusText(&status, L" iz ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.occludedPasses);
    AppendStatusText(&status, L" vis ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.visiblePasses);
    AppendStatusText(&status, L" mix ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.mixedBatches);
    AppendStatusText(&status, L" pf ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.patchFailures);
    AppendStatusText(&status, L" rip ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.ripDeferrals);

    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT line = { 235, 262, 610, 278 };
    DrawTextW(hdc, status.text, -1, &line,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
