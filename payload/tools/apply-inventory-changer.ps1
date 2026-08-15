param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$runtimePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_changer.inc'
if (-not (Test-Path -LiteralPath $runtimePath)) {
    throw "Inventory changer module was not found: $runtimePath"
}
$runtime = Get-Content -LiteralPath $runtimePath -Raw -Encoding UTF8

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'FrameStageNotifyHook anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $runtime + "`r`n`r`n" + $source.Substring($hookIndex)

$frameAnchor = @'
    g_originalFrameStageNotify(client, stage);
    if (stage == FRAME_RENDER_PASS)
    {
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
    g_originalFrameStageNotify(client, stage);
    if (stage == FRAME_RENDER_PASS)
    {
        UpdateInventoryChanger();
        const LONG botRequest = AtomicExchange(
'@
if (-not $source.Contains($frameAnchor)) {
    throw 'Render-frame inventory update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($frameAnchor, $frameReplacement)

$tabsAnchor = 'const wchar_t* tabs[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Misc", L"Configs" };'
$tabsReplacement = 'const wchar_t* tabs[] = { L"Ragebot", L"Anti-Aim", L"Visuals", L"Inventory", L"Configs" };'
if (-not $source.Contains($tabsAnchor)) {
    throw 'Menu tab anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($tabsAnchor, $tabsReplacement)

$ui = @'
static void DrawInventoryUnsigned(HDC hdc, unsigned int value, RECT* rc)
{
    WideStatusBuilder text;
    text.length = 0;
    text.text[0] = 0;
    AppendStatusUnsigned(&text, value);
    DrawTextW(hdc, text.text, -1, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawInventoryButton(HDC hdc, int x, int y, int w,
    const wchar_t* label, bool active)
{
    DrawRoundedCard(hdc, x, y, w, 30,
        active ? RGB_COLOR(37, 99, 235) : RGB_COLOR(39, 39, 45),
        RGB_COLOR(60, 60, 70), 6);
    SetTextColor(hdc, active ? RGB_COLOR(255, 255, 255) : RGB_COLOR(212, 212, 216));
    RECT rc = { x, y, x + w, y + 30 };
    DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void DrawInventoryChangerPanel(HDC hdc)
{
    DrawRoundedCard(hdc, 20, 50, 735, 430,
        RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);

    HFONT titleFont = CreateFontW(16, 0, 0, 0, 600, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, titleFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB_COLOR(255, 255, 255));
    RECT title = { 35, 66, 330, 90 };
    DrawTextW(hdc, L"INVENTORY CHANGER", -1, &title, DT_LEFT | DT_SINGLELINE);

    HFONT rowFont = CreateFontW(14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, rowFont);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT help = { 35, 92, 720, 116 };
    DrawTextW(hdc, L"Schema-backed cosmetic overrides; reapplied automatically when the active weapon changes.",
        -1, &help, DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(228, 228, 231));
    RECT enabledLabel = { 35, 132, 260, 154 };
    DrawTextW(hdc, L"Enable cosmetic override", -1, &enabledLabel, DT_LEFT | DT_SINGLELINE);
    DrawToggleSwitch(hdc, 680, 130, g_inventoryChanger.enabled);

    RECT paintLabel = { 35, 184, 180, 206 };
    DrawTextW(hdc, L"Paint kit ID", -1, &paintLabel, DT_LEFT | DT_SINGLELINE);
    DrawRoundedCard(hdc, 190, 176, 90, 32, RGB_COLOR(30, 30, 34), RGB_COLOR(60, 60, 70), 5);
    RECT paintValue = { 190, 176, 280, 208 };
    DrawInventoryUnsigned(hdc, static_cast<unsigned int>(g_inventoryChanger.paintKit < 0 ? 0 : g_inventoryChanger.paintKit), &paintValue);
    DrawInventoryButton(hdc, 300, 177, 70, L"-100", false);
    DrawInventoryButton(hdc, 380, 177, 70, L"-1", false);
    DrawInventoryButton(hdc, 460, 177, 70, L"+1", false);
    DrawInventoryButton(hdc, 540, 177, 70, L"+100", false);

    RECT seedLabel = { 35, 238, 180, 260 };
    DrawTextW(hdc, L"Seed", -1, &seedLabel, DT_LEFT | DT_SINGLELINE);
    DrawRoundedCard(hdc, 190, 230, 90, 32, RGB_COLOR(30, 30, 34), RGB_COLOR(60, 60, 70), 5);
    RECT seedValue = { 190, 230, 280, 262 };
    DrawInventoryUnsigned(hdc, static_cast<unsigned int>(g_inventoryChanger.seed < 0 ? 0 : g_inventoryChanger.seed), &seedValue);
    DrawInventoryButton(hdc, 300, 231, 70, L"-10", false);
    DrawInventoryButton(hdc, 380, 231, 70, L"-1", false);
    DrawInventoryButton(hdc, 460, 231, 70, L"+1", false);
    DrawInventoryButton(hdc, 540, 231, 70, L"+10", false);

    RECT wearLabel = { 35, 292, 180, 314 };
    DrawTextW(hdc, L"Wear preset", -1, &wearLabel, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 190, 285, 92, L"Factory New", g_inventoryChanger.wear < 0.07f);
    DrawInventoryButton(hdc, 292, 285, 92, L"Minimal", g_inventoryChanger.wear >= 0.07f && g_inventoryChanger.wear < 0.15f);
    DrawInventoryButton(hdc, 394, 285, 92, L"Field-Tested", g_inventoryChanger.wear >= 0.15f && g_inventoryChanger.wear < 0.38f);
    DrawInventoryButton(hdc, 496, 285, 92, L"Well-Worn", g_inventoryChanger.wear >= 0.38f && g_inventoryChanger.wear < 0.45f);
    DrawInventoryButton(hdc, 598, 285, 92, L"Battle", g_inventoryChanger.wear >= 0.45f);

    RECT statLabel = { 35, 348, 260, 370 };
    DrawTextW(hdc, L"StatTrak", -1, &statLabel, DT_LEFT | DT_SINGLELINE);
    DrawToggleSwitch(hdc, 680, 344, g_inventoryChanger.statTrak >= 0);

    DrawInventoryButton(hdc, 35, 405, 190, L"Reset cosmetic values", false);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT note = { 245, 410, 720, 442 };
    DrawTextW(hdc, L"Client-side only. Original values are restored when the changer is disabled.",
        -1, &note, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(rowFont);
}

'@
$menuProcAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuProcIndex = $source.IndexOf($menuProcAnchor)
if ($menuProcIndex -lt 0) {
    throw 'MenuWindowProc anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $menuProcIndex) + $ui + $source.Substring($menuProcIndex)

$paintAnchor = @'
        // Left Card: "ESP"
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@
$paintReplacement = @'
        if (g_espConfig.selectedTab == 3)
        {
            DrawInventoryChangerPanel(memDC);
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
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@
if (-not $source.Contains($paintAnchor)) {
    throw 'Inventory paint anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintAnchor, $paintReplacement)

$clickAnchor = @'
        // Manage modal click check
        if (g_espConfig.showManageModal)
'@
$clickReplacement = @'
        if (g_espConfig.selectedTab == 3)
        {
            if (mouseX >= 675 && mouseX <= 725 && mouseY >= 124 && mouseY <= 158)
                g_inventoryChanger.enabled = !g_inventoryChanger.enabled;
            else if (mouseY >= 172 && mouseY <= 212)
            {
                if (mouseX >= 295 && mouseX <= 375) g_inventoryChanger.paintKit -= 100;
                else if (mouseX >= 375 && mouseX <= 455) g_inventoryChanger.paintKit -= 1;
                else if (mouseX >= 455 && mouseX <= 535) g_inventoryChanger.paintKit += 1;
                else if (mouseX >= 535 && mouseX <= 615) g_inventoryChanger.paintKit += 100;
                if (g_inventoryChanger.paintKit < 0) g_inventoryChanger.paintKit = 0;
                if (g_inventoryChanger.paintKit > 100000) g_inventoryChanger.paintKit = 100000;
            }
            else if (mouseY >= 226 && mouseY <= 266)
            {
                if (mouseX >= 295 && mouseX <= 375) g_inventoryChanger.seed -= 10;
                else if (mouseX >= 375 && mouseX <= 455) g_inventoryChanger.seed -= 1;
                else if (mouseX >= 455 && mouseX <= 535) g_inventoryChanger.seed += 1;
                else if (mouseX >= 535 && mouseX <= 615) g_inventoryChanger.seed += 10;
                if (g_inventoryChanger.seed < 0) g_inventoryChanger.seed = 0;
                if (g_inventoryChanger.seed > 1000) g_inventoryChanger.seed = 1000;
            }
            else if (mouseY >= 280 && mouseY <= 320)
            {
                if (mouseX >= 185 && mouseX <= 287) g_inventoryChanger.wear = 0.0001f;
                else if (mouseX >= 287 && mouseX <= 389) g_inventoryChanger.wear = 0.08f;
                else if (mouseX >= 389 && mouseX <= 491) g_inventoryChanger.wear = 0.20f;
                else if (mouseX >= 491 && mouseX <= 593) g_inventoryChanger.wear = 0.40f;
                else if (mouseX >= 593 && mouseX <= 695) g_inventoryChanger.wear = 0.60f;
            }
            else if (mouseX >= 675 && mouseX <= 725 && mouseY >= 338 && mouseY <= 374)
                g_inventoryChanger.statTrak = g_inventoryChanger.statTrak >= 0 ? -1 : 0;
            else if (mouseX >= 30 && mouseX <= 230 && mouseY >= 400 && mouseY <= 440)
            {
                g_inventoryChanger.paintKit = 0;
                g_inventoryChanger.seed = 0;
                g_inventoryChanger.statTrak = -1;
                g_inventoryChanger.wear = 0.0001f;
            }
            else
                return 0;

            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }

        // Manage modal click check
        if (g_espConfig.showManageModal)
'@
if (-not $source.Contains($clickAnchor)) {
    throw 'Inventory click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($clickAnchor, $clickReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
