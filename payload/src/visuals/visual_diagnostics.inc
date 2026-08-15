#pragma auto_inline(on)

// Compact renderer diagnostics appended to the existing Visuals status label.
// Counters are monotonic so a runtime test can distinguish hook resolution,
// target publication/classification, ownership confirmation and patch safety.

static unsigned int PublishedVisualTargetCountForDiagnostics()
{
    // UI diagnostics are advisory only. Never block the render path: retain the
    // last successfully observed count if the short publication lock is busy.
    static unsigned int lastCount = 0;
    if (!TryLockVisualTargets())
        return lastCount;
    int count = g_visualTargets.count;
    if (count < 0)
        count = 0;
    if (count > kMaxVisualTargets)
        count = kMaxVisualTargets;
    lastCount = static_cast<unsigned int>(count);
    UnlockVisualTargets();
    return lastCount;
}

static void AppendVisualRendererDiagnostics(WideStatusBuilder* status)
{
    if (!status)
        return;
    AppendStatusText(status, L" | mesh ");
    AppendStatusText(status, g_meshRenderBackend.installed ? L"on" : L"fallback");
    AppendStatusText(status, L", mat ");
    AppendStatusText(status, g_materialManager.materialsReady ? L"on" : L"fallback");
    AppendStatusText(status, L", calls ");
    AppendStatusUnsigned(status, g_meshRenderBackend.drawCalls);
    AppendStatusText(status, L", pub ");
    AppendStatusUnsigned(status, PublishedVisualTargetCountForDiagnostics());
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

// Always-visible renderer state inside the Chams modal. Keep the most useful
// discriminator fields here: whether DrawObject is actually entering the hook,
// whether FrameStage published targets, and whether any target reached passes.
static void DrawChamsRuntimeDiagnostics(HDC hdc)
{
    WideStatusBuilder status;
    status.length = 0;
    status.text[0] = 0;
    AppendStatusText(&status, g_meshRenderBackend.installed ? L"MESH" : L"LEGACY");
    AppendStatusText(&status, g_materialManager.materialsReady ? L" MAT" : L" noMAT");
    AppendStatusText(&status, L" | dc ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.drawCalls);
    AppendStatusText(&status, L" pub ");
    AppendStatusUnsigned(&status, PublishedVisualTargetCountForDiagnostics());
    AppendStatusText(&status, L" t ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.targetDraws);
    AppendStatusText(&status, L" iz ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.occludedPasses);
    AppendStatusText(&status, L" vis ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.visiblePasses);
    AppendStatusText(&status, L" s ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.snapshotMisses);
    AppendStatusText(&status, L" lr ");
    AppendStatusUnsigned(&status, g_meshRenderBackend.layoutRejects);

    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT line = { 235, 262, 615, 278 };
    DrawTextW(hdc, status.text, -1, &line,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
