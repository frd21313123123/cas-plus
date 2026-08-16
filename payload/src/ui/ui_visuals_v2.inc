// cas+ native Visuals page + shared tooltip renderer.
//
// This module talks only to ESPConfig, the same state consumed by the existing
// runtime.  Migrating the page therefore changes presentation/input ownership
// without duplicating or changing any render feature implementation.

static const wchar_t* CasUiChamsStyleName(int style)
{
    switch (style)
    {
    case 1: return L"Visible only";
    case 2: return L"Through wall";
    case 3: return L"Glow pass";
    default: return L"Dual pass";
    }
}

static void CasUiDrawTooltipV3(HDC hdc, int width, int height)
{
    if (!g_casUiTooltipText || !*g_casUiTooltipText ||
        g_inventoryStickerModal || g_espConfig.showRgbPickerModal ||
        g_espConfig.showChamsModal || g_espConfig.showManageModal)
        return;

    constexpr int tooltipW = 318;
    constexpr int tooltipH = 68;
    int x = g_casUiMouseX + 18;
    int y = g_casUiMouseY + 18;
    if (x + tooltipW > width - 10)
        x = width - tooltipW - 10;
    if (y + tooltipH > height - 10)
        y = g_casUiMouseY - tooltipH - 14;
    if (x < 10) x = 10;
    if (y < 10) y = 10;

    DrawRoundedCard(hdc, x, y, tooltipW, tooltipH,
        RGB_COLOR(18, 20, 31), CAS_UI_BORDER_HI, 7);
    CasUiDrawLabel(hdc, L"INFO", x + 12, y + 7, 46, 14,
        CAS_UI_ACCENT, 9, 700, DT_LEFT);

    HFONT font = CasUiCreateFont(11, 450);
    HGDIOBJ old = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, CAS_UI_TEXT);
    RECT rc = { x + 12, y + 23, x + tooltipW - 12, y + tooltipH - 8 };
    DrawTextW(hdc, g_casUiTooltipText, -1, &rc,
        DT_LEFT | 0x00000010 /* DT_WORDBREAK */);
    SelectObject(hdc, old);
    DeleteObject(font);
}

static void CasUiDrawToggleV3(HDC hdc, int x, int y, bool enabled)
{
    const bool hover = CasUiMouseOver(x, y, 42, 22);
    const COLORREF track = enabled ?
        (hover ? RGB_COLOR(71, 55, 122) : CAS_UI_ACCENT_SOFT) :
        (hover ? CAS_UI_SURFACE_3 : CAS_UI_SURFACE_2);
    DrawRoundedCard(hdc, x, y, 42, 22, track,
        enabled || hover ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 11);
    const int knobX = enabled ? x + 22 : x + 4;
    DrawRoundedCard(hdc, knobX, y + 4, 16, 14,
        enabled ? CAS_UI_ACCENT : CAS_UI_MUTED_2,
        enabled ? CAS_UI_ACCENT : CAS_UI_MUTED_2, 7);
}

static void CasUiDrawColorSwatchV3(HDC hdc, int x, int y,
    const RGBVal& color, const wchar_t* tooltip)
{
    const bool hover = CasUiMouseOver(x, y, 28, 22);
    DrawRoundedCard(hdc, x, y, 28, 22, color.ToRef(),
        hover ? CAS_UI_TEXT : CAS_UI_BORDER_HI, 5);
    CasUiTooltipCandidate(x, y, 28, 22, tooltip, 3);
}

static void CasUiDrawVisualRowV3(HDC hdc, int x, int y, int w,
    const wchar_t* label, bool enabled, const wchar_t* tooltip,
    const RGBVal* color = nullptr, const wchar_t* colorTooltip = nullptr)
{
    const bool hover = CasUiMouseOver(x, y, w, 34);
    if (hover)
        DrawRoundedCard(hdc, x, y, w, 34,
            RGB_COLOR(24, 27, 42), RGB_COLOR(24, 27, 42), 6);
    CasUiDrawLabel(hdc, label, x + 10, y, w - 108, 34,
        enabled ? CAS_UI_TEXT : CAS_UI_MUTED, 12, 520, DT_LEFT);
    if (color)
        CasUiDrawColorSwatchV3(hdc, x + w - 80, y + 6,
            *color, colorTooltip ? colorTooltip : tooltip);
    CasUiDrawToggleV3(hdc, x + w - 44, y + 6, enabled);
    CasUiTooltipCandidate(x, y, w, 34, tooltip, 1);
}

