param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

$configAnchor = 'static ESPConfig g_espConfig{};'
if (-not $source.Contains($configAnchor)) {
    throw 'ESP config anchor was not found. Refusing to patch blindly.'
}

$featureConfig = @'
static ESPConfig g_espConfig{};

struct RageConfig {
    bool enabled = false;
    bool silent = true;
    bool autoFire = false;
    bool autoStop = true;
    bool autoScope = true;
    bool penetration = true;
    bool history = true;
    bool preferBody = false;
    int hitchance = 65;
    int minimumDamage = 30;
    int fieldOfView = 8;
};

struct AntiAimConfig {
    bool enabled = false;
    bool jitter = true;
    bool freestanding = false;
    bool atTargets = false;
    bool manualDirections = false;
    bool slowWalk = false;
    int pitchMode = 1;
    int yawOffset = 180;
    int jitterRange = 30;
};

struct MiscConfig {
    bool bunnyHop = false;
    bool autoStrafe = false;
    bool quickStop = false;
    bool edgeJump = false;
    bool airDuck = false;
    bool thirdPerson = false;
    bool removeScope = false;
    bool hitSound = false;
    bool watermark = true;
    int thirdPersonDistance = 120;
};

struct ConfigProfileState {
    int selectedProfile = 0;
    bool dirty = false;
};

static RageConfig g_rageConfig{};
static AntiAimConfig g_antiAimConfig{};
static MiscConfig g_miscConfig{};
static ConfigProfileState g_profileState{};

static void ApplyFeatureProfile(int profile)
{
    g_profileState.selectedProfile = profile;
    g_profileState.dirty = false;

    if (profile == 0) // Balanced
    {
        g_rageConfig = RageConfig{};
        g_antiAimConfig = AntiAimConfig{};
        g_miscConfig = MiscConfig{};
        g_rageConfig.enabled = false;
        g_miscConfig.bunnyHop = true;
        g_miscConfig.autoStrafe = true;
    }
    else if (profile == 1) // Aggressive
    {
        g_rageConfig = RageConfig{};
        g_antiAimConfig = AntiAimConfig{};
        g_miscConfig = MiscConfig{};
        g_rageConfig.enabled = true;
        g_rageConfig.autoFire = true;
        g_rageConfig.hitchance = 72;
        g_rageConfig.minimumDamage = 45;
        g_antiAimConfig.enabled = true;
        g_antiAimConfig.freestanding = true;
        g_miscConfig.bunnyHop = true;
        g_miscConfig.autoStrafe = true;
        g_miscConfig.quickStop = true;
    }
    else if (profile == 2) // Visual / utility
    {
        g_rageConfig = RageConfig{};
        g_antiAimConfig = AntiAimConfig{};
        g_miscConfig = MiscConfig{};
        g_espConfig.enable = true;
        g_espConfig.chams = true;
        g_espConfig.glow = true;
        g_miscConfig.thirdPerson = true;
    }
    else // Safe defaults
    {
        g_rageConfig = RageConfig{};
        g_antiAimConfig = AntiAimConfig{};
        g_miscConfig = MiscConfig{};
        g_espConfig = ESPConfig{};
    }
}
'@
$source = $source.Replace($configAnchor, $featureConfig)

$procAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
if (-not $source.Contains($procAnchor)) {
    throw 'MenuWindowProc anchor was not found. Refusing to patch blindly.'
}

