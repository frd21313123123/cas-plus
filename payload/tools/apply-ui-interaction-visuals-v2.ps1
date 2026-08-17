param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$preludePath = Join-Path $PSScriptRoot '..\src\ui\ui_interaction_v3_prelude.inc'
$visualsPath = Join-Path $PSScriptRoot '..\src\ui\ui_visuals_v2.inc'
if (-not (Test-Path -LiteralPath $preludePath)) {
    throw "UI interaction prelude was not found: $preludePath"
}
if (-not (Test-Path -LiteralPath $visualsPath)) {
    throw "Visuals V2 module was not found: $visualsPath"
}
$prelude = Get-Content -LiteralPath $preludePath -Raw -Encoding UTF8
$visuals = Get-Content -LiteralPath $visualsPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "UI interaction anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

function Replace-RequiredCount([string]$Needle, [string]$Replacement,
    [string]$Name, [int]$Expected) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne $Expected) {
        throw "UI interaction anchor '$Name' expected $Expected times, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Mouse globals/helpers must be visible to ui_redesign.inc's primitive controls.
$uiMarker = '// cas+ unified menu redesign.'
$uiMarkerCount = ([regex]::Matches($source, [regex]::Escape($uiMarker))).Count
if ($uiMarkerCount -ne 1) {
    throw "UI interaction redesign marker expected once, found $uiMarkerCount."
}
$uiMarkerIndex = $source.IndexOf($uiMarker)
$source = $source.Substring(0, $uiMarkerIndex) + $prelude + "`r`n`r`n" +
    $source.Substring($uiMarkerIndex)

# The Visuals module depends on the shared cas+ drawing primitives and is
# therefore injected after redesign/editor/sticker helpers and before WndProc.
$menuAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuCount = ([regex]::Matches($source, [regex]::Escape($menuAnchor))).Count
if ($menuCount -ne 1) {
    throw "UI interaction MenuWindowProc anchor expected once, found $menuCount."
}
$menuIndex = $source.IndexOf($menuAnchor)
$source = $source.Substring(0, $menuIndex) + $visuals + "`r`n`r`n" +
    $source.Substring($menuIndex)

# Shared buttons gain hover + pressed feedback without changing any caller or
# feature mutation function.
$buttonAnchor = @'
static void CasUiDrawButton(HDC hdc, int x, int y, int w, int h,
    const wchar_t* text, bool primary = false)
{
    DrawRoundedCard(hdc, x, y, w, h,
        primary ? CAS_UI_ACCENT : CAS_UI_SURFACE_3,
        primary ? CAS_UI_ACCENT : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, text, x, y, w, h,
        primary ? RGB_COLOR(255, 255, 255) : CAS_UI_TEXT,
        13, 500, DT_CENTER);
}
'@
$buttonReplacement = @'
static void CasUiDrawButton(HDC hdc, int x, int y, int w, int h,
    const wchar_t* text, bool primary = false)
{
    const bool hover = CasUiMouseOver(x, y, w, h);
    const bool pressed = hover && g_casUiMouseDown;
    COLORREF background = primary ? CAS_UI_ACCENT : CAS_UI_SURFACE_3;
    COLORREF border = primary ? CAS_UI_ACCENT : CAS_UI_BORDER;
    if (primary)
    {
        if (pressed) background = RGB_COLOR(101, 73, 220);
        else if (hover) background = RGB_COLOR(139, 111, 255);
        border = hover ? RGB_COLOR(174, 153, 255) : CAS_UI_ACCENT;
    }
    else if (pressed)
    {
        background = CAS_UI_ACCENT_SOFT;
        border = CAS_UI_BORDER_HI;
    }
    else if (hover)
    {
        background = RGB_COLOR(35, 38, 56);
        border = CAS_UI_BORDER_HI;
    }
    DrawRoundedCard(hdc, x, y, w, h, background, border, 6);
    CasUiDrawLabel(hdc, text, x, y, w, h,
        primary ? RGB_COLOR(255, 255, 255) : CAS_UI_TEXT,
        13, hover ? 600 : 500, DT_CENTER);
}
'@
Replace-Required $buttonAnchor $buttonReplacement.TrimEnd() 'shared button hover state'

$valuePillAnchor = @'
static void CasUiDrawValuePill(HDC hdc, int x, int y, int w,
    const wchar_t* text, bool active = false)
{
    DrawRoundedCard(hdc, x, y, w, 28,
        active ? CAS_UI_ACCENT_SOFT : CAS_UI_SURFACE_3,
        active ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, text, x, y, w, 28,
        active ? CAS_UI_TEXT : CAS_UI_MUTED, 11, 550, DT_CENTER);
}
'@
$valuePillReplacement = @'
static void CasUiDrawValuePill(HDC hdc, int x, int y, int w,
    const wchar_t* text, bool active = false)
{
    const bool hover = CasUiMouseOver(x, y, w, 28);
    const bool pressed = hover && g_casUiMouseDown;
    DrawRoundedCard(hdc, x, y, w, 28,
        active ? CAS_UI_ACCENT_SOFT :
            (pressed ? CAS_UI_ACCENT_SOFT :
             (hover ? RGB_COLOR(35, 38, 56) : CAS_UI_SURFACE_3)),
        active || hover ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, text, x, y, w, 28,
        active || hover ? CAS_UI_TEXT : CAS_UI_MUTED,
        11, hover ? 600 : 550, DT_CENTER);
}
'@
Replace-Required $valuePillAnchor $valuePillReplacement.TrimEnd() 'value-pill hover state'

# Inventory/category/catalog cards share this exact clickable card primitive.
$cardAnchor = @'
    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
'@
$cardReplacement = @'
    const bool cardHover = CasUiMouseOver(x, y, CAS_UI_CARD_W, CAS_UI_CARD_H);
    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        cardHover ? CAS_UI_SURFACE_3 : CAS_UI_SURFACE_2,
        cardHover ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 7);