static void CasUiDrawSectionV3(HDC hdc, int x, int y, int w, int h,
    const wchar_t* title, const wchar_t* subtitle)
{
    DrawRoundedCard(hdc, x, y, w, h,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, title, x + 16, y + 11, w - 32, 18,
        CAS_UI_TEXT, 12, 650, DT_LEFT);
    if (subtitle)
        CasUiDrawLabel(hdc, subtitle, x + 16, y + 29, w - 32, 16,
            CAS_UI_MUTED_2, 10, 400, DT_LEFT);
}

static void CasUiDrawFlagChipV3(HDC hdc, int x, int y, int w,
    const wchar_t* label, bool enabled, const wchar_t* tooltip)
{
    const bool hover = CasUiMouseOver(x, y, w, 30);
    DrawRoundedCard(hdc, x, y, w, 30,
        enabled ? CAS_UI_ACCENT_SOFT :
            (hover ? CAS_UI_SURFACE_3 : CAS_UI_SURFACE_2),
        enabled || hover ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, label, x, y, w, 30,
        enabled ? CAS_UI_TEXT : CAS_UI_MUTED, 10, 550, DT_CENTER);
    CasUiTooltipCandidate(x, y, w, 30, tooltip, 2);
}

static void CasUiOpenVisualColorV3(RGBVal* color, const wchar_t* title)
{
    if (!color)
        return;
    g_espConfig.targetRgbColor = color;
    g_espConfig.targetRgbTitle = title ? title : L"Color Picker";
    g_espConfig.showRgbPickerModal = true;
}

