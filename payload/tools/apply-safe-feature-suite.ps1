param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Old, [string]$New, [string]$Name) {
    if (-not $script:source.Contains($Old)) {
        throw "Safe-suite anchor '$Name' was not found. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Old, $New)
}

# Persist the safe training/diagnostic settings alongside the existing visuals.
Replace-Required @'
    BYTE flags[8];
};

static const unsigned int kConfigMagic = 0x50415343u; // 'CASP'
static const unsigned int kConfigVersion = 1u;
static const wchar_t kConfigPath[] = L"cas-plus-profile.cfg";
static const wchar_t* g_configStatus = L"Profile ready";
'@ @'
    BYTE flags[8];
    BYTE aimGuide;
    BYTE recoilGuide;
    BYTE targetTrainer;
    int aimGuideRadius;
    BYTE jumpCue;
    BYTE strafeCadence;
    BYTE landingCue;
    BYTE diagnosticsPanel;
    RGBVal accentColor;
};

struct SafeFeatureConfig {
    bool aimGuide = true;
    bool recoilGuide = false;
    bool targetTrainer = true;
    int aimGuideRadius = 90;
    bool jumpCue = true;
    bool strafeCadence = true;
    bool landingCue = false;
    bool diagnosticsPanel = true;
    RGBVal accentColor = { 56, 189, 248 };
    int targetHits = 0;
    int targetMisses = 0;
    int targetX = 565;
    int targetY = 255;
};

static SafeFeatureConfig g_safeFeatures{};
static const unsigned int kConfigMagic = 0x50415343u; // 'CASP'
static const unsigned int kConfigVersion = 2u;
static const wchar_t* g_configStatus = L"Profile ready";
static int g_configSlot = 0;
static const wchar_t* kConfigPaths[] = {
    L"cas-plus-slot1.cfg",
    L"cas-plus-slot2.cfg",
    L"cas-plus-slot3.cfg"
};

static LPCWSTR ActiveConfigPath()
{
    if (g_configSlot < 0 || g_configSlot > 2)
        g_configSlot = 0;
    return kConfigPaths[g_configSlot];
}
'@ 'safe persisted config fields'

Replace-Required @'
    out.flags[7] = g_espConfig.flagPin ? 1 : 0;
    return out;
}
'@ @'
    out.flags[7] = g_espConfig.flagPin ? 1 : 0;
    out.aimGuide = g_safeFeatures.aimGuide ? 1 : 0;
    out.recoilGuide = g_safeFeatures.recoilGuide ? 1 : 0;
    out.targetTrainer = g_safeFeatures.targetTrainer ? 1 : 0;
    out.aimGuideRadius = g_safeFeatures.aimGuideRadius;
    out.jumpCue = g_safeFeatures.jumpCue ? 1 : 0;
    out.strafeCadence = g_safeFeatures.strafeCadence ? 1 : 0;
    out.landingCue = g_safeFeatures.landingCue ? 1 : 0;
    out.diagnosticsPanel = g_safeFeatures.diagnosticsPanel ? 1 : 0;
    out.accentColor = g_safeFeatures.accentColor;
    return out;
}
'@ 'save safe settings'

Replace-Required @'
    g_espConfig.flagPin = in.flags[7] != 0;
}
'@ @'
    g_espConfig.flagPin = in.flags[7] != 0;
    g_safeFeatures.aimGuide = in.aimGuide != 0;
    g_safeFeatures.recoilGuide = in.recoilGuide != 0;
    g_safeFeatures.targetTrainer = in.targetTrainer != 0;
    g_safeFeatures.aimGuideRadius = in.aimGuideRadius < 40 ? 40 :
        (in.aimGuideRadius > 140 ? 140 : in.aimGuideRadius);
    g_safeFeatures.jumpCue = in.jumpCue != 0;
    g_safeFeatures.strafeCadence = in.strafeCadence != 0;
    g_safeFeatures.landingCue = in.landingCue != 0;
    g_safeFeatures.diagnosticsPanel = in.diagnosticsPanel != 0;
    g_safeFeatures.accentColor = in.accentColor;
}
'@ 'load safe settings'

