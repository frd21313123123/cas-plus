// Small UI adapter for independent visible/occluded Chams material presets.
// It is composed before the Win32 menu helpers, so forward-declare the shared
// rounded-card primitive implemented later in dllmain.cpp.

static void DrawRoundedCard(HDC hdc, int x, int y, int w, int h,
    COLORREF bg, COLORREF border, int radius);

static const wchar_t* ChamsMaterialPresetName(int preset)
{
    switch (ClampChamsMaterialPreset(preset))
    {
    case CHAMS_MATERIAL_SHADED: return L"Shaded";
    case CHAMS_MATERIAL_METALLIC: return L"Metallic";
    case CHAMS_MATERIAL_GLOW: return L"Glow";
    default: return L"Flat";
    }
}

static void DrawChamsMaterialPresetControls(HDC hdc)
{
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT visibleLabel = { 235, 278, 375, 300 };
    DrawTextW(hdc, L"Visible material", -1, &visibleLabel,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT occludedLabel = { 235, 307, 375, 329 };
    DrawTextW(hdc, L"Occluded material", -1, &occludedLabel,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 390, 277, 155, 24, RGB_COLOR(40, 40, 48),
        RGB_COLOR(60, 60, 70), 4);
    DrawRoundedCard(hdc, 390, 306, 155, 24, RGB_COLOR(40, 40, 48),
        RGB_COLOR(60, 60, 70), 4);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT visibleButton = { 390, 277, 545, 301 };
    DrawTextW(hdc, ChamsMaterialPresetName(g_chamsVisibleMaterialPreset), -1,
        &visibleButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT occludedButton = { 390, 306, 545, 330 };
    DrawTextW(hdc, ChamsMaterialPresetName(g_chamsOccludedMaterialPreset), -1,
        &occludedButton, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static bool HandleChamsMaterialPresetClick(int mouseX, int mouseY)
{
    if (mouseX < 390 || mouseX > 545)
        return false;
    if (mouseY >= 277 && mouseY <= 301)
    {
        g_chamsVisibleMaterialPreset =
            (ClampChamsMaterialPreset(g_chamsVisibleMaterialPreset) + 1) %
            CHAMS_MATERIAL_COUNT;
        RefreshSelectedChamsMaterials();
        return true;
    }
    if (mouseY >= 306 && mouseY <= 330)
    {
        g_chamsOccludedMaterialPreset =
            (ClampChamsMaterialPreset(g_chamsOccludedMaterialPreset) + 1) %
            CHAMS_MATERIAL_COUNT;
        RefreshSelectedChamsMaterials();
        return true;
    }
    return false;
}