static void CasUiDrawVisualsV2(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 2);
    CasUiDrawLabel(hdc, L"Player overlays, model effects and state flags",
        CAS_UI_CONTENT_X, 72, 520, 20, CAS_UI_MUTED, 11, 400, DT_LEFT);

    // Left: player overlays.
    CasUiDrawSectionV3(hdc, 174, 100, 370, 474,
        L"PLAYER ESP", L"World-space information rendered for tracked players");
    CasUiDrawVisualRowV3(hdc, 190, 148, 338,
        L"Master enable", g_espConfig.enable,
        L"Master switch for the Visuals feature set. Individual options keep their saved state.");
    CasUiDrawVisualRowV3(hdc, 190, 188, 338,
        L"Skeleton", g_espConfig.skeleton,
        L"Draw the current player skeleton using live bone positions.",
        &g_espConfig.skeletonColor, L"Change the live skeleton color.");
    CasUiDrawVisualRowV3(hdc, 190, 228, 338,
        L"History skeleton", g_espConfig.historySkeleton,
        L"Draw stored historical skeleton records behind the current player state.",
        &g_espConfig.historySkeletonColor, L"Change the history skeleton color.");
    CasUiDrawVisualRowV3(hdc, 190, 268, 338,
        L"Aim history skeleton", g_espConfig.aimHistorySkeleton,
        L"Draw the historical record selected by the targeting history logic.",
        &g_espConfig.aimHistorySkeletonColor, L"Change the aim-history skeleton color.");
    CasUiDrawVisualRowV3(hdc, 190, 308, 338,
        L"Footsteps", g_espConfig.footsteps,
        L"Render the configured footstep visualization around tracked players.",
        &g_espConfig.footstepsColor, L"Change the footstep visualization color.");
    CasUiDrawVisualRowV3(hdc, 190, 348, 338,
        L"Off-screen arrows", g_espConfig.offScreen,
        L"Show a directional indicator when a tracked player is outside the current view.",
        &g_espConfig.offScreenColor, L"Change the off-screen indicator color.");

    DrawRoundedCard(hdc, 190, 402, 338, 64,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
    CasUiDrawLabel(hdc, L"History depth", 202, 409, 112, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawLabel(hdc, L"records", 202, 430, 82, 17,
        CAS_UI_MUTED_2, 9, 400, DT_LEFT);
    RECT historyRc = { 330, 420, 378, 440 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(g_espConfig.historyTicks),
        &historyRc, DT_CENTER | DT_SINGLELINE);
    CasUiDrawButton(hdc, 392, 418, 54, 28, L"- 1");
    CasUiDrawButton(hdc, 454, 418, 58, 28, L"+ 1");
    CasUiTooltipCandidate(190, 402, 338, 64,
        L"Number of historical records requested by the history skeleton renderer.", 1);

    DrawRoundedCard(hdc, 190, 480, 338, 74,
        RGB_COLOR(13, 16, 25), CAS_UI_BORDER, 7);
    CasUiDrawLabel(hdc, L"COLOR TIP", 202, 489, 72, 16,
        CAS_UI_ACCENT, 9, 700, DT_LEFT);
    CasUiDrawLabel(hdc,
        L"Click any colored square to open the existing RGB picker.",
        202, 510, 306, 20, CAS_UI_MUTED, 10, 400, DT_LEFT);

    // Right: model effects.
    CasUiDrawSectionV3(hdc, 558, 100, 378, 286,
        L"MODEL EFFECTS", L"Depth/model rendering options");
    CasUiDrawVisualRowV3(hdc, 574, 148, 346,
        L"Glow", g_espConfig.glow,
        L"Apply the configured glow effect to tracked players.",
        &g_espConfig.glowColor, L"Change the glow color.");
    CasUiDrawVisualRowV3(hdc, 574, 188, 346,
        L"Chams", g_espConfig.chams,
        L"Enable the model-color render pipeline. The two swatches below control occluded and visible passes.");
    CasUiDrawVisualRowV3(hdc, 574, 228, 346,
        L"Enemies", g_espConfig.chamsEnemies,
        L"Apply chams to enemy player models when the Chams master switch is enabled.");
    CasUiDrawVisualRowV3(hdc, 574, 268, 346,
        L"Weapons", g_espConfig.chamsWeapons,
        L"Apply the supported chams pipeline to weapon entities.");

    const bool handsHover = CasUiMouseOver(574, 308, 346, 34);
    if (handsHover)
        DrawRoundedCard(hdc, 574, 308, 346, 34,
            RGB_COLOR(24, 27, 42), RGB_COLOR(24, 27, 42), 6);
    CasUiDrawLabel(hdc, L"Hands", 584, 308, 130, 34,
        CAS_UI_MUTED_2, 12, 520, DT_LEFT);
    CasUiDrawBadge(hdc, 824, 315, 82, L"UNAVAILABLE",
        CAS_UI_MUTED, CAS_UI_SURFACE_3);
    CasUiTooltipCandidate(574, 308, 346, 34,
        L"Hands chams stays disabled until a schema-backed local viewmodel/arms handle is validated.", 2);

    DrawRoundedCard(hdc, 574, 346, 346, 28,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, L"Style", 584, 346, 48, 28,
        CAS_UI_MUTED, 10, 500, DT_LEFT);
    CasUiDrawButton(hdc, 638, 346, 34, 28, L"<");
    CasUiDrawLabel(hdc, CasUiChamsStyleName(g_espConfig.chamsStyle),
        676, 346, 164, 28, CAS_UI_TEXT, 10, 550, DT_CENTER);
    CasUiDrawButton(hdc, 844, 346, 34, 28, L">");
    CasUiDrawColorSwatchV3(hdc, 884, 349,
        g_espConfig.chamsColorOccluded,
        L"Change the chams color used by the occluded/through-wall pass.");
    // Visible color sits as a compact second swatch immediately left of the
    // right edge; both are independently clickable below in hit-testing.
    CasUiDrawColorSwatchV3(hdc, 852, 349,
        g_espConfig.chamsColorVisible,
        L"Change the chams color used by the normal visible pass.");
    CasUiTooltipCandidate(574, 346, 266, 28,
        L"Choose which chams passes are active: dual, visible-only, through-wall-only or glow pass.", 1);

    // Right bottom: compact player-state flags.
    CasUiDrawSectionV3(hdc, 558, 398, 378, 176,
        L"PLAYER FLAGS", L"Small state labels displayed by the ESP");
    CasUiDrawFlagChipV3(hdc, 574, 442, 78, L"HK", g_espConfig.flagHK,
        L"Show the configured HK/player-state flag.");
    CasUiDrawFlagChipV3(hdc, 658, 442, 78, L"ZOOM", g_espConfig.flagZoom,
        L"Show a flag when the tracked player is scoped/zoomed.");
    CasUiDrawFlagChipV3(hdc, 742, 442, 78, L"BLIND", g_espConfig.flagBlind,
        L"Show a flag when the tracked player is flashed/blinded.");
    CasUiDrawFlagChipV3(hdc, 826, 442, 78, L"RELOAD", g_espConfig.flagReload,
        L"Show a flag while the tracked player is reloading.");
    CasUiDrawFlagChipV3(hdc, 574, 480, 78, L"C4", g_espConfig.flagC4,
        L"Show a flag for the configured C4/bomb state.");
    CasUiDrawFlagChipV3(hdc, 658, 480, 78, L"VIP", g_espConfig.flagVIP,
        L"Show the configured VIP/state flag when available.");
    CasUiDrawFlagChipV3(hdc, 742, 480, 78, L"DEFUSE", g_espConfig.flagDefuse,
        L"Show a flag for defuse-related player state.");
    CasUiDrawFlagChipV3(hdc, 826, 480, 78, L"PIN", g_espConfig.flagPin,
        L"Show the configured PIN/state flag.");

    CasUiDrawLabel(hdc, L"Hover any control for an explanation.",
        574, 530, 300, 20, CAS_UI_MUTED_2, 9, 400, DT_LEFT);
}

static bool CasUiHandleVisualsV2Click(int mouseX, int mouseY)
{
    // Player ESP rows. Clicking a swatch edits color; the rest toggles.
    if (CasUiPointInRect(mouseX, mouseY, 190, 148, 338, 34))
    {
        g_espConfig.enable = !g_espConfig.enable;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 190, 188, 338, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 448, 194, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.skeletonColor,
                L"Skeleton RGB Color");
        else
            g_espConfig.skeleton = !g_espConfig.skeleton;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 190, 228, 338, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 448, 234, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.historySkeletonColor,
                L"History Skeleton RGB Color");
        else
            g_espConfig.historySkeleton = !g_espConfig.historySkeleton;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 190, 268, 338, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 448, 274, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.aimHistorySkeletonColor,
                L"Aim History Skeleton RGB Color");
        else
            g_espConfig.aimHistorySkeleton = !g_espConfig.aimHistorySkeleton;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 190, 308, 338, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 448, 314, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.footstepsColor,
                L"Footsteps RGB Color");
        else
            g_espConfig.footsteps = !g_espConfig.footsteps;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 190, 348, 338, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 448, 354, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.offScreenColor,
                L"Off-screen Arrow RGB Color");
        else
            g_espConfig.offScreen = !g_espConfig.offScreen;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 392, 418, 54, 28))
    {
        if (g_espConfig.historyTicks > 1)
            --g_espConfig.historyTicks;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 454, 418, 58, 28))
    {
        if (g_espConfig.historyTicks < 64)
            ++g_espConfig.historyTicks;
        return true;
    }

    // Model effects.
    if (CasUiPointInRect(mouseX, mouseY, 574, 148, 346, 34))
    {
        if (CasUiPointInRect(mouseX, mouseY, 840, 154, 28, 22))
            CasUiOpenVisualColorV3(&g_espConfig.glowColor,
                L"Glow RGB Color");
        else
            g_espConfig.glow = !g_espConfig.glow;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 574, 188, 346, 34))
    {
        g_espConfig.chams = !g_espConfig.chams;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 574, 228, 346, 34))
    {
        g_espConfig.chamsEnemies = !g_espConfig.chamsEnemies;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 574, 268, 346, 34))
    {
        g_espConfig.chamsWeapons = !g_espConfig.chamsWeapons;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 638, 346, 34, 28))
    {
        g_espConfig.chamsStyle = (g_espConfig.chamsStyle + 3) % 4;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 844, 346, 34, 28))
    {
        g_espConfig.chamsStyle = (g_espConfig.chamsStyle + 1) % 4;
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 884, 349, 28, 22))
    {
        CasUiOpenVisualColorV3(&g_espConfig.chamsColorOccluded,
            L"Chams Occluded RGB Color");
        return true;
    }
    if (CasUiPointInRect(mouseX, mouseY, 852, 349, 28, 22))
    {
        CasUiOpenVisualColorV3(&g_espConfig.chamsColorVisible,
            L"Chams Visible RGB Color");
        return true;
    }

    // Flags.
    if (CasUiPointInRect(mouseX, mouseY, 574, 442, 78, 30))
        g_espConfig.flagHK = !g_espConfig.flagHK;
    else if (CasUiPointInRect(mouseX, mouseY, 658, 442, 78, 30))
        g_espConfig.flagZoom = !g_espConfig.flagZoom;
    else if (CasUiPointInRect(mouseX, mouseY, 742, 442, 78, 30))
        g_espConfig.flagBlind = !g_espConfig.flagBlind;
    else if (CasUiPointInRect(mouseX, mouseY, 826, 442, 78, 30))
        g_espConfig.flagReload = !g_espConfig.flagReload;
    else if (CasUiPointInRect(mouseX, mouseY, 574, 480, 78, 30))
        g_espConfig.flagC4 = !g_espConfig.flagC4;
    else if (CasUiPointInRect(mouseX, mouseY, 658, 480, 78, 30))
        g_espConfig.flagVIP = !g_espConfig.flagVIP;
    else if (CasUiPointInRect(mouseX, mouseY, 742, 480, 78, 30))
        g_espConfig.flagDefuse = !g_espConfig.flagDefuse;
    else if (CasUiPointInRect(mouseX, mouseY, 826, 480, 78, 30))
        g_espConfig.flagPin = !g_espConfig.flagPin;
    else
        return true;
    return true;
}