Replace-Required @'
    HANDLE file = CreateFileW(kConfigPath, GENERIC_WRITE, 0, nullptr,
'@ @'
    HANDLE file = CreateFileW(ActiveConfigPath(), GENERIC_WRITE, 0, nullptr,
'@ 'save active slot'

Replace-Required @'
    HANDLE file = CreateFileW(kConfigPath, GENERIC_READ, 0, nullptr,
'@ @'
    HANDLE file = CreateFileW(ActiveConfigPath(), GENERIC_READ, 0, nullptr,
'@ 'load active slot'

Replace-Required @'
    g_espConfig = ESPConfig{};
    g_espConfig.selectedTab = tab;
}
'@ @'
    g_espConfig = ESPConfig{};
    g_espConfig.selectedTab = tab;
    g_safeFeatures = SafeFeatureConfig{};
}
'@ 'reset safe settings'

# Rename dangerous/online-cheat-oriented placeholder tabs to functional offline
# training pages. Visuals, Misc and Configs remain unchanged.
Replace-Required @'
        const wchar_t* tabs[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Misc", L"Configs" };
'@ @'
        const wchar_t* tabs[] = { L"Aim Lab", L"Movement", L"Visuals", L"Misc", L"Configs" };
'@ 'safe tab names'

Replace-Required @'
    HWND wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"CAS v2.3 - ESP Settings & Interactive Preview",
'@ @'
    HWND wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"CAS+ - Offline Training, Visuals & Diagnostics",
'@ 'window title'

# Replace the placeholder page with working offline training and diagnostics UI.
$oldFoundation = @'
static void DrawFoundationPage(HDC hdc, int tab)
{
    DrawRoundedCard(hdc, 20, 50, 740, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
    const wchar_t* names[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Misc", L"Configs" };
    HFONT title = CreateFontW(20, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, title);
    SetTextColor(hdc, RGB_COLOR(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT titleRc = { 45, 75, 720, 110 };
    DrawTextW(hdc, names[tab], -1, &titleRc, DT_LEFT | DT_SINGLELINE);
    HFONT body = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, body);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT bodyRc = { 45, 125, 700, 190 };
    DrawTextW(hdc,
        L"Runtime page reserved. Controls are only exposed once a backend is implemented; no fake toggles are shown.",
        -1, &bodyRc, DT_LEFT);
    SelectObject(hdc, old);
    DeleteObject(title);
    DeleteObject(body);
}
'@

$newFoundation = @'
static void DrawFeatureTitle(HDC hdc, const wchar_t* titleText,
    const wchar_t* subtitle)
{
    HFONT title = CreateFontW(20, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, title);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB_COLOR(255, 255, 255));
    RECT titleRc = { 45, 70, 720, 100 };
    DrawTextW(hdc, titleText, -1, &titleRc, DT_LEFT | DT_SINGLELINE);
    HFONT body = CreateFontW(13, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, body);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT subRc = { 45, 102, 720, 128 };
    DrawTextW(hdc, subtitle, -1, &subRc, DT_LEFT | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(title);
    DeleteObject(body);
}

static void DrawTrainingToggleRow(HDC hdc, int y, const wchar_t* text,
    bool enabled)
{
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT label = { 55, y, 285, y + 24 };
    DrawTextW(hdc, text, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggleSwitch(hdc, 300, y + 2, enabled);
}

static void DrawAimLabPage(HDC hdc)
{
    DrawRoundedCard(hdc, 20, 50, 740, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
    DrawFeatureTitle(hdc, L"Aim Lab", L"Offline visual practice helpers; no aim or fire automation.");

    HFONT body = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, body);
    SetBkMode(hdc, TRANSPARENT);
    DrawTrainingToggleRow(hdc, 145, L"Aim guide", g_safeFeatures.aimGuide);
    DrawTrainingToggleRow(hdc, 185, L"Recoil guide", g_safeFeatures.recoilGuide);
    DrawTrainingToggleRow(hdc, 225, L"Target trainer", g_safeFeatures.targetTrainer);

    DrawRoundedCard(hdc, 45, 285, 300, 42, RGB_COLOR(32, 32, 35), RGB_COLOR(63, 63, 70), 6);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT radiusRc = { 45, 285, 345, 327 };
    const wchar_t* radiusText = g_safeFeatures.aimGuideRadius <= 60 ?
        L"Guide radius: Small" : (g_safeFeatures.aimGuideRadius <= 100 ?
        L"Guide radius: Medium" : L"Guide radius: Large");
    DrawTextW(hdc, radiusText, -1, &radiusRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawRoundedCard(hdc, 390, 130, 330, 310, RGB_COLOR(18, 18, 20), RGB_COLOR(45, 45, 50), 8);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT previewLabel = { 405, 143, 700, 165 };
    DrawTextW(hdc, L"Practice surface", -1, &previewLabel, DT_LEFT | DT_SINGLELINE);

    const int cx = 555;
    const int cy = 285;
    if (g_safeFeatures.aimGuide)
    {
        HPEN guidePen = CreatePen(PS_SOLID, 1, g_safeFeatures.accentColor.ToRef());
        HGDIOBJ oldPen = SelectObject(hdc, guidePen);
        const int r = g_safeFeatures.aimGuideRadius;
        Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
        MoveToEx(hdc, cx - 12, cy, nullptr); LineTo(hdc, cx + 13, cy);
        MoveToEx(hdc, cx, cy - 12, nullptr); LineTo(hdc, cx, cy + 13);
        SelectObject(hdc, oldPen);
        DeleteObject(guidePen);
    }
    if (g_safeFeatures.recoilGuide)
    {
        HPEN recoilPen = CreatePen(PS_SOLID, 2, RGB_COLOR(249, 115, 22));
        HGDIOBJ oldPen = SelectObject(hdc, recoilPen);
        MoveToEx(hdc, cx, cy + 18, nullptr);
        LineTo(hdc, cx - 8, cy - 5);
        LineTo(hdc, cx + 6, cy - 28);
        LineTo(hdc, cx - 4, cy - 50);
        SelectObject(hdc, oldPen);
        DeleteObject(recoilPen);
    }
    if (g_safeFeatures.targetTrainer)
    {
        HPEN targetPen = CreatePen(PS_SOLID, 2, RGB_COLOR(239, 68, 68));
        HBRUSH targetBrush = CreateSolidBrush(RGB_COLOR(63, 20, 24));
        HGDIOBJ oldPen = SelectObject(hdc, targetPen);
        HGDIOBJ oldBrush = SelectObject(hdc, targetBrush);
        Ellipse(hdc, g_safeFeatures.targetX - 13, g_safeFeatures.targetY - 13,
            g_safeFeatures.targetX + 13, g_safeFeatures.targetY + 13);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(targetPen);
        DeleteObject(targetBrush);
    }
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT hint = { 405, 410, 700, 432 };
    DrawTextW(hdc, L"Click the red target to move it", -1, &hint, DT_CENTER | DT_SINGLELINE);

    SelectObject(hdc, old);
    DeleteObject(body);
}

static void DrawMovementPage(HDC hdc)
{
    DrawRoundedCard(hdc, 20, 50, 740, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
    DrawFeatureTitle(hdc, L"Movement Training", L"Visual timing/cadence practice without input automation.");
    HFONT body = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, body);
    SetBkMode(hdc, TRANSPARENT);
    DrawTrainingToggleRow(hdc, 145, L"Jump cue", g_safeFeatures.jumpCue);
    DrawTrainingToggleRow(hdc, 185, L"Strafe cadence", g_safeFeatures.strafeCadence);
    DrawTrainingToggleRow(hdc, 225, L"Landing cue", g_safeFeatures.landingCue);

    DrawRoundedCard(hdc, 390, 130, 330, 310, RGB_COLOR(18, 18, 20), RGB_COLOR(45, 45, 50), 8);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT label = { 405, 145, 700, 170 };
    DrawTextW(hdc, L"Cadence visualizer", -1, &label, DT_LEFT | DT_SINGLELINE);
    const bool leftPhase = ((g_botHighlightFrameCounter / 18u) & 1u) == 0u;
    if (g_safeFeatures.strafeCadence)
    {
        DrawRoundedCard(hdc, 435, 220, 105, 70,
            leftPhase ? g_safeFeatures.accentColor.ToRef() : RGB_COLOR(32, 32, 35),
            RGB_COLOR(63, 63, 70), 8);
        DrawRoundedCard(hdc, 570, 220, 105, 70,
            leftPhase ? RGB_COLOR(32, 32, 35) : g_safeFeatures.accentColor.ToRef(),
            RGB_COLOR(63, 63, 70), 8);
        SetTextColor(hdc, RGB_COLOR(255, 255, 255));
        RECT l = { 435, 220, 540, 290 };
        RECT r = { 570, 220, 675, 290 };
        DrawTextW(hdc, L"LEFT", -1, &l, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(hdc, L"RIGHT", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (g_safeFeatures.jumpCue)
    {
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT jump = { 430, 320, 680, 345 };
        DrawTextW(hdc, L"JUMP: use the cadence transition", -1, &jump, DT_CENTER | DT_SINGLELINE);
    }
    if (g_safeFeatures.landingCue)
    {
        SetTextColor(hdc, RGB_COLOR(234, 179, 8));
        RECT land = { 430, 355, 680, 380 };
        DrawTextW(hdc, L"LAND: reset rhythm deliberately", -1, &land, DT_CENTER | DT_SINGLELINE);
    }
    SelectObject(hdc, old);
    DeleteObject(body);
}

static void DrawRuntimeStatusRow(HDC hdc, int y, const wchar_t* label,
    bool ready)
{
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT text = { 55, y, 360, y + 24 };
    DrawTextW(hdc, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    const COLORREF c = ready ? RGB_COLOR(132, 204, 22) : RGB_COLOR(239, 68, 68);
    HBRUSH b = CreateSolidBrush(c);
    HGDIOBJ oldBrush = SelectObject(hdc, b);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(5));
    Ellipse(hdc, 345, y + 5, 357, y + 17);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(b);
}

static void DrawMiscPage(HDC hdc)
{
    DrawRoundedCard(hdc, 20, 50, 740, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
    DrawFeatureTitle(hdc, L"Misc & Diagnostics", L"Runtime health and local troubleshooting information.");
    HFONT body = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, body);
    SetBkMode(hdc, TRANSPARENT);
    DrawTrainingToggleRow(hdc, 145, L"Diagnostics panel", g_safeFeatures.diagnosticsPanel);
    if (g_safeFeatures.diagnosticsPanel)
    {
        DrawRuntimeStatusRow(hdc, 205, L"Entity runtime", g_botHighlightRuntimeReady || g_preResolvedEntityRuntimeReady);
        DrawRuntimeStatusRow(hdc, 245, L"Skybox runtime", g_preResolvedSkyboxRuntimeReady);
        DrawRuntimeStatusRow(hdc, 285, L"Frame-stage hook", g_frameStageVtableSlot != nullptr && g_originalFrameStageNotify != nullptr);
        DrawRuntimeStatusRow(hdc, 325, L"Game window", g_gameWindow != nullptr);
    }
    DrawRoundedCard(hdc, 420, 205, 270, 42, RGB_COLOR(32, 32, 35), RGB_COLOR(63, 63, 70), 6);
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT reset = { 420, 205, 690, 247 };
    DrawTextW(hdc, L"Reset trainer counters", -1, &reset, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(body);
}

static void DrawFoundationPage(HDC hdc, int tab)
{
    if (tab == 0)
        DrawAimLabPage(hdc);
    else if (tab == 1)
        DrawMovementPage(hdc);
    else
        DrawMiscPage(hdc);
}
'@

Replace-Required $oldFoundation $newFoundation 'working safe foundation pages'

# Upgrade the configs page to three independent profile slots.
Replace-Required @'
    RECT pathRc = { 45, 112, 720, 138 };
    DrawTextW(hdc, L"File: cas-plus-profile.cfg (current working directory)", -1,
        &pathRc, DT_LEFT | DT_SINGLELINE);
    RECT statusRc = { 45, 145, 720, 175 };
    DrawTextW(hdc, g_configStatus, -1, &statusRc, DT_LEFT | DT_SINGLELINE);

    const wchar_t* labels[] = { L"Save profile", L"Load profile", L"Reset defaults" };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 205 + i * 60;
        DrawRoundedCard(hdc, 45, y, 300, 42, RGB_COLOR(32, 32, 35),
            RGB_COLOR(63, 63, 70), 6);
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT rc = { 45, y, 345, y + 42 };
        DrawTextW(hdc, labels[i], -1, &rc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
'@ @'
    RECT pathRc = { 45, 112, 720, 138 };
    DrawTextW(hdc, L"Three independent local profile slots", -1,
        &pathRc, DT_LEFT | DT_SINGLELINE);
    RECT statusRc = { 45, 145, 720, 175 };
    DrawTextW(hdc, g_configStatus, -1, &statusRc, DT_LEFT | DT_SINGLELINE);

    const wchar_t* slots[] = { L"Slot 1", L"Slot 2", L"Slot 3" };
    for (int s = 0; s < 3; ++s)
    {
        const int x = 45 + s * 105;
        DrawRoundedCard(hdc, x, 185, 90, 36,
            g_configSlot == s ? RGB_COLOR(37, 99, 235) : RGB_COLOR(32, 32, 35),
            RGB_COLOR(63, 63, 70), 6);
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT slotRc = { x, 185, x + 90, 221 };
        DrawTextW(hdc, slots[s], -1, &slotRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const wchar_t* labels[] = { L"Save profile", L"Load profile", L"Reset defaults" };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 245 + i * 60;
        DrawRoundedCard(hdc, 45, y, 300, 42, RGB_COLOR(32, 32, 35),
            RGB_COLOR(63, 63, 70), 6);
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT rc = { 45, y, 345, y + 42 };
        DrawTextW(hdc, labels[i], -1, &rc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
'@ 'multi-slot config UI'

# Config slot/action click handling. This is applied after runtime-sync changed the
# load/reset blocks, so it matches the synchronized source exactly.
Replace-Required @'
        if (g_espConfig.selectedTab == 4)
        {
            if (mouseX >= 45 && mouseX <= 345)
            {
                if (mouseY >= 205 && mouseY <= 247)
                {
                    g_configStatus = SaveCasProfile() ? L"Profile saved" : L"Save failed";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
                if (mouseY >= 265 && mouseY <= 307)
                {
                    const bool loaded = LoadCasProfile();
                    g_configStatus = loaded ? L"Profile loaded" : L"Load failed or profile is incompatible";
                    if (loaded)
                    {
                        const bool modelEffects = g_espConfig.enable &&
                            (g_espConfig.chams || g_espConfig.glow);
                        g_botHighlightEnabled = modelEffects;
                        QueueBotHighlight(modelEffects);
                    }
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
                if (mouseY >= 325 && mouseY <= 367)
                {
                    ResetCasProfile();
                    const bool modelEffects = g_espConfig.enable &&
                        (g_espConfig.chams || g_espConfig.glow);
                    g_botHighlightEnabled = modelEffects;
                    QueueBotHighlight(modelEffects);
                    g_configStatus = L"Defaults restored";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        // Non-visual foundation pages intentionally have no active controls yet.
        if (g_espConfig.selectedTab != 2)
            return 0;
'@ @'
        if (g_espConfig.selectedTab == 4)
        {
            if (mouseY >= 185 && mouseY <= 221)
            {
                for (int s = 0; s < 3; ++s)
                {
                    const int x = 45 + s * 105;
                    if (mouseX >= x && mouseX <= x + 90)
                    {
                        g_configSlot = s;
                        g_configStatus = L"Profile slot selected";
                        InvalidateRect(wnd, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (mouseX >= 45 && mouseX <= 345)
            {
                if (mouseY >= 245 && mouseY <= 287)
                {
                    g_configStatus = SaveCasProfile() ? L"Profile saved" : L"Save failed";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
                if (mouseY >= 305 && mouseY <= 347)
                {
                    const bool loaded = LoadCasProfile();
                    g_configStatus = loaded ? L"Profile loaded" : L"Load failed or profile is incompatible";
                    if (loaded)
                    {
                        const bool modelEffects = g_espConfig.enable &&
                            (g_espConfig.chams || g_espConfig.glow);
                        g_botHighlightEnabled = modelEffects;
                        QueueBotHighlight(modelEffects);
                    }
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
                if (mouseY >= 365 && mouseY <= 407)
                {
                    ResetCasProfile();
                    const bool modelEffects = g_espConfig.enable &&
                        (g_espConfig.chams || g_espConfig.glow);
                    g_botHighlightEnabled = modelEffects;
                    QueueBotHighlight(modelEffects);
                    g_configStatus = L"Defaults restored";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        if (g_espConfig.selectedTab == 0)
        {
            if (mouseX >= 295 && mouseX <= 350)
            {
                if (mouseY >= 140 && mouseY <= 175) g_safeFeatures.aimGuide = !g_safeFeatures.aimGuide;
                else if (mouseY >= 180 && mouseY <= 215) g_safeFeatures.recoilGuide = !g_safeFeatures.recoilGuide;
                else if (mouseY >= 220 && mouseY <= 255) g_safeFeatures.targetTrainer = !g_safeFeatures.targetTrainer;
                else goto aim_page_no_toggle;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
        aim_page_no_toggle:
            if (mouseX >= 45 && mouseX <= 345 && mouseY >= 285 && mouseY <= 327)
            {
                if (g_safeFeatures.aimGuideRadius <= 60) g_safeFeatures.aimGuideRadius = 90;
                else if (g_safeFeatures.aimGuideRadius <= 100) g_safeFeatures.aimGuideRadius = 120;
                else g_safeFeatures.aimGuideRadius = 60;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if (g_safeFeatures.targetTrainer && mouseX >= 390 && mouseX <= 720 &&
                mouseY >= 130 && mouseY <= 440)
            {
                const int dx = mouseX - g_safeFeatures.targetX;
                const int dy = mouseY - g_safeFeatures.targetY;
                if (dx * dx + dy * dy <= 18 * 18)
                {
                    ++g_safeFeatures.targetHits;
                    g_safeFeatures.targetX = 420 + ((g_safeFeatures.targetHits * 73) % 270);
                    g_safeFeatures.targetY = 175 + ((g_safeFeatures.targetHits * 47) % 215);
                }
                else
                {
                    ++g_safeFeatures.targetMisses;
                }
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }

        if (g_espConfig.selectedTab == 1)
        {
            if (mouseX >= 295 && mouseX <= 350)
            {
                if (mouseY >= 140 && mouseY <= 175) g_safeFeatures.jumpCue = !g_safeFeatures.jumpCue;
                else if (mouseY >= 180 && mouseY <= 215) g_safeFeatures.strafeCadence = !g_safeFeatures.strafeCadence;
                else if (mouseY >= 220 && mouseY <= 255) g_safeFeatures.landingCue = !g_safeFeatures.landingCue;
                else return 0;
                InvalidateRect(wnd, nullptr, FALSE);
            }
            return 0;
        }

        if (g_espConfig.selectedTab == 3)
        {
            if (mouseX >= 295 && mouseX <= 350 && mouseY >= 140 && mouseY <= 175)
            {
                g_safeFeatures.diagnosticsPanel = !g_safeFeatures.diagnosticsPanel;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            if (mouseX >= 420 && mouseX <= 690 && mouseY >= 205 && mouseY <= 247)
            {
                g_safeFeatures.targetHits = 0;
                g_safeFeatures.targetMisses = 0;
                g_safeFeatures.targetX = 565;
                g_safeFeatures.targetY = 255;
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }
            return 0;
        }

        if (g_espConfig.selectedTab != 2)
            return 0;
'@ 'safe page click handling'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated safe feature-suite source: $OutputPath"