$helpers = @'
static void DrawFeatureToggleRow(HDC hdc, int y, const wchar_t* label,
    bool enabled)
{
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT labelRc = { 42, y, 310, y + 24 };
    DrawTextW(hdc, label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawToggleSwitch(hdc, 330, y + 2, enabled);
}

static void DrawFeatureValueRow(HDC hdc, int y, const wchar_t* label,
    int value, const wchar_t* suffix)
{
    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT labelRc = { 42, y, 260, y + 24 };
    DrawTextW(hdc, label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    wchar_t valueText[32]{};
    int pos = 0;
    int number = value;
    if (number < 0)
    {
        valueText[pos++] = L'-';
        number = -number;
    }
    wchar_t reversed[16]{};
    int digits = 0;
    do {
        reversed[digits++] = static_cast<wchar_t>(L'0' + (number % 10));
        number /= 10;
    } while (number > 0 && digits < 15);
    while (digits > 0)
        valueText[pos++] = reversed[--digits];
    if (suffix)
    {
        for (int i = 0; suffix[i] && pos < 30; ++i)
            valueText[pos++] = suffix[i];
    }
    valueText[pos] = 0;

    SetTextColor(hdc, RGB_COLOR(96, 165, 250));
    RECT valueRc = { 275, y, 360, y + 24 };
    DrawTextW(hdc, valueText, -1, &valueRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void DrawFeatureRuntimeNote(HDC hdc, const wchar_t* title,
    const wchar_t* detail)
{
    DrawRoundedCard(hdc, 400, 62, 340, 150, RGB_COLOR(24, 24, 27),
        RGB_COLOR(39, 39, 42), 8);
    SetTextColor(hdc, RGB_COLOR(244, 244, 245));
    RECT titleRc = { 420, 82, 720, 105 };
    DrawTextW(hdc, title, -1, &titleRc, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT detailRc = { 420, 116, 720, 190 };
    DrawTextW(hdc, detail, -1, &detailRc, DT_LEFT);
}

static void DrawFeatureSuiteTab(HDC hdc, int selectedTab)
{
    HFONT rowFont = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, rowFont);
    SetBkMode(hdc, TRANSPARENT);

    DrawRoundedCard(hdc, 20, 50, 360, 430, RGB_COLOR(24, 24, 27),
        RGB_COLOR(39, 39, 42), 8);

    if (selectedTab == 0)
    {
        SetTextColor(hdc, RGB_COLOR(161, 161, 170));
        RECT title = { 35, 62, 250, 82 };
        DrawTextW(hdc, L"RAGEBOT", -1, &title, DT_LEFT | DT_SINGLELINE);
        DrawFeatureToggleRow(hdc, 94, L"Enable", g_rageConfig.enabled);
        DrawFeatureToggleRow(hdc, 128, L"Silent aim", g_rageConfig.silent);
        DrawFeatureToggleRow(hdc, 162, L"Auto fire", g_rageConfig.autoFire);
        DrawFeatureToggleRow(hdc, 196, L"Auto stop", g_rageConfig.autoStop);
        DrawFeatureToggleRow(hdc, 230, L"Auto scope", g_rageConfig.autoScope);
        DrawFeatureToggleRow(hdc, 264, L"Penetration", g_rageConfig.penetration);
        DrawFeatureToggleRow(hdc, 298, L"History", g_rageConfig.history);
        DrawFeatureToggleRow(hdc, 332, L"Prefer body", g_rageConfig.preferBody);
        DrawFeatureValueRow(hdc, 372, L"Hitchance", g_rageConfig.hitchance, L"%");
        DrawFeatureValueRow(hdc, 402, L"Minimum damage", g_rageConfig.minimumDamage, nullptr);
        DrawFeatureValueRow(hdc, 432, L"FOV", g_rageConfig.fieldOfView, L" deg");
        DrawFeatureRuntimeNote(hdc, L"Command runtime",
            L"Settings are live in the shared feature state. Shot/angle mutation stays disabled until a validated CS2 usercmd bridge is available.");
    }
    else if (selectedTab == 1)
    {
        SetTextColor(hdc, RGB_COLOR(161, 161, 170));
        RECT title = { 35, 62, 250, 82 };
        DrawTextW(hdc, L"ANTI-AIM", -1, &title, DT_LEFT | DT_SINGLELINE);
        DrawFeatureToggleRow(hdc, 94, L"Enable", g_antiAimConfig.enabled);
        DrawFeatureToggleRow(hdc, 128, L"Jitter", g_antiAimConfig.jitter);
        DrawFeatureToggleRow(hdc, 162, L"Freestanding", g_antiAimConfig.freestanding);
        DrawFeatureToggleRow(hdc, 196, L"At targets", g_antiAimConfig.atTargets);
        DrawFeatureToggleRow(hdc, 230, L"Manual directions", g_antiAimConfig.manualDirections);
        DrawFeatureToggleRow(hdc, 264, L"Slow walk", g_antiAimConfig.slowWalk);
        DrawFeatureValueRow(hdc, 314, L"Pitch mode", g_antiAimConfig.pitchMode, nullptr);
        DrawFeatureValueRow(hdc, 348, L"Yaw offset", g_antiAimConfig.yawOffset, L" deg");
        DrawFeatureValueRow(hdc, 382, L"Jitter range", g_antiAimConfig.jitterRange, L" deg");
        DrawFeatureRuntimeNote(hdc, L"Validated-state design",
            L"Anti-aim state is separated from rendering and will only mutate commands after the command ABI is signature-validated.");
    }
    else if (selectedTab == 3)
    {
        SetTextColor(hdc, RGB_COLOR(161, 161, 170));
        RECT title = { 35, 62, 250, 82 };
        DrawTextW(hdc, L"MOVEMENT / MISC", -1, &title, DT_LEFT | DT_SINGLELINE);
        DrawFeatureToggleRow(hdc, 94, L"Bunny hop", g_miscConfig.bunnyHop);
        DrawFeatureToggleRow(hdc, 128, L"Auto strafe", g_miscConfig.autoStrafe);
        DrawFeatureToggleRow(hdc, 162, L"Quick stop", g_miscConfig.quickStop);
        DrawFeatureToggleRow(hdc, 196, L"Edge jump", g_miscConfig.edgeJump);
        DrawFeatureToggleRow(hdc, 230, L"Air duck", g_miscConfig.airDuck);
        DrawFeatureToggleRow(hdc, 264, L"Third person", g_miscConfig.thirdPerson);
        DrawFeatureToggleRow(hdc, 298, L"Remove scope", g_miscConfig.removeScope);
        DrawFeatureToggleRow(hdc, 332, L"Hit sound", g_miscConfig.hitSound);
        DrawFeatureToggleRow(hdc, 366, L"Watermark", g_miscConfig.watermark);
        DrawFeatureValueRow(hdc, 410, L"Third-person distance", g_miscConfig.thirdPersonDistance, nullptr);
        DrawFeatureRuntimeNote(hdc, L"Utility layer",
            L"The menu/config layer is active now; movement command writes remain gated behind the same validated usercmd bridge as Ragebot.");
    }
    else if (selectedTab == 4)
    {
        SetTextColor(hdc, RGB_COLOR(161, 161, 170));
        RECT title = { 35, 62, 250, 82 };
        DrawTextW(hdc, L"CONFIG PROFILES", -1, &title, DT_LEFT | DT_SINGLELINE);
        const wchar_t* names[] = { L"Balanced", L"Aggressive", L"Visual / utility", L"Safe defaults" };
        for (int i = 0; i < 4; ++i)
        {
            const int y = 105 + i * 64;
            const bool active = g_profileState.selectedProfile == i;
            DrawRoundedCard(hdc, 38, y, 320, 46,
                active ? RGB_COLOR(39, 39, 42) : RGB_COLOR(28, 28, 31),
                active ? RGB_COLOR(59, 130, 246) : RGB_COLOR(45, 45, 50), 6);
            SetTextColor(hdc, active ? RGB_COLOR(255, 255, 255) : RGB_COLOR(212, 212, 216));
            RECT rc = { 54, y, 340, y + 46 };
            DrawTextW(hdc, names[i], -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        DrawFeatureRuntimeNote(hdc, L"Profiles",
            L"Profiles update Rage, Anti-Aim, Visual and Misc settings together. File-backed import/export is intentionally isolated for a later small module.");
    }

    SelectObject(hdc, oldFont);
    DeleteObject(rowFont);
}

static bool HandleFeatureSuiteClick(HWND wnd, int mouseX, int mouseY)
{
    bool changed = false;
    if (g_espConfig.selectedTab == 0 && mouseX >= 320 && mouseX <= 372)
    {
        if (mouseY >= 94 && mouseY <= 120) { g_rageConfig.enabled = !g_rageConfig.enabled; changed = true; }
        else if (mouseY >= 128 && mouseY <= 154) { g_rageConfig.silent = !g_rageConfig.silent; changed = true; }
        else if (mouseY >= 162 && mouseY <= 188) { g_rageConfig.autoFire = !g_rageConfig.autoFire; changed = true; }
        else if (mouseY >= 196 && mouseY <= 222) { g_rageConfig.autoStop = !g_rageConfig.autoStop; changed = true; }
        else if (mouseY >= 230 && mouseY <= 256) { g_rageConfig.autoScope = !g_rageConfig.autoScope; changed = true; }
        else if (mouseY >= 264 && mouseY <= 290) { g_rageConfig.penetration = !g_rageConfig.penetration; changed = true; }
        else if (mouseY >= 298 && mouseY <= 324) { g_rageConfig.history = !g_rageConfig.history; changed = true; }
        else if (mouseY >= 332 && mouseY <= 358) { g_rageConfig.preferBody = !g_rageConfig.preferBody; changed = true; }
    }
    else if (g_espConfig.selectedTab == 1 && mouseX >= 320 && mouseX <= 372)
    {
        if (mouseY >= 94 && mouseY <= 120) { g_antiAimConfig.enabled = !g_antiAimConfig.enabled; changed = true; }
        else if (mouseY >= 128 && mouseY <= 154) { g_antiAimConfig.jitter = !g_antiAimConfig.jitter; changed = true; }
        else if (mouseY >= 162 && mouseY <= 188) { g_antiAimConfig.freestanding = !g_antiAimConfig.freestanding; changed = true; }
        else if (mouseY >= 196 && mouseY <= 222) { g_antiAimConfig.atTargets = !g_antiAimConfig.atTargets; changed = true; }
        else if (mouseY >= 230 && mouseY <= 256) { g_antiAimConfig.manualDirections = !g_antiAimConfig.manualDirections; changed = true; }
        else if (mouseY >= 264 && mouseY <= 290) { g_antiAimConfig.slowWalk = !g_antiAimConfig.slowWalk; changed = true; }
    }
    else if (g_espConfig.selectedTab == 3 && mouseX >= 320 && mouseX <= 372)
    {
        if (mouseY >= 94 && mouseY <= 120) { g_miscConfig.bunnyHop = !g_miscConfig.bunnyHop; changed = true; }
        else if (mouseY >= 128 && mouseY <= 154) { g_miscConfig.autoStrafe = !g_miscConfig.autoStrafe; changed = true; }
        else if (mouseY >= 162 && mouseY <= 188) { g_miscConfig.quickStop = !g_miscConfig.quickStop; changed = true; }
        else if (mouseY >= 196 && mouseY <= 222) { g_miscConfig.edgeJump = !g_miscConfig.edgeJump; changed = true; }
        else if (mouseY >= 230 && mouseY <= 256) { g_miscConfig.airDuck = !g_miscConfig.airDuck; changed = true; }
        else if (mouseY >= 264 && mouseY <= 290) { g_miscConfig.thirdPerson = !g_miscConfig.thirdPerson; changed = true; }
        else if (mouseY >= 298 && mouseY <= 324) { g_miscConfig.removeScope = !g_miscConfig.removeScope; changed = true; }
        else if (mouseY >= 332 && mouseY <= 358) { g_miscConfig.hitSound = !g_miscConfig.hitSound; changed = true; }
        else if (mouseY >= 366 && mouseY <= 392) { g_miscConfig.watermark = !g_miscConfig.watermark; changed = true; }
    }
    else if (g_espConfig.selectedTab == 4 && mouseX >= 38 && mouseX <= 358)
    {
        for (int i = 0; i < 4; ++i)
        {
            const int y = 105 + i * 64;
            if (mouseY >= y && mouseY <= y + 46)
            {
                ApplyFeatureProfile(i);
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        g_profileState.dirty = true;
        InvalidateRect(wnd, nullptr, FALSE);
    }
    return changed;
}

'@
$source = $source.Replace($procAnchor, $helpers + $procAnchor)

$paintAnchor = @'
        // Left Card: "ESP"
'@
if (-not $source.Contains($paintAnchor)) {
    throw 'Visual paint anchor was not found. Refusing to patch blindly.'
}

$featurePaint = @'
        if (g_espConfig.selectedTab != 2)
        {
            DrawFeatureSuiteTab(memDC, g_espConfig.selectedTab);
            SelectObject(memDC, oldFont);
            DeleteObject(tabFont);
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, 0x00CC0020 /* SRCCOPY */);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(wnd, &ps);
            return 0;
        }

        // Left Card: "ESP"
'@
$source = $source.Replace($paintAnchor, $featurePaint)

$clickAnchor = @'
        // Manage modal click check
'@
if (-not $source.Contains($clickAnchor)) {
    throw 'Click handler anchor was not found. Refusing to patch blindly.'
}

$featureClick = @'
        if (g_espConfig.selectedTab != 2 &&
            HandleFeatureSuiteClick(wnd, mouseX, mouseY))
            return 0;

        // Non-visual tabs do not fall through into ESP hitboxes.
        if (g_espConfig.selectedTab != 2)
            return DefWindowProcW(wnd, msg, wParam, lParam);

        // Manage modal click check
'@
$source = $source.Replace($clickAnchor, $featureClick)

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated feature-suite source: $OutputPath"
