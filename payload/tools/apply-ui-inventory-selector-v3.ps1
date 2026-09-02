param([Parameter(Mandatory = $true)][string]$InputPath)

$ErrorActionPreference = 'Stop'
$script:source = (Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8).Replace("`r`n", "`n")
$module = (Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\ui\ui_inventory_selector_v3.inc') -Raw -Encoding UTF8).Replace("`r`n", "`n")
if ($script:source.Contains('// Verified local-catalog selector.')) { throw 'Selector stage must run exactly once, after all prior UI stages.' }

function Replace-One([string]$Needle, [string]$Replacement) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) { throw "Selector anchor expected exactly once ($count): $Needle" }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Keep the old My items/sticker/name editor, but route catalog construction and
# finishing exclusively through the verified selector. Earlier build stages
# retain all their exact source anchors; this stage owns only final routing.
foreach ($name in @('CasUiDrawInventoryScreen', 'CasUiHandleInventoryClick', 'CasUiItemDisplayName', 'CasUiItemFinishName')) {
    $pattern = '(?m)^static (?:void|bool|const wchar_t\*) ' + $name + '\([^\n]*\)\n\{'
    $matches = [regex]::Matches($script:source, $pattern)
    if ($matches.Count -ne 1) { throw "Selector definition $name expected once, found $($matches.Count)." }
    $old = $matches[0].Value
    $script:source = $script:source.Replace($old, $old.Replace($name + '(', $name + 'Legacy('))
}
$prototypes = @'
static void CasUiDrawInventoryScreen(HDC, int, int);
static bool CasUiHandleInventoryClick(int, int, int);
static const wchar_t* CasUiItemDisplayName(const VirtualInventoryItem&);
static const wchar_t* CasUiItemFinishName(const VirtualInventoryItem&);

'@
Replace-One '// cas+ unified menu redesign.' ($prototypes + "`n// cas+ unified menu redesign.")

# The no-activate menu uses its existing game-window text bridge. Forward only
# UI input, asynchronously, under an atomic visible-selector gate. Private
# messages prevent TranslateMessage from generating duplicate WM_CHAR events.
$gameProc = "static LRESULT CALLBACK InventoryGameWindowProc(HWND wnd, UINT msg,`n    WPARAM wParam, LPARAM lParam)`n{"
$bridge = @'
static volatile LONG g_casSelectorInputActive = 0;
static LRESULT CALLBACK InventoryGameWindowProc(HWND wnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    if (g_casSelectorInputActive && g_menuWindow && IsWindowVisible(g_menuWindow))
    {
        if (msg == 0x0102 || msg == 0x020a ||
            (msg == 0x0100 && wParam != 0x2d /* keep Insert menu toggle */))
        {
            const UINT forwarded = msg == 0x0100 ? 0x8500 : msg == 0x0102 ? 0x8501 : 0x8502;
            PostMessageW(g_menuWindow, forwarded, wParam, lParam);
            return 0;
        }
    }
'@
Replace-One $gameProc $bridge.TrimEnd()

$menuProc = "static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)`n{"
$route = @'
static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    const UINT selectorMessage = msg == 0x8500 ? 0x0100 : msg == 0x8501 ? 0x0102 : msg == 0x8502 ? 0x020a : msg;
    if (CasSelectorHandleInput(selectorMessage, wParam, lParam))
    {
        InvalidateRect(wnd, nullptr, FALSE);
        return 0;
    }
'@
Replace-One $menuProc ($module + "`n`n" + $route.TrimEnd())
Replace-One '        g_espConfig.selectedTab = i;' "        g_espConfig.selectedTab = i;`n        AtomicExchange(&g_casSelectorInputActive, 0);"

# Remove entry points that let the old editor invent incompatible paint IDs.
Replace-One '    CasUiDrawButton(hdc, 802, 132, 54, 30, L"<");' '    CasUiDrawButton(hdc, 742, 132, 178, 32, L"Choose finish...");'
Replace-One '    CasUiDrawButton(hdc, 864, 132, 56, 30, L">");' '    // Definition is stable while editing an existing item.'
Replace-One '    CasUiDrawButton(hdc, 742, 220, 78, 28, L"< Skin");' '    CasUiDrawButton(hdc, 742, 220, 178, 32, L"Choose finish...");'
Replace-One '    CasUiDrawButton(hdc, 826, 220, 94, 28, L"Skin >");' '    // Compatible finish selector owns paint changes.'
Replace-One '    CasUiDrawButton(hdc, 742, 254, 38, 26, L"-1");' '    // Raw paint ID decrement is not exposed.'
Replace-One '    CasUiDrawButton(hdc, 784, 254, 38, 26, L"+1");' '    // Raw paint ID increment is not exposed.'
Replace-One 'L"Paint kit selects the finish. The catalog arrows move through compatible known paint kits; +/-1 changes the raw paint-kit ID."' 'L"Choose finish opens a searchable list of verified finishes for this item. Changes are saved only with Save changes."'
Replace-One 'L"Item definition controls. For knives, gloves, agents and music kits the arrows cycle their matching catalog domain."' 'L"Choose a verified finish for this item. To use another model, add a separate item from the catalog."'

# Neutral accents are category-independent and do not imply invented rarity.
$accentPattern = '(?s)static COLORREF CasUiItemAccent\(const VirtualInventoryItem& item\)\n\{.*?\n\}'
if ([regex]::Matches($script:source, $accentPattern).Count -ne 1) { throw 'Selector accent function not found exactly once.' }
$script:source = [regex]::Replace($script:source, $accentPattern, "static COLORREF CasUiItemAccent(const VirtualInventoryItem&)`n{`n    return RGB_COLOR(179, 186, 201);`n}")
Set-Content -LiteralPath $InputPath -Value $script:source.Replace("`n", "`r`n") -Encoding UTF8
Write-Host "Applied verified inventory selector V3: $InputPath"
