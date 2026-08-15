// Compact target-category controls for the Chams modal.
// The modal itself is implemented later in dllmain.cpp, so forward-declare the
// shared UI/lifecycle helpers used by this adapter.

static void DrawToggleSwitch(HDC hdc, int x, int y, bool active);
static void QueueBotHighlight(bool enabled);

static void DrawChamsTargetControls(HDC hdc, const ESPConfig& cfg)
{
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT title = { 385, 122, 600, 143 };
    DrawTextW(hdc, L"Targets", -1, &title, DT_LEFT | DT_SINGLELINE);

    struct TargetRow {
        const wchar_t* label;
        bool active;
        int x;
        int y;
    };
    const TargetRow rows[] = {
        { L"Enemy",    cfg.chamsEnemies,        385, 150 },
        { L"Ally",     cfg.chamsAllies,         385, 178 },
        { L"Local",    cfg.chamsLocal,          385, 206 },
        { L"Weapons",  cfg.chamsWeapons,        385, 234 },
        { L"Hands",    cfg.chamsHands,          500, 150 },
        { L"Dropped",  cfg.chamsDroppedWeapons, 500, 178 },
        { L"Nades",    cfg.chamsGrenades,       500, 206 },
        { L"Bomb",     cfg.chamsBomb,           500, 234 }
    };

    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        DrawToggleSwitch(hdc, rows[i].x, rows[i].y, rows[i].active);
        RECT label = { rows[i].x + 43, rows[i].y,
            rows[i].x + 110, rows[i].y + 20 };
        DrawTextW(hdc, rows[i].label, -1, &label,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static bool HandleChamsTargetClick(int mouseX, int mouseY)
{
    bool* target = nullptr;
    if (mouseX >= 385 && mouseX <= 423)
    {
        if (mouseY >= 150 && mouseY <= 170) target = &g_espConfig.chamsEnemies;
        else if (mouseY >= 178 && mouseY <= 198) target = &g_espConfig.chamsAllies;
        else if (mouseY >= 206 && mouseY <= 226) target = &g_espConfig.chamsLocal;
        else if (mouseY >= 234 && mouseY <= 254) target = &g_espConfig.chamsWeapons;
    }
    else if (mouseX >= 500 && mouseX <= 538)
    {
        if (mouseY >= 150 && mouseY <= 170) target = &g_espConfig.chamsHands;
        else if (mouseY >= 178 && mouseY <= 198) target = &g_espConfig.chamsDroppedWeapons;
        else if (mouseY >= 206 && mouseY <= 226) target = &g_espConfig.chamsGrenades;
        else if (mouseY >= 234 && mouseY <= 254) target = &g_espConfig.chamsBomb;
    }
    if (!target)
        return false;

    *target = !*target;
    QueueBotHighlight(g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow));
    return true;
}