'@
Replace-RequiredCount $cardAnchor $cardReplacement.TrimEnd() 'clickable card hover state' 3

# Sidebar navigation uses the same hover language.
$tabColorAnchor = @'
            bool active = (g_espConfig.selectedTab == i);
            COLORREF tabBg = active ? CAS_UI_ACCENT_SOFT : CAS_UI_SIDEBAR;
'@
$tabColorReplacement = @'
            bool active = (g_espConfig.selectedTab == i);
            const bool hovered = CasUiMouseOver(14, 116 + i * 50, 130, 38);
            COLORREF tabBg = active ? CAS_UI_ACCENT_SOFT :
                (hovered ? CAS_UI_SURFACE_3 : CAS_UI_SIDEBAR);
'@
Replace-Required $tabColorAnchor $tabColorReplacement.TrimEnd() 'sidebar hover state'

$tabBorderAnchor = @'
            DrawRoundedCard(memDC, 14, 116 + i * 50, 130, 38,
                tabBg, active ? CAS_UI_BORDER_HI : CAS_UI_SIDEBAR, 6);
'@
$tabBorderReplacement = @'
            DrawRoundedCard(memDC, 14, 116 + i * 50, 130, 38,
                tabBg, (active || hovered) ? CAS_UI_BORDER_HI : CAS_UI_SIDEBAR, 6);
'@
Replace-Required $tabBorderAnchor $tabBorderReplacement.TrimEnd() 'sidebar hover border'

# Fix the compact chams-style row geometry from the module before compile.
Replace-Required '    CasUiDrawButton(hdc, 844, 346, 34, 28, L">");' `
    '    CasUiDrawButton(hdc, 798, 346, 34, 28, L">");' 'chams style right button geometry'
Replace-Required '        676, 346, 164, 28, CAS_UI_TEXT, 10, 550, DT_CENTER);' `
    '        676, 346, 118, 28, CAS_UI_TEXT, 10, 550, DT_CENTER);' 'chams style label geometry'
Replace-Required '    CasUiDrawColorSwatchV3(hdc, 884, 349,' `
    '    CasUiDrawColorSwatchV3(hdc, 876, 349,' 'chams occluded swatch geometry'
Replace-Required '    CasUiDrawColorSwatchV3(hdc, 852, 349,' `
    '    CasUiDrawColorSwatchV3(hdc, 840, 349,' 'chams visible swatch geometry'
Replace-Required '    if (CasUiPointInRect(mouseX, mouseY, 844, 346, 34, 28))' `
    '    if (CasUiPointInRect(mouseX, mouseY, 798, 346, 34, 28))' 'chams style right click geometry'
Replace-Required '    if (CasUiPointInRect(mouseX, mouseY, 884, 349, 28, 22))' `
    '    if (CasUiPointInRect(mouseX, mouseY, 876, 349, 28, 22))' 'chams occluded click geometry'
