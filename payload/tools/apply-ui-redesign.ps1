param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\ui\ui_redesign.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "UI redesign module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "UI redesign anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# The module is injected after every Inventory domain and immediately before the
# Win32 menu proc. This lets it reuse the existing GDI helpers and all current
# InventoryStore/catalog operations without introducing another runtime hook.
$menuAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuCount = ([regex]::Matches($source, [regex]::Escape($menuAnchor))).Count
if ($menuCount -ne 1) {
    throw "UI redesign MenuWindowProc anchor expected once, found $menuCount."
}
$menuIndex = $source.IndexOf($menuAnchor)
$source = $source.Substring(0, $menuIndex) + $module + "`r`n`r`n" +
    $source.Substring($menuIndex)

# Scope the resize to the actual menu window. The generated payload also owns a
# second 780x500 surface, so matching just the two constants is intentionally
# rejected as ambiguous.
Replace-Required @'
    constexpr int kWidth = 780;
    constexpr int kHeight = 500;
    HWND wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"CAS v2.3 - ESP Settings & Interactive Preview",
'@ @'
    constexpr int kWidth = 980;
    constexpr int kHeight = 620;
    HWND wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClassName, L"cas+  |  control center",
'@ 'menu window dimensions/title'

Replace-Required @'
        HBRUSH bgBrush = CreateSolidBrush(RGB_COLOR(22, 22, 24));
'@ @'
        HBRUSH bgBrush = CreateSolidBrush(CAS_UI_BG);
'@ 'menu background'

# Transform the old horizontal tab strip into the permanent navigation rail.
Replace-Required @'
        HGDIOBJ oldFont = SelectObject(memDC, tabFont);
        SetBkMode(memDC, TRANSPARENT);

        for (int i = 0; i < 5; ++i)
'@ @'
        HGDIOBJ oldFont = SelectObject(memDC, tabFont);
        SetBkMode(memDC, TRANSPARENT);
        CasUiDrawSidebarChrome(memDC, h);

        for (int i = 0; i < 5; ++i)
'@ 'sidebar chrome'

Replace-Required @'
            COLORREF tabBg = active ? RGB_COLOR(39, 39, 42) : RGB_COLOR(24, 24, 27);
'@ @'
            COLORREF tabBg = active ? CAS_UI_ACCENT_SOFT : CAS_UI_SIDEBAR;
'@ 'sidebar active background'

Replace-Required @'
            DrawRoundedCard(memDC, 20 + i * 92, 12, 84, 28, tabBg, RGB_COLOR(45, 45, 50), 6);
'@ @'
            DrawRoundedCard(memDC, 14, 116 + i * 50, 130, 38,
                tabBg, active ? CAS_UI_BORDER_HI : CAS_UI_SIDEBAR, 6);
            if (active)
                CasUiDrawRect(memDC, 14, 124 + i * 50, 17, 146 + i * 50,
                    CAS_UI_ACCENT);
'@ 'sidebar tab geometry'

Replace-Required @'
            SetTextColor(memDC, active ? RGB_COLOR(255, 255, 255) : RGB_COLOR(161, 161, 170));
'@ @'
            SetTextColor(memDC, active ? CAS_UI_TEXT : CAS_UI_MUTED);
'@ 'sidebar tab text color'

Replace-Required @'
            RECT tabRc = { 20 + i * 92, 12, 20 + i * 92 + 84, 40 };
'@ @'
            RECT tabRc = { 28, 116 + i * 50, 136, 154 + i * 50 };
'@ 'sidebar tab text geometry'

# Inventory is the first fully redesigned feature page. The old editor remains
# available as a detail view inside the new shell, so all current controls keep
# working during the migration.
Replace-Required '            DrawInventoryChangerPanel(memDC);' `
    '            CasUiDrawInventoryScreen(memDC, w, h);' 'inventory screen'

# The current Visuals/Rage/AA/Configs content remains functional while it is
# migrated: render those panels inside the new content surface through a GDI
# viewport offset. This avoids duplicating their hit-testing in this first pass.
Replace-Required @'
        // Left Card: "ESP"
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@ @'
        CasUiDrawPageChrome(memDC, w, h, g_espConfig.selectedTab);
        POINT casUiOldOrigin{};
        SetViewportOrgEx(memDC, 160, 60, &casUiOldOrigin);

        // Left Card: "ESP"
        DrawRoundedCard(memDC, 20, 50, 360, 430, RGB_COLOR(24, 24, 27), RGB_COLOR(39, 39, 42), 8);
'@ 'legacy content viewport'

Replace-Required @'
        // Render RGB Color Picker Modal if open
'@ @'
        SetViewportOrgEx(memDC, casUiOldOrigin.x, casUiOldOrigin.y, nullptr);

        // Render RGB Color Picker Modal if open
'@ 'restore content viewport'

# Route every click through the new sidebar/inventory shell before the existing
# feature hit-testing. Detail-editor and legacy feature coordinates are then
# translated back into their original local coordinate systems.
Replace-Required @'
        int mouseX = static_cast<int>(lParam & 0xFFFF);
        int mouseY = static_cast<int>((lParam >> 16) & 0xFFFF);

        // RGB Color Picker Modal click handling
'@ @'
        int mouseX = static_cast<int>(lParam & 0xFFFF);
        int mouseY = static_cast<int>((lParam >> 16) & 0xFFFF);
        RECT casUiClient{};
        GetClientRect(wnd, &casUiClient);
        if (CasUiPrepareMenuClick(wnd, &mouseX, &mouseY,
            casUiClient.bottom - casUiClient.top))
            return 0;

        // RGB Color Picker Modal click handling
'@ 'new menu click router'

# The new router owns navigation; retaining the old horizontal-tab hit test
# would create invisible click targets over the content header after the layout
# changed.
Replace-Required @'
        // Check Tab bar clicks
        if (mouseY >= 12 && mouseY <= 40)
        {
            for (int i = 0; i < 5; ++i)
            {
                if (mouseX >= 20 + i * 92 && mouseX <= 20 + i * 92 + 84)
                {
                    g_espConfig.selectedTab = i;
                    InvalidateRect(wnd, nullptr, FALSE);
                    return 0;
                }
            }
        }
'@ @'
        // Navigation is handled by CasUiPrepareMenuClick.
'@ 'remove legacy tab hit test'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Applied unified cas+ UI redesign shell: $InputPath"
