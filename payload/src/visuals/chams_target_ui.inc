// Compact target-category controls for the Chams modal.
// The modal itself is implemented later in dllmain.cpp, so forward-declare the
// shared UI/lifecycle helpers used by this adapter.

static void DrawToggleSwitch(HDC hdc, int x, int y, bool active);
static void QueueBotHighlight(bool enabled);
static void DrawChamsRuntimeDiagnostics(HDC hdc);

static void DrawChamsTargetControls(HDC hdc, const ESPConfig& cfg)
{
    // Keep this heading below the modal title. The previous layout placed
    // "Targets" on the same baseline as the title and made the new controls
    // look like they were not present at all.
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT title = { 405, 145, 615, 166 };
    DrawTextW(hdc, L"Targets", -1, &title, DT_LEFT | DT_SINGLELINE);

    struct TargetRow {
        const wchar_t* label;
        bool active;
        int x;
        int y;
    };
    const TargetRow rows[] = {
        { L"Enemy",    cfg.chamsEnemies,        405, 170 },
        { L"Ally",     cfg.chamsAllies,         405, 194 },
        { L"Local",    cfg.chamsLocal,          405, 218 },
        { L"Weapons",  cfg.chamsWeapons,        405, 242 },
        { L"Hands",    cfg.chamsHands,          510, 170 },
        { L"Dropped",  cfg.chamsDroppedWeapons, 510, 194 },
        { L"Nades",    cfg.chamsGrenades,       510, 218 },
        { L"Bomb",     cfg.chamsBomb,           510, 242 }
    };

    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
    {
        DrawToggleSwitch(hdc, rows[i].x, rows[i].y, rows[i].active);
        RECT label = { rows[i].x + 43, rows[i].y,
            rows[i].x + 105, rows[i].y + 20 };
        DrawTextW(hdc, rows[i].label, -1, &label,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    DrawChamsRuntimeDiagnostics(hdc);
}

static bool HandleChamsTargetClick(int mouseX, int mouseY)
{
    bool* target = nullptr;
    if (mouseX >= 405 && mouseX <= 443)
    {
        if (mouseY >= 170 && mouseY <= 190) target = &g_espConfig.chamsEnemies;
        else if (mouseY >= 194 && mouseY <= 214) target = &g_espConfig.chamsAllies;
        else if (mouseY >= 218 && mouseY <= 238) target = &g_espConfig.chamsLocal;
        else if (mouseY >= 242 && mouseY <= 262) target = &g_espConfig.chamsWeapons;
    }
    else if (mouseX >= 510 && mouseX <= 548)
    {
        if (mouseY >= 170 && mouseY <= 190) target = &g_espConfig.chamsHands;
        else if (mouseY >= 194 && mouseY <= 214) target = &g_espConfig.chamsDroppedWeapons;
        else if (mouseY >= 218 && mouseY <= 238) target = &g_espConfig.chamsGrenades;
        else if (mouseY >= 242 && mouseY <= 262) target = &g_espConfig.chamsBomb;
    }
    if (!target)
        return false;

    *target = !*target;
    QueueBotHighlight(g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow));
    return true;
}
