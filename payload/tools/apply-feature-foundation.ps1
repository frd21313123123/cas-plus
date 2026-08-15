param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Old, [string]$New, [string]$Name) {
    if (-not $source.Contains($Old)) {
        throw "Feature-foundation anchor '$Name' was not found. Refusing to patch blindly."
    }
    $script:source = $source.Replace($Old, $New)
}

# File I/O imports. The payload intentionally avoids the CRT, so profiles use
# kernel32 directly and remain compatible with the existing manual-map build.
Replace-Required @'
__declspec(dllimport) BOOL WINAPI MessageBeep(UINT);
'@ @'
__declspec(dllimport) BOOL WINAPI MessageBeep(UINT);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI ReadFile(HANDLE, LPVOID, DWORD, DWORD*, LPVOID);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, const void*, DWORD, DWORD*, LPVOID);
'@ 'kernel32 file imports'

Replace-Required @'
#define PM_REMOVE 0x0001
'@ @'
#define PM_REMOVE 0x0001
#define GENERIC_READ  0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
'@ 'file constants'

Replace-Required @'
static ESPConfig g_espConfig{};
'@ @'
static ESPConfig g_espConfig{};

struct PersistedESPConfig {
    unsigned int magic;
    unsigned int version;
    BYTE enable;
    BYTE skeleton;
    RGBVal skeletonColor;
    BYTE historySkeleton;
    RGBVal historySkeletonColor;
    int historyTicks;
    BYTE aimHistorySkeleton;
    RGBVal aimHistorySkeletonColor;
    BYTE footsteps;
    RGBVal footstepsColor;
    BYTE glow;
    RGBVal glowColor;
    BYTE chams;
    RGBVal chamsColorOccluded;
    RGBVal chamsColorVisible;
    int chamsStyle;
    BYTE chamsEnemies;
    BYTE chamsWeapons;
    BYTE offScreen;
    RGBVal offScreenColor;
    BYTE flags[8];
};

static const unsigned int kConfigMagic = 0x50415343u; // 'CASP'
static const unsigned int kConfigVersion = 1u;
static const wchar_t kConfigPath[] = L"cas-plus-profile.cfg";
static const wchar_t* g_configStatus = L"Profile ready";
'@ 'persisted config structure'

Replace-Required @'
static void* AtomicCompareExchangePointer(void** destination, void* desired,
    void* expected)
{
'@ @'
static PersistedESPConfig BuildPersistedConfig()
{
    PersistedESPConfig out{};
    out.magic = kConfigMagic;
    out.version = kConfigVersion;
    out.enable = g_espConfig.enable ? 1 : 0;
    out.skeleton = g_espConfig.skeleton ? 1 : 0;
    out.skeletonColor = g_espConfig.skeletonColor;
    out.historySkeleton = g_espConfig.historySkeleton ? 1 : 0;
    out.historySkeletonColor = g_espConfig.historySkeletonColor;
    out.historyTicks = g_espConfig.historyTicks;
    out.aimHistorySkeleton = g_espConfig.aimHistorySkeleton ? 1 : 0;
    out.aimHistorySkeletonColor = g_espConfig.aimHistorySkeletonColor;
    out.footsteps = g_espConfig.footsteps ? 1 : 0;
    out.footstepsColor = g_espConfig.footstepsColor;
    out.glow = g_espConfig.glow ? 1 : 0;
    out.glowColor = g_espConfig.glowColor;
    out.chams = g_espConfig.chams ? 1 : 0;
    out.chamsColorOccluded = g_espConfig.chamsColorOccluded;
    out.chamsColorVisible = g_espConfig.chamsColorVisible;
    out.chamsStyle = g_espConfig.chamsStyle;
    out.chamsEnemies = g_espConfig.chamsEnemies ? 1 : 0;
    out.chamsWeapons = g_espConfig.chamsWeapons ? 1 : 0;
    out.offScreen = g_espConfig.offScreen ? 1 : 0;
    out.offScreenColor = g_espConfig.offScreenColor;
    out.flags[0] = g_espConfig.flagHK ? 1 : 0;
    out.flags[1] = g_espConfig.flagZoom ? 1 : 0;
    out.flags[2] = g_espConfig.flagBlind ? 1 : 0;
    out.flags[3] = g_espConfig.flagReload ? 1 : 0;
    out.flags[4] = g_espConfig.flagC4 ? 1 : 0;
    out.flags[5] = g_espConfig.flagVIP ? 1 : 0;
    out.flags[6] = g_espConfig.flagDefuse ? 1 : 0;
    out.flags[7] = g_espConfig.flagPin ? 1 : 0;
    return out;
}

static void ApplyPersistedConfig(const PersistedESPConfig& in)
{
    g_espConfig.enable = in.enable != 0;
    g_espConfig.skeleton = in.skeleton != 0;
    g_espConfig.skeletonColor = in.skeletonColor;
    g_espConfig.historySkeleton = in.historySkeleton != 0;
    g_espConfig.historySkeletonColor = in.historySkeletonColor;
    g_espConfig.historyTicks = in.historyTicks < 1 ? 1 : (in.historyTicks > 64 ? 64 : in.historyTicks);
    g_espConfig.aimHistorySkeleton = in.aimHistorySkeleton != 0;
    g_espConfig.aimHistorySkeletonColor = in.aimHistorySkeletonColor;
    g_espConfig.footsteps = in.footsteps != 0;
    g_espConfig.footstepsColor = in.footstepsColor;
    g_espConfig.glow = in.glow != 0;
    g_espConfig.glowColor = in.glowColor;
    g_espConfig.chams = in.chams != 0;
    g_espConfig.chamsColorOccluded = in.chamsColorOccluded;
    g_espConfig.chamsColorVisible = in.chamsColorVisible;
    g_espConfig.chamsStyle = in.chamsStyle < 0 ? 0 : (in.chamsStyle > 3 ? 3 : in.chamsStyle);
    g_espConfig.chamsEnemies = in.chamsEnemies != 0;
    g_espConfig.chamsWeapons = in.chamsWeapons != 0;
    g_espConfig.offScreen = in.offScreen != 0;
    g_espConfig.offScreenColor = in.offScreenColor;
    g_espConfig.flagHK = in.flags[0] != 0;
    g_espConfig.flagZoom = in.flags[1] != 0;
    g_espConfig.flagBlind = in.flags[2] != 0;
    g_espConfig.flagReload = in.flags[3] != 0;
    g_espConfig.flagC4 = in.flags[4] != 0;
    g_espConfig.flagVIP = in.flags[5] != 0;
    g_espConfig.flagDefuse = in.flags[6] != 0;
    g_espConfig.flagPin = in.flags[7] != 0;

    const bool modelEffects = g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow);
    g_botHighlightEnabled = modelEffects;
    QueueBotHighlight(modelEffects);
}