Replace-Required '    if (CasUiPointInRect(mouseX, mouseY, 852, 349, 28, 22))' `
    '    if (CasUiPointInRect(mouseX, mouseY, 840, 349, 28, 22))' 'chams visible click geometry'

# Visuals V2 paints after the translated legacy content, covering it completely,
# while existing RGB/chams modals remain above the new page.
$visualDrawAnchor = @'
        SetViewportOrgEx(memDC, casUiOldOrigin.x, casUiOldOrigin.y, nullptr);

        // Render RGB Color Picker Modal if open
'@
$visualDrawReplacement = @'
        SetViewportOrgEx(memDC, casUiOldOrigin.x, casUiOldOrigin.y, nullptr);
        if (g_espConfig.selectedTab == 2)
            CasUiDrawVisualsV2(memDC, w, h);

        // Render RGB Color Picker Modal if open
'@
Replace-Required $visualDrawAnchor $visualDrawReplacement 'Visuals V2 draw route'

# Visuals owns its raw window coordinates. Other tabs keep the translated legacy
# hit-testing until their own native page is migrated.
$legacyRouteAnchor = @'
    // Visual modals are deliberately left in window coordinates.
    if (g_espConfig.showRgbPickerModal || g_espConfig.showChamsModal ||
        g_espConfig.showManageModal)
        return false;

    // Legacy feature panels are rendered through the same viewport offset.
    *mouseX -= 160;
    *mouseY -= 60;
    return false;
'@
$legacyRouteReplacement = @'
    // Visual modals are deliberately left in window coordinates.
    if (g_espConfig.showRgbPickerModal || g_espConfig.showChamsModal ||
        g_espConfig.showManageModal)
        return false;

    if (g_espConfig.selectedTab == 2)
    {
        CasUiHandleVisualsV2Click(*mouseX, *mouseY);
        InvalidateRect(wnd, nullptr, FALSE);
        return true;
    }

    // Legacy feature panels are rendered through the same viewport offset.
    *mouseX -= 160;
    *mouseY -= 60;
    return false;
'@
Replace-Required $legacyRouteAnchor $legacyRouteReplacement 'Visuals V2 click ownership'

# Refresh cursor coordinates at paint time too, so hover clears correctly when
# the menu is moved underneath a stationary cursor.
$paintAnchor = @'
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(wnd, &ps);
        RECT rc{};
        GetClientRect(wnd, &rc);
'@
$paintReplacement = @'
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(wnd, &ps);
        RECT rc{};
        GetClientRect(wnd, &rc);
        CasUiRefreshMouse(wnd);
        CasUiBeginInteractiveFrame();
'@
Replace-Required $paintAnchor $paintReplacement.TrimEnd() 'interactive frame begin'

# Draw the selected tooltip last, after every page/modal, but before the
# backbuffer is copied to the window.
$paintTailAnchor = @'
        SelectObject(memDC, oldFont);
        DeleteObject(tabFont);
        DeleteObject(rowFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, 0x00CC0020 /* SRCCOPY */);
'@
$paintTailReplacement = @'
        CasUiDrawTooltipV3(memDC, w, h);
        SelectObject(memDC, oldFont);
        DeleteObject(tabFont);
        DeleteObject(rowFont);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, 0x00CC0020 /* SRCCOPY */);
'@
Replace-Required $paintTailAnchor $paintTailReplacement 'tooltip backbuffer overlay'

# Feed hover state from the owned menu WndProc. The existing RGB drag path is
# preserved exactly and still runs only while MK_LBUTTON is held.
$mouseMoveAnchor = '    if (msg == WM_MOUSEMOVE && (wParam & 0x0001 /* MK_LBUTTON */))'
$mouseMoveReplacement = @'
    if (msg == WM_MOUSEMOVE)
    {
        CasUiSetMouseFromMessage(lParam);
        InvalidateRect(wnd, nullptr, FALSE);
    }
    if (msg == WM_MOUSEMOVE && (wParam & 0x0001 /* MK_LBUTTON */))
'@
Replace-Required $mouseMoveAnchor $mouseMoveReplacement.TrimEnd() 'menu mouse-move tracking'

$mouseDownAnchor = @'
    if (msg == WM_LBUTTONDOWN)
    {
        int mouseX = static_cast<int>(lParam & 0xFFFF);
        int mouseY = static_cast<int>((lParam >> 16) & 0xFFFF);
'@
$mouseDownReplacement = @'
    if (msg == WM_LBUTTONUP)
    {
        CasUiSetMouseFromMessage(lParam);
        g_casUiMouseDown = false;
        InvalidateRect(wnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_LBUTTONDOWN)
    {
        CasUiSetMouseFromMessage(lParam);
        g_casUiMouseDown = true;
        int mouseX = static_cast<int>(lParam & 0xFFFF);
        int mouseY = static_cast<int>((lParam >> 16) & 0xFFFF);
'@
Replace-Required $mouseDownAnchor $mouseDownReplacement.TrimEnd() 'menu pressed-state tracking'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Applied shared hover/tooltips + native Visuals V2 page: $InputPath"
