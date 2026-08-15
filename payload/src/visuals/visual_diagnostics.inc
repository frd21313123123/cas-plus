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

// Permanent on-screen diagnostics for the Chams settings modal. Unlike the
// legacy status label this is visible exactly where renderer settings are
// changed, so runtime validation does not depend on a narrow child control.
static void DrawChamsRuntimeDiagnostics(HDC hdc)
{
    WideStatusBuilder first;
    first.length = 0;
    first.text[0] = 0;
    AppendStatusText(&first, L"Renderer: ");
    AppendStatusText(&first, g_meshRenderBackend.installed ? L"MESH" : L"LEGACY");
    AppendStatusText(&first, L" | Material: ");
    AppendStatusText(&first, g_materialManager.materialsReady ? L"ON" : L"OFF");
    AppendStatusText(&first, L" | target ");
    AppendStatusUnsigned(&first, g_meshRenderBackend.targetDraws);

    WideStatusBuilder second;
    second.length = 0;
    second.text[0] = 0;
    AppendStatusText(&second, L"iz ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.occludedPasses);
    AppendStatusText(&second, L" | vis ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.visiblePasses);
    AppendStatusText(&second, L" | mix ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.mixedBatches);
    AppendStatusText(&second, L" | ownrej ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.ownershipRejects);
    AppendStatusText(&second, L" | pf ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.patchFailures);
    AppendStatusText(&second, L" | rip ");
    AppendStatusUnsigned(&second, g_meshRenderBackend.ripDeferrals);

    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT firstLine = { 235, 338, 625, 356 };
    DrawTextW(hdc, first.text, -1, &firstLine,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT secondLine = { 235, 357, 625, 375 };
    DrawTextW(hdc, second.text, -1, &secondLine,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}