static bool SaveCasProfile()
{
    const PersistedESPConfig data = BuildPersistedConfig();
    HANDLE file = CreateFileW(kConfigPath, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, &data, static_cast<DWORD>(sizeof(data)),
        &written, nullptr);
    CloseHandle(file);
    return ok && written == sizeof(data);
}

static bool LoadCasProfile()
{
    HANDLE file = CreateFileW(kConfigPath, GENERIC_READ, 0, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    PersistedESPConfig data{};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &data, static_cast<DWORD>(sizeof(data)),
        &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(data) || data.magic != kConfigMagic ||
        data.version != kConfigVersion)
        return false;
    ApplyPersistedConfig(data);
    return true;
}

static void ResetCasProfile()
{
    const int tab = g_espConfig.selectedTab;
    g_espConfig = ESPConfig{};
    g_espConfig.selectedTab = tab;
    const bool modelEffects = g_espConfig.enable &&
        (g_espConfig.chams || g_espConfig.glow);
    g_botHighlightEnabled = modelEffects;
    QueueBotHighlight(modelEffects);
}

static void* AtomicCompareExchangePointer(void** destination, void* desired,
    void* expected)
{
'@ 'profile implementation'

# UI helpers are inserted immediately before the window procedure, after all
# low-level GDI card helpers used by these functions have already been defined.
Replace-Required @'
static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
'@ @'
static void DrawConfigPage(HDC hdc)
{
    DrawRoundedCard(hdc, 20, 50, 740, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
    HFONT title = CreateFontW(20, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, title);
    SetTextColor(hdc, RGB_COLOR(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT titleRc = { 45, 75, 720, 105 };
    DrawTextW(hdc, L"CAS+ profile", -1, &titleRc, DT_LEFT | DT_SINGLELINE);

    HFONT body = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, body);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
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

    SelectObject(hdc, old);
    DeleteObject(title);
    DeleteObject(body);
}

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

static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
'@ 'config page helpers'

Replace-Required @'
        // Left Card: "ESP"
'@ @'
        if (g_espConfig.selectedTab == 4)
        {
            DrawConfigPage(memDC);
        }
        else if (g_espConfig.selectedTab == 2)
        {
        // Left Card: "ESP"
'@ 'visual page opening route'

Replace-Required @'
        // Render RGB Color Picker Modal if open
'@ @'
        }
        else
        {
            DrawFoundationPage(memDC, g_espConfig.selectedTab);
        }

        // Render RGB Color Picker Modal if open
'@ 'visual page closing route'

Replace-Required @'
        // Manage modal click check
'@ @'
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
                    g_configStatus = LoadCasProfile() ? L"Profile loaded" : L"Load failed or profile is incompatible";
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
                if (mouseY >= 325 && mouseY <= 367)
                {
                    ResetCasProfile();
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

        // Manage modal click check
'@ 'config click handlers'

$outputDirectory = Split-Path -Parent $OutputPath
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Generated feature-foundation source: $OutputPath"
