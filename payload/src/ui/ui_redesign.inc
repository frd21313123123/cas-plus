// cas+ unified menu redesign.
//
// Visual direction strictly inspired by modern dark inventory tools:
// persistent vertical navigation rail with icons, 2-column Skinchanger
// (GLOBALS + PAINT KIT), real localized CS2 finishes, and deep charcoal palette.

extern "C" __declspec(dllimport) BOOL WINAPI SetViewportOrgEx(
    HDC, int, int, POINT*);

static const COLORREF CAS_UI_BG          = RGB_COLOR(17, 18, 22);   // #111216
static const COLORREF CAS_UI_SIDEBAR     = RGB_COLOR(15, 16, 20);   // #0f1014
static const COLORREF CAS_UI_SURFACE     = RGB_COLOR(22, 23, 28);   // #16171c
static const COLORREF CAS_UI_SURFACE_2   = RGB_COLOR(29, 31, 39);   // #1d1f27
static const COLORREF CAS_UI_SURFACE_3   = RGB_COLOR(36, 38, 48);   // #242630
static const COLORREF CAS_UI_BORDER      = RGB_COLOR(31, 33, 42);   // #1f212a
static const COLORREF CAS_UI_BORDER_HI   = RGB_COLOR(48, 52, 66);   // #303442
static const COLORREF CAS_UI_TEXT        = RGB_COLOR(240, 241, 245); // #f0f1f5
static const COLORREF CAS_UI_MUTED       = RGB_COLOR(133, 138, 153); // #858a99
static const COLORREF CAS_UI_MUTED_2     = RGB_COLOR(88, 93, 110);  // #585d6e
static const COLORREF CAS_UI_ACCENT      = RGB_COLOR(59, 130, 246);  // #3b82f6 (vibrant blue)
static const COLORREF CAS_UI_ACCENT_SOFT = RGB_COLOR(27, 34, 52);   // #1b2234
static const COLORREF CAS_UI_SUCCESS     = RGB_COLOR(101, 209, 126);
static const COLORREF CAS_UI_DANGER      = RGB_COLOR(239, 93, 112);

constexpr int CAS_UI_SIDEBAR_W = 154;
constexpr int CAS_UI_CONTENT_X = 174;
constexpr int CAS_UI_CONTENT_RIGHT = 946;
constexpr int CAS_UI_CARD_W = 180;
constexpr int CAS_UI_CARD_H = 112;
constexpr int CAS_UI_CARD_GAP_X = 14;
constexpr int CAS_UI_CARD_GAP_Y = 14;
constexpr int CAS_UI_GRID_Y = 132;
constexpr int CAS_UI_GRID_COLUMNS = 4;
constexpr int CAS_UI_GRID_ROWS = 3;
constexpr int CAS_UI_GRID_PAGE = CAS_UI_GRID_COLUMNS * CAS_UI_GRID_ROWS;

// Primary Skinchanger State
static int g_casUiSkinchangerSubtab = 2; // 0: Gloves, 1: Knife, 2: Weapon, 3: My Items
static int g_casUiSelectedGloveIndex = 0;
static int g_casUiSelectedKnifeIndex = 0;
static int g_casUiSelectedWeaponIndex = 0;
static int g_casUiSkinchangerScroll = 0;
static bool g_casUiSkinchangerEnable = true;
static int g_casUiSkinchangerSeed = 0;
static float g_casUiSkinchangerWear = 0.0001f;
static int g_casUiSkinchangerStatTrak = -1;
static BYTE g_casUiSkinchangerTeam = INVENTORY_TEAM_BOTH;

// Legacy / Inventory browsing state
static int g_casUiInventoryView = 0; // 0 items, 1 browse, 2 category, 3 detail, 4 skin browser
static int g_casUiBrowseCategory = 0;
static int g_casUiInventoryPage = 0;
static int g_casUiCatalogPage = 0;

// Skin browser state (view 4)
static int g_casUiSkinBrowserKind = 0;
static int g_casUiSkinBrowserSourceIndex = 0;
static unsigned short g_casUiSkinBrowserDefIndex = 0;
static int g_casUiSkinBrowserPage = 0;
static int g_casUiSkinBrowserSelected = -1;
static float g_casUiSkinPreviewWear = 0.0001f;
static int g_casUiSkinPreviewStatTrak = -1;
static BYTE g_casUiSkinPreviewTeam = INVENTORY_TEAM_BOTH;

struct CasUiInventoryCard {
    VirtualInventoryItem item;
    int absoluteIndex;
};

// 19 CS2 knife models
struct CasUiKnifeModel {
    unsigned short defIndex;
    const wchar_t* name;
};
static const CasUiKnifeModel kCasUiKnives[] = {
    { 508, L"Ð¨Ñ‚Ñ‹Ðº-Ð½Ð¾Ð¶ M9" },
    { 500, L"Ð¨Ñ‚Ñ‹Ðº-Ð½Ð¾Ð¶" },
    { 507, L"ÐšÐµÑ€Ð°Ð¼Ð±Ð¸Ñ‚" },
    { 515, L"ÐÐ¾Ð¶-Ð±Ð°Ð±Ð¾Ñ‡ÐºÐ°" },
    { 523, L"ÐÐ¾Ð¶ Â«ÐšÐ¾Ð³Ð¾Ñ‚ÑŒÂ»" },
    { 525, L"Ð¡ÐºÐµÐ»ÐµÑ‚Ð½Ñ‹Ð¹ Ð½Ð¾Ð¶" },
    { 522, L"Ð¡Ñ‚Ð¸Ð»ÐµÑ‚" },
    { 526, L"ÐšÑƒÐºÑ€Ð¸" },
    { 505, L"Ð¡ÐºÐ»Ð°Ð´Ð½Ð¾Ð¹ Ð½Ð¾Ð¶" },
    { 506, L"ÐÐ¾Ð¶ Ñ Ð»ÐµÐ·Ð²Ð¸ÐµÐ¼-ÐºÑ€ÑŽÐºÐ¾Ð¼" },
    { 509, L"ÐžÑ…Ð¾Ñ‚Ð½Ð¸Ñ‡Ð¸Ð¹ Ð½Ð¾Ð¶" },
    { 512, L"ÐÐ¾Ð¶-Ñ„Ð°Ð»ÑŒÑˆÐ¸Ð¾Ð½" },
    { 514, L"ÐÐ¾Ð¶ Ð‘Ð¾ÑƒÐ¸" },
    { 516, L"Ð¢Ñ‹Ñ‡ÐºÐ¾Ð²Ñ‹Ðµ Ð½Ð¾Ð¶Ð¸" },
    { 517, L"ÐŸÐ°Ñ€Ð°ÐºÐ¾Ñ€Ð´-Ð½Ð¾Ð¶" },
    { 518, L"ÐÐ¾Ð¶ Ð²Ñ‹Ð¶Ð¸Ð²Ð°Ð½Ð¸Ñ" },
    { 519, L"ÐÐ¾Ð¶ Â«ÐœÐµÐ´Ð²ÐµÐ´ÑŒÂ»" },
    { 520, L"ÐÐ¾Ð¶ Ð½Ð°Ð²Ð°Ñ…Ð°" },
    { 521, L"ÐÐ¾Ð¶ Â«Ð‘Ñ€Ð¾Ð´ÑÐ³Ð°Â»" },
    { 503, L"ÐšÐ»Ð°ÑÑÐ¸Ñ‡ÐµÑÐºÐ¸Ð¹ Ð½Ð¾Ð¶" }
};
constexpr int kCasUiKnifeCount = static_cast<int>(sizeof(kCasUiKnives) / sizeof(kCasUiKnives[0]));

// 8 Glove models
struct CasUiGloveModel {
    unsigned short defIndex;
    const wchar_t* name;
};
static const CasUiGloveModel kCasUiGloves[] = {
    { 5030, L"Ð’Ð¾Ð´Ð¸Ñ‚ÐµÐ»ÑŒÑÐºÐ¸Ðµ Ð¿ÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸" },
    { 5031, L"Ð¡Ð¿Ð¾Ñ€Ñ‚Ð¸Ð²Ð½Ñ‹Ðµ Ð¿ÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸" },
    { 5032, L"ÐŸÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸ ÑÐ¿ÐµÑ†Ð½Ð°Ð·Ð°" },
    { 5033, L"ÐœÐ¾Ñ‚Ð¾Ñ†Ð¸ÐºÐ»ÐµÑ‚Ð½Ñ‹Ðµ Ð¿ÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸" },
    { 5027, L"ÐŸÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸ Â«Ð‘Ð»Ð°Ð´Ñ…Ð°ÑƒÐ½Ð´Â»" },
    { 5034, L"ÐŸÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸ Â«Ð“Ð¸Ð´Ñ€Ð°Â»" },
    { 5035, L"ÐŸÐµÑ€Ñ‡Ð°Ñ‚ÐºÐ¸ Â«Ð¡Ð»Ð¾Ð¼Ð°Ð½Ð½Ñ‹Ð¹ ÐºÐ»Ñ‹ÐºÂ»" },
    { 5036, L"ÐžÐ±Ð¼Ð¾Ñ‚ÐºÐ¸ Ñ€ÑƒÐº" }
};
constexpr int kCasUiGloveCount = static_cast<int>(sizeof(kCasUiGloves) / sizeof(kCasUiGloves[0]));

// 35 CS2 Weapons
struct CasUiWeaponModel {
    unsigned short defIndex;
    const wchar_t* name;
};
static const CasUiWeaponModel kCasUiWeapons[] = {
    { 40, L"SSG 08" },
    { 7,  L"AK-47" },
    { 16, L"M4A4" },
    { 60, L"M4A1-S" },
    { 9,  L"AWP" },
    { 1,  L"Desert Eagle" },
    { 61, L"USP-S" },
    { 4,  L"Glock-18" },
    { 36, L"P250" },
    { 3,  L"Five-SeveN" },
    { 30, L"Tec-9" },
    { 63, L"CZ75-Auto" },
    { 2,  L"Dual Berettas" },
    { 64, L"R8 Revolver" },
    { 32, L"P2000" },
    { 34, L"MP9" },
    { 17, L"MAC-10" },
    { 19, L"P90" },
    { 23, L"MP5-SD" },
    { 33, L"MP7" },
    { 24, L"UMP-45" },
    { 26, L"PP-Bizon" },
    { 13, L"Galil AR" },
    { 10, L"FAMAS" },
    { 8,  L"AUG" },
    { 39, L"SG 553" },
    { 38, L"SCAR-20" },
    { 11, L"G3SG1" },
    { 35, L"Nova" },
    { 25, L"XM1014" },
    { 27, L"MAG-7" },
    { 29, L"Sawed-Off" },
    { 28, L"Negev" },
    { 14, L"M249" },
    { 31, L"Zeus x27" }
};
constexpr int kCasUiWeaponCount = static_cast<int>(sizeof(kCasUiWeapons) / sizeof(kCasUiWeapons[0]));

static HFONT CasUiCreateFont(int height, int weight)
{
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void CasUiDrawRect(HDC hdc, int left, int top, int right, int bottom,
    COLORREF color)
{
    RECT rc = { left, top, right, bottom };
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

static bool CasUiPointInRect(int x, int y, int left, int top, int width, int height)
{
    return x >= left && x < left + width && y >= top && y < top + height;
}

static void CasUiAsciiToWide(const char* input, wchar_t* output, int capacity)
{
    if (!output || capacity <= 0)
        return;
    int i = 0;
    if (input)
    {
        while (i + 1 < capacity && input[i])
        {
            const unsigned char c = static_cast<unsigned char>(input[i]);
            output[i] = c >= 0x20 && c <= 0x7E ?
                static_cast<wchar_t>(c) : L'?';
            ++i;
        }
    }
    output[i] = 0;
}

static void CasUiDrawLabel(HDC hdc, const wchar_t* text,
    int x, int y, int w, int h, COLORREF color,
    int fontSize = 13, int weight = 400, UINT align = DT_LEFT)
{
    HFONT font = CasUiCreateFont(fontSize, weight);
    HGDIOBJ old = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT rc = { x, y, x + w, y + h };
    DrawTextW(hdc, text ? text : L"", -1, &rc,
        align | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);
}

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

static void CasUiDrawBadge(HDC hdc, int x, int y, int w,
    const wchar_t* text, COLORREF fg, COLORREF bg)
{
    DrawRoundedCard(hdc, x, y, w, 20, bg, bg, 5);
    CasUiDrawLabel(hdc, text, x, y, w, 20, fg, 11, 600, DT_CENTER);
}

static const wchar_t* CasUiWearLabel(float wear)
{
    if (wear < 0.07f) return L"Factory New";
    if (wear < 0.15f) return L"Minimal Wear";
    if (wear < 0.38f) return L"Field-Tested";
    if (wear < 0.45f) return L"Well-Worn";
    return L"Battle-Scarred";
}

static void CasUiDrawValuePill(HDC hdc, int x, int y, int w,
    const wchar_t* text, bool active = false)
{
    DrawRoundedCard(hdc, x, y, w, 28,
        active ? CAS_UI_ACCENT_SOFT : CAS_UI_SURFACE_3,
        active ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, text, x, y, w, 28,
        active ? CAS_UI_TEXT : CAS_UI_MUTED, 11, 550, DT_CENTER);
}

static void CasUiDrawEditorSection(HDC hdc, int x, int y, int w, int h,
    const wchar_t* title)
{
    DrawRoundedCard(hdc, x, y, w, h,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, title, x + 16, y + 10, w - 32, 18,
        CAS_UI_MUTED_2, 10, 650, DT_LEFT);
}

static void CasUiDrawToggleSwitch(HDC hdc, int x, int y, bool active)
{
    COLORREF bg = active ? CAS_UI_ACCENT : RGB_COLOR(38, 41, 52);
    HPEN pen = CreatePen(PS_SOLID, 1, bg);
    HBRUSH brush = CreateSolidBrush(bg);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    RoundRect(hdc, x, y, x + 38, y + 20, 18, 18);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    int knobX = active ? (x + 20) : (x + 2);
    HBRUSH knobBrush = CreateSolidBrush(RGB_COLOR(255, 255, 255));
    HPEN knobPen = CreatePen(PS_SOLID, 1, RGB_COLOR(255, 255, 255));
    oldPen = SelectObject(hdc, knobPen);
    oldBrush = SelectObject(hdc, knobBrush);
    Ellipse(hdc, knobX, y + 2, knobX + 16, y + 18);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(knobPen);
    DeleteObject(knobBrush);
}

static void CasUiDrawSlider(HDC hdc, int x, int y, int w, float value, float minVal, float maxVal)
{
    float norm = (value - minVal) / (maxVal - minVal);
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    CasUiDrawRect(hdc, x, y + 2, x + w, y + 6, RGB_COLOR(38, 41, 52));
    int filledW = static_cast<int>(norm * w);
    if (filledW > 0)
        CasUiDrawRect(hdc, x, y + 2, x + filledW, y + 6, CAS_UI_ACCENT);

    int thumbX = x + filledW;
    HBRUSH thumbBrush = CreateSolidBrush(CAS_UI_ACCENT);
    HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB_COLOR(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(hdc, thumbPen);
    HGDIOBJ oldBrush = SelectObject(hdc, thumbBrush);
    Ellipse(hdc, thumbX - 6, y - 2, thumbX + 6, y + 10);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(thumbPen);
    DeleteObject(thumbBrush);
}

static void CasUiDrawCheckmark(HDC hdc, int x, int y, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, x, y + 5, nullptr);
    LineTo(hdc, x + 4, y + 10);
    LineTo(hdc, x + 12, y + 1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void CasUiDrawChevronDown(HDC hdc, int x, int y, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, x + 4, y + 4);
    LineTo(hdc, x + 8, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static const wchar_t* CasUiTabTitle(int tab)
{
    switch (tab)
    {
    case 0: return L"Ragebot";
    case 1: return L"Anti-Aim";
    case 2: return L"Visuals";
    case 3: return L"Skinchanger";
    case 4: return L"Configs";
    default: return L"Skinchanger";
    }
}

static void CasUiDrawSidebarChrome(HDC hdc, int height)
{
    CasUiDrawRect(hdc, 0, 0, CAS_UI_SIDEBAR_W, height, CAS_UI_SIDEBAR);
    CasUiDrawRect(hdc, CAS_UI_SIDEBAR_W - 1, 0,
        CAS_UI_SIDEBAR_W, height, CAS_UI_BORDER);
    CasUiDrawRect(hdc, 0, 0, 3, height, CAS_UI_ACCENT);

    CasUiDrawLabel(hdc, L"cas+", 20, 20, 100, 26,
        CAS_UI_TEXT, 22, 700, DT_LEFT);
    CasUiDrawLabel(hdc, L"CONTROL CENTER", 20, 46, 110, 18,
        CAS_UI_MUTED_2, 10, 600, DT_LEFT);
    CasUiDrawLabel(hdc, L"FEATURES", 20, 88, 100, 18,
        CAS_UI_MUTED_2, 10, 600, DT_LEFT);

    if (height > 130)
    {
        DrawRoundedCard(hdc, 14, height - 74, 126, 54,
            CAS_UI_SURFACE, CAS_UI_BORDER, 7);
        DrawRoundedCard(hdc, 24, height - 62, 30, 30,
            CAS_UI_ACCENT_SOFT, CAS_UI_BORDER_HI, 6);
        CasUiDrawLabel(hdc, L"C+", 24, height - 62, 30, 30,
            CAS_UI_TEXT, 11, 700, DT_CENTER);
        CasUiDrawLabel(hdc, L"cas+ local", 62, height - 60, 68, 18,
            CAS_UI_TEXT, 11, 600, DT_LEFT);
        CasUiDrawLabel(hdc, L"runtime", 62, height - 43, 68, 16,
            CAS_UI_MUTED_2, 10, 400, DT_LEFT);
    }
}

static void CasUiDrawPageChrome(HDC hdc, int width, int height, int tab)
{
    const int contentW = width - CAS_UI_SIDEBAR_W - 16;
    DrawRoundedCard(hdc, CAS_UI_SIDEBAR_W + 8, 8,
        contentW, height - 16, CAS_UI_BG, CAS_UI_BORDER, 9);
    CasUiDrawLabel(hdc, CasUiTabTitle(tab), CAS_UI_CONTENT_X, 24,
        250, 28, CAS_UI_TEXT, 20, 650, DT_LEFT);
    CasUiDrawLabel(hdc, L"cas+  /", CAS_UI_CONTENT_X, 54,
        52, 18, CAS_UI_MUTED_2, 11, 500, DT_LEFT);
    CasUiDrawLabel(hdc, CasUiTabTitle(tab), CAS_UI_CONTENT_X + 54, 54,
        180, 18, CAS_UI_ACCENT, 11, 600, DT_LEFT);
}

static const wchar_t* CasUiInventoryDomainName(const VirtualInventoryItem& item)
{
    return InventoryCatalogFullDomainName(
        item.slotDefinitionIndex, item.overrideDefinitionIndex);
}

static const wchar_t* CasUiItemDisplayName(const VirtualInventoryItem& item)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return InventoryMusicName(static_cast<int>(item.overrideDefinitionIndex));
    if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        return InventoryAgentName(item.overrideDefinitionIndex);
    if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
    {
        if (item.paintKit > 0 && InventoryGameCatalogReady())
        {
            const wchar_t* gn = InventoryGameCatalogDisplayName(item);
            if (gn && gn[0]) return gn;
        }
        return InventoryGloveName(item.overrideDefinitionIndex);
    }
    if (item.paintKit > 0)
    {
        if (InventoryGameCatalogReady())
        {
            const wchar_t* gn = InventoryGameCatalogDisplayName(item);
            if (gn && gn[0]) return gn;
        }
        const wchar_t* pn = InventoryCatalogPaintName(item.paintKit, item.slotDefinitionIndex);
        if (pn && pn[0] && pn[0] != L'R')
            return pn;
    }
    return CasUiInventoryDomainName(item);
}

static const wchar_t* CasUiItemFinishName(const VirtualInventoryItem& item)
{
    if (item.paintKit <= 0)
        return L"Vanilla";
    if (InventoryGameCatalogReady())
    {
        const InventoryGameCatalogRecord* rec = InventoryGameCatalogForItem(item);
        if (rec)
        {
            if (rec->finishName[0]) return rec->finishName;
            if (rec->displayName[0]) return rec->displayName;
        }
    }
    const wchar_t* pn = InventoryCatalogPaintName(item.paintKit, item.slotDefinitionIndex);
    if (pn && pn[0] && pn[0] != L'R')
        return pn;
    return L"Custom";
}

static const wchar_t* CasUiTeamBadge(BYTE teams)
{
    if (teams == INVENTORY_TEAM_T) return L"T";
    if (teams == INVENTORY_TEAM_CT) return L"CT";
    if (teams == INVENTORY_TEAM_BOTH) return L"T + CT";
    return L"NONE";
}

static COLORREF CasUiItemAccent(const VirtualInventoryItem& item)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        return RGB_COLOR(216, 76, 154);
    if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        return RGB_COLOR(227, 157, 80);
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return RGB_COLOR(82, 176, 223);
    if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        return RGB_COLOR(198, 84, 222);
    if (item.statTrak >= 0)
        return RGB_COLOR(223, 138, 69);
    return CAS_UI_ACCENT;
}

static void CasUiDrawItemGlyph(HDC hdc, int x, int y,
    const wchar_t* name, COLORREF accent)
{
    wchar_t glyph[4] = { L'?', 0, 0, 0 };
    if (name && name[0])
    {
        glyph[0] = name[0];
        if (name[1] && name[1] != L' ')
            glyph[1] = name[1];
    }
    DrawRoundedCard(hdc, x, y, 50, 42,
        CAS_UI_ACCENT_SOFT, CAS_UI_BORDER_HI, 7);
    CasUiDrawLabel(hdc, glyph, x, y, 50, 42,
        accent, 20, 700, DT_CENTER);
}

static void CasUiDrawInventoryCard(HDC hdc, int x, int y,
    const CasUiInventoryCard& card)
{
    const VirtualInventoryItem& item = card.item;
    const wchar_t* name = CasUiItemDisplayName(item);
    const COLORREF accent = CasUiItemAccent(item);

    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
    CasUiDrawRect(hdc, x, y + CAS_UI_CARD_H - 3,
        x + CAS_UI_CARD_W, y + CAS_UI_CARD_H, accent);
    CasUiDrawItemGlyph(hdc, x + 14, y + 14, name, accent);

    CasUiDrawLabel(hdc, name, x + 14, y + 60,
        CAS_UI_CARD_W - 28, 18, CAS_UI_TEXT, 12, 600, DT_LEFT);

    wchar_t subtitle[64]{};
    if (item.customNameLength > 0)
        CasUiAsciiToWide(item.customName, subtitle,
            static_cast<int>(sizeof(subtitle) / sizeof(subtitle[0])));
    else if (item.paintKit > 0)
    {
        const wchar_t* finish = CasUiItemFinishName(item);
        int len = 0;
        while (finish && finish[len] && len < 40)
        {
            subtitle[len] = finish[len];
            ++len;
        }
        subtitle[len] = 0;
    }
    else
    {
        subtitle[0] = L'D'; subtitle[1] = L'e'; subtitle[2] = L'f';
        subtitle[3] = L'a'; subtitle[4] = L'u'; subtitle[5] = L'l';
        subtitle[6] = L't'; subtitle[7] = 0;
    }
    CasUiDrawLabel(hdc, subtitle, x + 14, y + 80,
        100, 17, CAS_UI_MUTED, 11, 400, DT_LEFT);

    if (item.statTrak >= 0)
    {
        CasUiDrawBadge(hdc, x + CAS_UI_CARD_W - 58, y + 80, 44,
            L"ST", RGB_COLOR(223, 138, 69), RGB_COLOR(48, 30, 16));
    }

    if (item.equippedTeams != INVENTORY_TEAM_NONE)
        CasUiDrawBadge(hdc, x + CAS_UI_CARD_W - 62, y + 14, 48,
            CasUiTeamBadge(item.equippedTeams), CAS_UI_SUCCESS,
            RGB_COLOR(25, 50, 37));
}

static int CasUiCopyInventoryPage(CasUiInventoryCard* cards, int capacity,
    int* totalMatches)
{
    if (!cards || capacity <= 0 || !totalMatches || !InventoryTryLock())
        return 0;
    int matching = 0;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
        if (InventorySearchMatches(g_inventoryStore.items[i]))
            ++matching;
    *totalMatches = matching;
    const int maxPage = matching > 0 ? (matching - 1) / CAS_UI_GRID_PAGE : 0;
    g_casUiInventoryPage = InventoryClampInt(g_casUiInventoryPage, 0, maxPage);
    const int first = g_casUiInventoryPage * CAS_UI_GRID_PAGE;
    int seen = 0;
    int written = 0;
    for (int i = 0; i < g_inventoryStore.itemCount && written < capacity; ++i)
    {
        if (!InventorySearchMatches(g_inventoryStore.items[i]))
            continue;
        if (seen++ < first)
            continue;
        cards[written].item = g_inventoryStore.items[i];
        cards[written].absoluteIndex = i;
        ++written;
    }
    InventoryUnlock();
    return written;
}

static void CasUiDrawSearchBox(HDC hdc)
{
    DrawRoundedCard(hdc, 610, 66, 180, 34,
        CAS_UI_SURFACE_2,
        AtomicExchange(&g_inventoryTextMode, g_inventoryTextMode) ==
            INVENTORY_TEXT_SEARCH ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    wchar_t text[40]{};
    if (g_inventorySearchLength > 0)
        CasUiAsciiToWide(g_inventorySearchText, text,
            static_cast<int>(sizeof(text) / sizeof(text[0])));
    CasUiDrawLabel(hdc,
        g_inventorySearchLength > 0 ? text : L"Search inventory...",
        624, 66, 150, 34,
        g_inventorySearchLength > 0 ? CAS_UI_TEXT : CAS_UI_MUTED_2,
        12, 400, DT_LEFT);
}

static void CasUiDrawInventoryHome(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawSearchBox(hdc);
    CasUiDrawButton(hdc, 804, 66, 132, 34, L"+  Add items", true);

    CasUiInventoryCard cards[CAS_UI_GRID_PAGE]{};
    int totalMatches = 0;
    const int count = CasUiCopyInventoryPage(cards,
        CAS_UI_GRID_PAGE, &totalMatches);

    CasUiDrawLabel(hdc, L"MY ITEMS", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);

    if (count == 0)
    {
        DrawRoundedCard(hdc, CAS_UI_CONTENT_X, 140, 762, 230,
            CAS_UI_SURFACE, CAS_UI_BORDER, 8);
        CasUiDrawLabel(hdc, L"Your virtual inventory is empty",
            CAS_UI_CONTENT_X + 28, 188, 420, 28,
            CAS_UI_TEXT, 18, 650, DT_LEFT);
        CasUiDrawLabel(hdc,
            L"Use Add items to browse weapons, knives, gloves, agents and music kits.",
            CAS_UI_CONTENT_X + 28, 222, 660, 24,
            CAS_UI_MUTED, 12, 400, DT_LEFT);
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X + 28, 270,
            140, 34, L"Browse items", true);
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const int column = i % CAS_UI_GRID_COLUMNS;
        const int row = i / CAS_UI_GRID_COLUMNS;
        const int x = CAS_UI_CONTENT_X +
            column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
        const int y = CAS_UI_GRID_Y +
            row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
        CasUiDrawInventoryCard(hdc, x, y, cards[i]);
    }

    if (totalMatches > CAS_UI_GRID_PAGE)
    {
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X, height - 58,
            78, 30, L"<  Prev");
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X + 86, height - 58,
            78, 30, L"Next  >");
    }
}

static COLORREF CasUiGetSkinRarityColor(const wchar_t* name, int paintKit)
{
    if (paintKit <= 0 || !name || !name[0])
        return RGB_COLOR(225, 227, 235); // Default / White

    int hash = paintKit % 5;
    if (hash == 0) return RGB_COLOR(235, 75, 75);  // Covert Red
    if (hash == 1) return RGB_COLOR(211, 44, 230); // Classified Pink
    if (hash == 2) return RGB_COLOR(136, 71, 255); // Restricted Purple
    if (hash == 3) return RGB_COLOR(75, 105, 255); // Mil-Spec Blue
    return RGB_COLOR(235, 75, 75);                 // Covert Red
}

static unsigned short CasUiGetCurrentDefinition(int subtab, const wchar_t** modelName)
{
    if (subtab == 0) // Gloves
    {
        if (g_casUiSelectedGloveIndex < 0 || g_casUiSelectedGloveIndex >= kCasUiGloveCount)
            g_casUiSelectedGloveIndex = 0;
        if (modelName) *modelName = kCasUiGloves[g_casUiSelectedGloveIndex].name;
        return kCasUiGloves[g_casUiSelectedGloveIndex].defIndex;
    }
    else if (subtab == 1) // Knife
    {
        if (g_casUiSelectedKnifeIndex < 0 || g_casUiSelectedKnifeIndex >= kCasUiKnifeCount)
            g_casUiSelectedKnifeIndex = 0;
        if (modelName) *modelName = kCasUiKnives[g_casUiSelectedKnifeIndex].name;
        return kCasUiKnives[g_casUiSelectedKnifeIndex].defIndex;
    }
    else // Weapon
    {
        if (g_casUiSelectedWeaponIndex < 0 || g_casUiSelectedWeaponIndex >= kCasUiWeaponCount)
            g_casUiSelectedWeaponIndex = 0;
        if (modelName) *modelName = kCasUiWeapons[g_casUiSelectedWeaponIndex].name;
        return kCasUiWeapons[g_casUiSelectedWeaponIndex].defIndex;
    }
}

static void CasUiApplySkinchangerSelection(int subtab, int modelIndex, int paintKit, int seed, float wear, int statTrak, BYTE team)
{
    if (!InventoryTryLock())
        return;

    unsigned short defIndex = 0;
    unsigned short slotDef = 0;

    if (subtab == 0) // Gloves
    {
        slotDef = INVENTORY_SLOT_GLOVE;
        if (modelIndex >= 0 && modelIndex < kCasUiGloveCount)
            defIndex = kCasUiGloves[modelIndex].defIndex;
        else
            defIndex = 5030;
    }
    else if (subtab == 1) // Knife
    {
        slotDef = INVENTORY_SLOT_KNIFE;
        if (modelIndex >= 0 && modelIndex < kCasUiKnifeCount)
            defIndex = kCasUiKnives[modelIndex].defIndex;
        else
            defIndex = 508;
    }
    else // Weapon
    {
        if (modelIndex >= 0 && modelIndex < kCasUiWeaponCount)
            defIndex = kCasUiWeapons[modelIndex].defIndex;
        else
            defIndex = 40; // SSG 08
        slotDef = defIndex;
    }

    // Look for existing item
    int existing = -1;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
    {
        if (g_inventoryStore.items[i].slotDefinitionIndex == slotDef)
        {
            existing = i;
            break;
        }
    }

    if (existing >= 0)
    {
        VirtualInventoryItem& item = g_inventoryStore.items[existing];
        item.overrideDefinitionIndex = defIndex;
        item.paintKit = paintKit;
        item.seed = seed;
        item.wear = wear;
        item.statTrak = statTrak;
        item.equippedTeams = team;
        InventoryEquipItemLocked(existing, team);
    }
    else if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        ZeroBytes(&item, sizeof(item));
        item.itemId = g_inventoryStore.nextItemId++;
        item.slotDefinitionIndex = slotDef;
        item.overrideDefinitionIndex = defIndex;
        item.paintKit = paintKit;
        item.seed = seed;
        item.wear = wear;
        item.statTrak = statTrak;
        item.equippedTeams = team;
        item.quality = (slotDef == INVENTORY_SLOT_KNIFE || slotDef == INVENTORY_SLOT_GLOVE) ? 3 : 0;
        int idx = g_inventoryStore.itemCount++;
        g_inventoryStore.items[idx] = item;
        InventoryEquipItemLocked(idx, team);
    }

    MarkInventoryDirty();
    InventoryUnlock();
}

static int CasUiSkinchangerTotalSkins(unsigned short defIndex)
{
    if (InventoryGameCatalogReady())
    {
        int count = InventoryGameCatalogCountForDefinition(defIndex);
        if (count > 0)
            return count + 1; // +1 for Default
    }
    return InventoryCatalogPaintCount();
}

static bool CasUiSkinchangerGetSkin(unsigned short defIndex, int index, const wchar_t** finishName, int* paintKit)
{
    if (!finishName || !paintKit || index < 0)
        return false;

    if (index == 0)
    {
        *finishName = L"Default";
        *paintKit = 0;
        return true;
    }

    if (InventoryGameCatalogReady())
    {
        const InventoryGameCatalogRecord* rec =
            InventoryGameCatalogOrdinalForDefinition(defIndex, index - 1);
        if (rec)
        {
            *finishName = (rec->finishName[0]) ? rec->finishName :
                (rec->displayName[0] ? rec->displayName : L"Default");
            *paintKit = rec->paintKit;
            return true;
        }
    }

    if (index < InventoryCatalogPaintCount())
    {
        *paintKit = kInventoryPaints[index].paintKit;
        *finishName = kInventoryPaints[index].displayName;
        return true;
    }
    return false;
}

// Full 2-Column Dark Skinchanger matching User Screenshots
static void CasUiDrawSkinchanger(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);

    // Title: "Skinchanger"
    CasUiDrawLabel(hdc, L"Skinchanger", 174, 24, 180, 28,
        RGB_COLOR(255, 255, 255), 18, 700, DT_LEFT);

    // Horizontal Subtabs: Gloves | Knife | Weapon | My Items
    const wchar_t* subtabs[] = { L"Gloves", L"Knife", L"Weapon", L"My Items" };
    const int subtabX[] = { 360, 434, 500, 584 };
    const int subtabW[] = { 68,  60,  78,  84 };

    for (int i = 0; i < 4; ++i)
    {
        const bool active = (g_casUiSkinchangerSubtab == i);
        if (active)
        {
            DrawRoundedCard(hdc, subtabX[i], 22, subtabW[i], 32,
                RGB_COLOR(27, 29, 37), CAS_UI_BORDER, 6);
            CasUiDrawRect(hdc, subtabX[i] + 6, 51,
                subtabX[i] + subtabW[i] - 6, 54, CAS_UI_ACCENT);
            CasUiDrawLabel(hdc, subtabs[i], subtabX[i], 22,
                subtabW[i], 29, RGB_COLOR(255, 255, 255), 13, 600, DT_CENTER);
        }
        else
        {
            CasUiDrawLabel(hdc, subtabs[i], subtabX[i], 22,
                subtabW[i], 32, CAS_UI_MUTED, 13, 500, DT_CENTER);
        }
    }

    const wchar_t* modelName = nullptr;
    const unsigned short defIndex =
        CasUiGetCurrentDefinition(g_casUiSkinchangerSubtab, &modelName);

    // Find currently active item attributes
    int activePaint = 0;
    int activeSeed = g_casUiSkinchangerSeed;
    float activeWear = g_casUiSkinchangerWear;
    int activeStatTrak = g_casUiSkinchangerStatTrak;
    BYTE activeTeam = g_casUiSkinchangerTeam;

    if (InventoryTryLock())
    {
        unsigned short targetSlot = (g_casUiSkinchangerSubtab == 0) ? INVENTORY_SLOT_GLOVE :
            ((g_casUiSkinchangerSubtab == 1) ? INVENTORY_SLOT_KNIFE : defIndex);
        for (int i = 0; i < g_inventoryStore.itemCount; ++i)
        {
            if (g_inventoryStore.items[i].slotDefinitionIndex == targetSlot)
            {
                activePaint = g_inventoryStore.items[i].paintKit;
                activeSeed = g_inventoryStore.items[i].seed;
                activeWear = g_inventoryStore.items[i].wear;
                activeStatTrak = g_inventoryStore.items[i].statTrak;
                activeTeam = g_inventoryStore.items[i].equippedTeams;
                break;
            }
        }
        InventoryUnlock();
    }

    // Left Card: "GLOBALS"
    const int leftCardX = 174;
    const int leftCardY = 66;
    const int leftCardW = 370;
    const int leftCardH = 526;
    DrawRoundedCard(hdc, leftCardX, leftCardY, leftCardW, leftCardH,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, L"GLOBALS", leftCardX + 20, leftCardY + 16,
        leftCardW - 40, 18, CAS_UI_MUTED_2, 10, 700, DT_LEFT);

    // Row 1: Enable
    const int row1Y = leftCardY + 48;
    CasUiDrawLabel(hdc, L"Enable", leftCardX + 20, row1Y,
        100, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    CasUiDrawToggleSwitch(hdc, leftCardX + leftCardW - 58, row1Y, g_casUiSkinchangerEnable);

    // Row 2: Model / Weapon
    const int row2Y = leftCardY + 98;
    CasUiDrawLabel(hdc, (g_casUiSkinchangerSubtab == 2) ? L"Weapon" : L"Model",
        leftCardX + 20, row2Y + 6, 80, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    DrawRoundedCard(hdc, leftCardX + 150, row2Y, 200, 34,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, L"<", leftCardX + 152, row2Y, 24, 34,
        CAS_UI_MUTED, 12, 600, DT_CENTER);
    CasUiDrawLabel(hdc, modelName ? modelName : L"", leftCardX + 176, row2Y,
        148, 34, CAS_UI_TEXT, 12, 500, DT_LEFT);
    CasUiDrawChevronDown(hdc, leftCardX + 328, row2Y + 14, CAS_UI_MUTED);
    CasUiDrawLabel(hdc, L">", leftCardX + 348, row2Y, 24, 34,
        CAS_UI_MUTED, 12, 600, DT_CENTER);

    // Row 3: Seed
    const int row3Y = leftCardY + 150;
    CasUiDrawLabel(hdc, L"Seed", leftCardX + 20, row3Y + 6,
        80, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    DrawRoundedCard(hdc, leftCardX + 190, row3Y, 80, 32,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 6);
    RECT seedRc = { leftCardX + 190, row3Y, leftCardX + 270, row3Y + 32 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, static_cast<unsigned int>(activeSeed), &seedRc,
        DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    CasUiDrawButton(hdc, leftCardX + 276, row3Y, 34, 32, L"-");
    CasUiDrawButton(hdc, leftCardX + 316, row3Y, 34, 32, L"+");

    // Row 4: Wear
    const int row4Y = leftCardY + 204;
    CasUiDrawLabel(hdc, L"Wear", leftCardX + 20, row4Y + 6,
        60, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    CasUiDrawSlider(hdc, leftCardX + 150, row4Y + 12, 130, activeWear, 0.0001f, 1.00f);
    wchar_t wearStr[16]{};
    int wearInt = static_cast<int>(activeWear * 100.0f + 0.5f);
    wearStr[0] = L'0'; wearStr[1] = L'.';
    wearStr[2] = static_cast<wchar_t>(L'0' + (wearInt / 10) % 10);
    wearStr[3] = static_cast<wchar_t>(L'0' + (wearInt % 10));
    wearStr[4] = 0;
    CasUiDrawLabel(hdc, wearStr, leftCardX + 292, row4Y + 6, 58, 22,
        CAS_UI_MUTED, 12, 500, DT_RIGHT);

    // Row 5: StatTrak
    const int row5Y = leftCardY + 258;
    CasUiDrawLabel(hdc, L"StatTrak", leftCardX + 20, row5Y + 4,
        80, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    CasUiDrawValuePill(hdc, leftCardX + 190, row5Y, 48, L"Off", activeStatTrak < 0);
    CasUiDrawButton(hdc, leftCardX + 244, row5Y, 34, 28, L"-");
    CasUiDrawButton(hdc, leftCardX + 282, row5Y, 34, 28, L"+");
    CasUiDrawValuePill(hdc, leftCardX + 320, row5Y, 30, L"ST", activeStatTrak >= 0);

    // Row 6: Team
    const int row6Y = leftCardY + 310;
    CasUiDrawLabel(hdc, L"Team", leftCardX + 20, row6Y + 4,
        60, 22, CAS_UI_TEXT, 13, 500, DT_LEFT);
    CasUiDrawValuePill(hdc, leftCardX + 150, row6Y, 58, L"T", activeTeam == INVENTORY_TEAM_T);
    CasUiDrawValuePill(hdc, leftCardX + 214, row6Y, 58, L"CT", activeTeam == INVENTORY_TEAM_CT);
    CasUiDrawValuePill(hdc, leftCardX + 278, row6Y, 72, L"Both", activeTeam == INVENTORY_TEAM_BOTH);

    // Row 7: Action Button
    const int row7Y = leftCardY + 380;
    CasUiDrawButton(hdc, leftCardX + 20, row7Y, leftCardW - 40, 38,
        L"+  Add to Inventory", true);

    // Right Card: "PAINT KIT"
    const int rightCardX = 560;
    const int rightCardY = 66;
    const int rightCardW = 380;
    const int rightCardH = 526;
    DrawRoundedCard(hdc, rightCardX, rightCardY, rightCardW, rightCardH,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, L"PAINT KIT", rightCardX + 20, rightCardY + 16,
        rightCardW - 40, 18, CAS_UI_MUTED_2, 10, 700, DT_LEFT);

    const int totalSkins = CasUiSkinchangerTotalSkins(defIndex);
    constexpr int VISIBLE_SKINS = 9;
    const int maxScroll = totalSkins > VISIBLE_SKINS ? totalSkins - VISIBLE_SKINS : 0;
    g_casUiSkinchangerScroll = InventoryClampInt(g_casUiSkinchangerScroll, 0, maxScroll);

    for (int i = 0; i < VISIBLE_SKINS; ++i)
    {
        const int itemIdx = g_casUiSkinchangerScroll + i;
        if (itemIdx >= totalSkins)
            break;

        const wchar_t* fName = nullptr;
        int fPaint = 0;
        if (!CasUiSkinchangerGetSkin(defIndex, itemIdx, &fName, &fPaint))
            continue;

        const int itemY = rightCardY + 44 + i * 48;
        const bool isSelected = (fPaint == activePaint);

        // Hover / selection subtle highlight
        if (isSelected)
            CasUiDrawRect(hdc, rightCardX + 8, itemY,
                rightCardX + rightCardW - 20, itemY + 46, RGB_COLOR(25, 27, 35));

        // Rarity color
        COLORREF textColor = isSelected ? RGB_COLOR(255, 255, 255) :
            CasUiGetSkinRarityColor(fName, fPaint);

        CasUiDrawLabel(hdc, fName ? fName : L"Default", rightCardX + 20,
            itemY + 12, rightCardW - 80, 22, textColor, 13, 500, DT_LEFT);

        if (isSelected)
            CasUiDrawCheckmark(hdc, rightCardX + rightCardW - 44,
                itemY + 16, RGB_COLOR(240, 241, 245));

        // Divider
        CasUiDrawRect(hdc, rightCardX + 20, itemY + 46,
            rightCardX + rightCardW - 20, itemY + 47, RGB_COLOR(27, 29, 37));
    }

    // Scrollbar
    if (totalSkins > VISIBLE_SKINS)
    {
        const int scrollTrackY = rightCardY + 44;
        const int scrollTrackH = 432;
        CasUiDrawRect(hdc, rightCardX + rightCardW - 14, scrollTrackY,
            rightCardX + rightCardW - 10, scrollTrackY + scrollTrackH,
            RGB_COLOR(22, 24, 30));

        int thumbH = scrollTrackH * VISIBLE_SKINS / totalSkins;
        if (thumbH < 32) thumbH = 32;
        int thumbY = scrollTrackY + (scrollTrackH - thumbH) *
            g_casUiSkinchangerScroll / maxScroll;

        CasUiDrawRect(hdc, rightCardX + rightCardW - 14, thumbY,
            rightCardX + rightCardW - 10, thumbY + thumbH, CAS_UI_ACCENT);
    }
}

static const wchar_t* CasUiCategoryName(int category)
{
    switch (category)
    {
    case 1: return L"Pistols";
    case 2: return L"Rifles";
    case 3: return L"SMGs";
    case 4: return L"Heavy";
    case 5: return L"Snipers";
    case 6: return L"Shotguns";
    case 7: return L"Knives";
    case 8: return L"Gloves";
    case 9: return L"Agents";
    case 10: return L"Music kits";
    default: return L"All items";
    }
}

static const wchar_t* CasUiCategoryGlyph(int category)
{
    switch (category)
    {
    case 1: return L"P";
    case 2: return L"R";
    case 3: return L"S";
    case 4: return L"H";
    case 5: return L"SN";
    case 6: return L"SH";
    case 7: return L"K";
    case 8: return L"G";
    case 9: return L"A";
    case 10: return L"M";
    default: return L"*";
    }
}

static void CasUiDrawCategoryCard(HDC hdc, int x, int y, int category)
{
    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
    DrawRoundedCard(hdc, x + 14, y + 14, 52, 46,
        CAS_UI_ACCENT_SOFT, CAS_UI_BORDER_HI, 7);
    CasUiDrawLabel(hdc, CasUiCategoryGlyph(category), x + 14, y + 14,
        52, 46, CAS_UI_ACCENT, 18, 700, DT_CENTER);
    CasUiDrawLabel(hdc, CasUiCategoryName(category), x + 14, y + 72,
        CAS_UI_CARD_W - 28, 22, CAS_UI_TEXT, 13, 600, DT_LEFT);
    CasUiDrawRect(hdc, x, y + CAS_UI_CARD_H - 3,
        x + CAS_UI_CARD_W, y + CAS_UI_CARD_H, CAS_UI_ACCENT);
}

static void CasUiDrawBrowse(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawButton(hdc, CAS_UI_CONTENT_X, 66, 86, 30, L"<  Back");
    CasUiDrawLabel(hdc, L"Browse", CAS_UI_CONTENT_X + 102, 66,
        180, 30, CAS_UI_TEXT, 15, 650, DT_LEFT);
    CasUiDrawLabel(hdc, L"CATEGORIES", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);

    for (int i = 0; i < 10; ++i)
    {
        const int column = i % CAS_UI_GRID_COLUMNS;
        const int row = i / CAS_UI_GRID_COLUMNS;
        const int x = CAS_UI_CONTENT_X +
            column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
        const int y = CAS_UI_GRID_Y +
            row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
        CasUiDrawCategoryCard(hdc, x, y, i + 1);
    }
}

static bool CasUiWeaponMatchesCategory(
    const InventoryWeaponCatalogEntry& entry, int category)
{
    switch (category)
    {
    case 1: return entry.category == INVENTORY_CATEGORY_PISTOL;
    case 2: return entry.category == INVENTORY_CATEGORY_RIFLE;
    case 3: return entry.category == INVENTORY_CATEGORY_SMG;
    case 4: return entry.category == INVENTORY_CATEGORY_HEAVY;
    case 5: return entry.category == INVENTORY_CATEGORY_SNIPER;
    case 6: return entry.category == INVENTORY_CATEGORY_SHOTGUN;
    case 7: return entry.category == INVENTORY_CATEGORY_KNIFE &&
        entry.definitionIndex >= 500;
    default: return false;
    }
}

static int CasUiCatalogCount(int category)
{
    int count = 0;
    if (category >= 1 && category <= 7)
    {
        for (int i = 0; i < InventoryCatalogWeaponCount(); ++i)
            if (CasUiWeaponMatchesCategory(kInventoryWeapons[i], category))
                ++count;
    }
    else if (category == 8)
        count = InventoryGloveCatalogCount();
    else if (category == 9)
        count = InventoryAgentCatalogCount();
    else if (category == 10)
        count = InventoryMusicCatalogCount();
    return count;
}

static bool CasUiResolveCatalogItem(int category, int ordinal,
    int* kind, int* sourceIndex, const wchar_t** name)
{
    if (!kind || !sourceIndex || !name || ordinal < 0)
        return false;
    int seen = 0;
    if (category >= 1 && category <= 7)
    {
        for (int i = 0; i < InventoryCatalogWeaponCount(); ++i)
        {
            if (!CasUiWeaponMatchesCategory(kInventoryWeapons[i], category))
                continue;
            if (seen++ == ordinal)
            {
                *kind = 1;
                *sourceIndex = i;
                *name = kInventoryWeapons[i].displayName;
                return true;
            }
        }
    }
    else if (category == 8 && ordinal < InventoryGloveCatalogCount())
    {
        *kind = 2; *sourceIndex = ordinal;
        *name = kInventoryGloves[ordinal].displayName;
        return true;
    }
    else if (category == 9 && ordinal < InventoryAgentCatalogCount())
    {
        *kind = 3; *sourceIndex = ordinal;
        *name = kInventoryAgents[ordinal].displayName;
        return true;
    }
    else if (category == 10 && ordinal < InventoryMusicCatalogCount())
    {
        *kind = 4; *sourceIndex = ordinal;
        *name = kInventoryMusicKits[ordinal].displayName;
        return true;
    }
    return false;
}

static void CasUiDrawCatalogItemCard(HDC hdc, int x, int y,
    int category, int ordinal)
{
    int kind = 0;
    int source = 0;
    const wchar_t* name = nullptr;
    if (!CasUiResolveCatalogItem(category, ordinal, &kind, &source, &name))
        return;
    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
    DrawRoundedCard(hdc, x + 14, y + 14, 52, 44,
        CAS_UI_ACCENT_SOFT, CAS_UI_BORDER_HI, 7);
    wchar_t glyph[4] = { L'?', 0, 0, 0 };
    if (name && name[0])
    {
        glyph[0] = name[0];
        if (name[1] && name[1] != L' ')
            glyph[1] = name[1];
    }
    CasUiDrawLabel(hdc, glyph, x + 14, y + 14,
        52, 44, CAS_UI_ACCENT, 18, 700, DT_CENTER);
    CasUiDrawLabel(hdc, name, x + 14, y + 68,
        CAS_UI_CARD_W - 28, 20, CAS_UI_TEXT, 12, 600, DT_LEFT);
    CasUiDrawLabel(hdc, L"Click to add", x + 14, y + 87,
        CAS_UI_CARD_W - 28, 16, CAS_UI_MUTED_2, 10, 400, DT_LEFT);
    CasUiDrawRect(hdc, x, y + CAS_UI_CARD_H - 3,
        x + CAS_UI_CARD_W, y + CAS_UI_CARD_H, CAS_UI_ACCENT);
}

static void CasUiDrawCategory(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawButton(hdc, CAS_UI_CONTENT_X, 66, 86, 30, L"<  Back");
    CasUiDrawLabel(hdc, L"Browse  /", CAS_UI_CONTENT_X + 102, 66,
        68, 30, CAS_UI_MUTED, 13, 500, DT_LEFT);
    CasUiDrawLabel(hdc, CasUiCategoryName(g_casUiBrowseCategory),
        CAS_UI_CONTENT_X + 170, 66, 250, 30,
        CAS_UI_TEXT, 15, 650, DT_LEFT);

    const int total = CasUiCatalogCount(g_casUiBrowseCategory);
    const int maxPage = total > 0 ? (total - 1) / CAS_UI_GRID_PAGE : 0;
    g_casUiCatalogPage = InventoryClampInt(g_casUiCatalogPage, 0, maxPage);
    const int first = g_casUiCatalogPage * CAS_UI_GRID_PAGE;
    const int visible = total - first > CAS_UI_GRID_PAGE ?
        CAS_UI_GRID_PAGE : total - first;

    CasUiDrawLabel(hdc, L"ITEMS", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < visible; ++i)
    {
        const int column = i % CAS_UI_GRID_COLUMNS;
        const int row = i / CAS_UI_GRID_COLUMNS;
        const int x = CAS_UI_CONTENT_X +
            column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
        const int y = CAS_UI_GRID_Y +
            row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
        CasUiDrawCatalogItemCard(hdc, x, y,
            g_casUiBrowseCategory, first + i);
    }
    if (total > CAS_UI_GRID_PAGE)
    {
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X, height - 58,
            78, 30, L"<  Prev");
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X + 86, height - 58,
            78, 30, L"Next  >");
    }
}

static void CasUiDrawInventoryDetail(HDC hdc, int width, int height)
{
    CasUiDrawInventoryDetailV2(hdc, width, height);
}

static int CasUiSkinBrowserCount()
{
    if (InventoryGameCatalogReady())
    {
        const int count = InventoryGameCatalogCountForDefinition(g_casUiSkinBrowserDefIndex);
        if (count > 0)
            return count;
    }
    return InventoryCatalogPaintCount();
}

static bool CasUiSkinBrowserItem(int index, const wchar_t** name, int* paintKit)
{
    if (index < 0)
        return false;
    if (InventoryGameCatalogReady())
    {
        const InventoryGameCatalogRecord* rec =
            InventoryGameCatalogOrdinalForDefinition(
                g_casUiSkinBrowserDefIndex, index);
        if (rec)
        {
            if (name)
                *name = (rec->finishName[0]) ? rec->finishName :
                    (rec->displayName[0] ? rec->displayName : L"Vanilla");
            if (paintKit)
                *paintKit = rec->paintKit;
            return true;
        }
    }
    if (index < InventoryCatalogPaintCount())
    {
        if (paintKit)
            *paintKit = kInventoryPaints[index].paintKit;
        if (name)
            *name = kInventoryPaints[index].displayName;
        return true;
    }
    return false;
}

static void CasUiDrawSkinCard(HDC hdc, int x, int y,
    const wchar_t* name, int paintKit, bool selected)
{
    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, 42,
        selected ? CAS_UI_ACCENT_SOFT : CAS_UI_SURFACE_2,
        selected ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    if (selected)
        CasUiDrawRect(hdc, x, y + 4, x + 3, y + 38, CAS_UI_ACCENT);

    CasUiDrawLabel(hdc, name ? name : L"Vanilla", x + 12, y + 4,
        CAS_UI_CARD_W - 24, 18, CAS_UI_TEXT, 12, 600, DT_LEFT);

    wchar_t paintStr[24]{};
    paintStr[0] = L'#';
    int temp = paintKit;
    if (temp <= 0)
    {
        paintStr[1] = L'0'; paintStr[2] = 0;
    }
    else
    {
        int digits = 0;
        int t = temp;
        while (t > 0) { ++digits; t /= 10; }
        paintStr[digits + 1] = 0;
        for (int i = digits; i > 0; --i)
        {
            paintStr[i] = static_cast<wchar_t>(L'0' + (temp % 10));
            temp /= 10;
        }
    }
    CasUiDrawLabel(hdc, paintStr, x + 12, y + 22,
        CAS_UI_CARD_W - 24, 16, CAS_UI_MUTED_2, 10, 400, DT_LEFT);
}

static bool CasUiAddSkinFromBrowser()
{
    if (!InventoryTryLock())
        return false;
    if (g_inventoryStore.itemCount >= INVENTORY_MAX_ITEMS)
    {
        InventoryUnlock();
        return false;
    }

    VirtualInventoryItem item;
    ZeroBytes(&item, sizeof(item));
    item.itemId = g_inventoryStore.nextItemId++;
    item.overrideDefinitionIndex = g_casUiSkinBrowserDefIndex;
    item.statTrak = g_casUiSkinPreviewStatTrak;
    item.wear = g_casUiSkinPreviewWear;
    item.equippedTeams = g_casUiSkinPreviewTeam;

    if (g_casUiSkinBrowserKind == 2)
    {
        item.slotDefinitionIndex = INVENTORY_SLOT_GLOVE;
        item.quality = 3;
    }
    else
    {
        item.slotDefinitionIndex = InventoryIsKnifeDefinition(g_casUiSkinBrowserDefIndex) ?
            INVENTORY_SLOT_KNIFE : g_casUiSkinBrowserDefIndex;
        item.quality = item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ? 3 : 0;
    }

    if (g_casUiSkinBrowserSelected >= 0)
    {
        const wchar_t* fn = nullptr;
        int pk = 0;
        if (CasUiSkinBrowserItem(g_casUiSkinBrowserSelected, &fn, &pk))
            item.paintKit = pk;
    }

    const int index = g_inventoryStore.itemCount++;
    g_inventoryStore.items[index] = item;
    InventoryEquipItemLocked(index, item.equippedTeams);
    g_inventoryUiSelected = index;
    g_inventoryUiPage = index / 7;
    MarkInventoryDirty();
    InventoryUnlock();
    return true;
}

static void CasUiDrawSkinBrowser(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawButton(hdc, CAS_UI_CONTENT_X, 66, 86, 30, L"< Back");

    const wchar_t* weaponName =
        InventoryCatalogWeaponName(g_casUiSkinBrowserDefIndex);
    CasUiDrawLabel(hdc, L"Browse  /", CAS_UI_CONTENT_X + 102, 66,
        68, 30, CAS_UI_MUTED, 13, 500, DT_LEFT);
    CasUiDrawLabel(hdc, weaponName, CAS_UI_CONTENT_X + 170, 66,
        250, 30, CAS_UI_TEXT, 15, 650, DT_LEFT);

    CasUiDrawLabel(hdc, L"AVAILABLE SKINS", CAS_UI_CONTENT_X, 104,
        200, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);

    const int skinCount = CasUiSkinBrowserCount();
    constexpr int SKINS_PER_PAGE = 9;
    const int maxPage = skinCount > 0 ? (skinCount - 1) / SKINS_PER_PAGE : 0;
    g_casUiSkinBrowserPage =
        InventoryClampInt(g_casUiSkinBrowserPage, 0, maxPage);
    const int first = g_casUiSkinBrowserPage * SKINS_PER_PAGE;
    int visible = skinCount - first;
    if (visible > SKINS_PER_PAGE) visible = SKINS_PER_PAGE;
    if (visible < 0) visible = 0;

    for (int i = 0; i < visible; ++i)
    {
        const wchar_t* finishName = nullptr;
        int finishPaint = 0;
        if (CasUiSkinBrowserItem(first + i, &finishName, &finishPaint))
        {
            const int y = 128 + i * 46;
            CasUiDrawSkinCard(hdc, CAS_UI_CONTENT_X, y, finishName, finishPaint,
                g_casUiSkinBrowserSelected == first + i);
        }
    }

    if (skinCount > SKINS_PER_PAGE)
    {
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X, height - 58,
            78, 30, L"< Prev");
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X + 86, height - 58,
            78, 30, L"Next >");
    }

    const int panelX = CAS_UI_CONTENT_X + 400;
    const int panelW = 360;
    DrawRoundedCard(hdc, panelX, 104, panelW, 450,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, L"PREVIEW", panelX + 16, 116,
        panelW - 32, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);

    COLORREF accent = CAS_UI_ACCENT;
    if (InventoryIsKnifeDefinition(g_casUiSkinBrowserDefIndex))
        accent = RGB_COLOR(198, 84, 222);
    CasUiDrawItemGlyph(hdc, panelX + 16, 144, weaponName, accent);
    CasUiDrawLabel(hdc, weaponName, panelX + 78, 144,
        panelW - 94, 24, CAS_UI_TEXT, 17, 650, DT_LEFT);

    const wchar_t* skinName = L"Vanilla (no skin)";
    int selectedPaintKit = 0;
    if (g_casUiSkinBrowserSelected >= 0)
    {
        const wchar_t* curName = nullptr;
        int curPaint = 0;
        if (CasUiSkinBrowserItem(g_casUiSkinBrowserSelected, &curName, &curPaint))
        {
            skinName = curName;
            selectedPaintKit = curPaint;
        }
    }
    CasUiDrawLabel(hdc, skinName, panelX + 78, 170,
        panelW - 94, 20, CAS_UI_ACCENT, 13, 600, DT_LEFT);

    CasUiDrawLabel(hdc, L"Paint kit", panelX + 16, 208,
        80, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    RECT pkValue = { panelX + 100, 207, panelX + 180, 226 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(selectedPaintKit), &pkValue,
        DT_LEFT | DT_SINGLELINE);

    CasUiDrawLabel(hdc, L"Wear", panelX + 16, 242,
        60, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    CasUiDrawLabel(hdc, CasUiWearLabel(g_casUiSkinPreviewWear),
        panelX + 80, 240, 120, 20, CAS_UI_TEXT, 11, 550, DT_LEFT);
    CasUiDrawValuePill(hdc, panelX + 16, 264, 56, L"FN",
        g_casUiSkinPreviewWear < 0.07f);
    CasUiDrawValuePill(hdc, panelX + 78, 264, 56, L"MW",
        g_casUiSkinPreviewWear >= 0.07f && g_casUiSkinPreviewWear < 0.15f);
    CasUiDrawValuePill(hdc, panelX + 140, 264, 56, L"FT",
        g_casUiSkinPreviewWear >= 0.15f && g_casUiSkinPreviewWear < 0.38f);
    CasUiDrawValuePill(hdc, panelX + 202, 264, 56, L"WW",
        g_casUiSkinPreviewWear >= 0.38f && g_casUiSkinPreviewWear < 0.45f);
    CasUiDrawValuePill(hdc, panelX + 264, 264, 56, L"BS",
        g_casUiSkinPreviewWear >= 0.45f);

    CasUiDrawLabel(hdc, L"StatTrak", panelX + 16, 310,
        80, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    CasUiDrawValuePill(hdc, panelX + 100, 304, 60, L"Off",
        g_casUiSkinPreviewStatTrak < 0);
    CasUiDrawValuePill(hdc, panelX + 166, 304, 60, L"On",
        g_casUiSkinPreviewStatTrak >= 0);

    CasUiDrawLabel(hdc, L"Equip", panelX + 16, 350,
        60, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    CasUiDrawValuePill(hdc, panelX + 16, 374, 68, L"T",
        g_casUiSkinPreviewTeam == INVENTORY_TEAM_T);
    CasUiDrawValuePill(hdc, panelX + 90, 374, 68, L"CT",
        g_casUiSkinPreviewTeam == INVENTORY_TEAM_CT);
    CasUiDrawValuePill(hdc, panelX + 164, 374, 82, L"T + CT",
        g_casUiSkinPreviewTeam == INVENTORY_TEAM_BOTH);

    CasUiDrawButton(hdc, panelX + 16, 420, panelW - 32, 38,
        L"Add to inventory", true);
    CasUiDrawButton(hdc, panelX + 16, 466, panelW - 32, 30,
        L"Add vanilla (no skin)");
}

static void CasUiDrawInventoryScreen(HDC hdc, int width, int height)
{
    if (g_casUiSkinchangerSubtab == 3)
    {
        if (g_casUiInventoryView == 1)
            CasUiDrawBrowse(hdc, width, height);
        else if (g_casUiInventoryView == 2)
            CasUiDrawCategory(hdc, width, height);
        else if (g_casUiInventoryView == 3)
            CasUiDrawInventoryDetail(hdc, width, height);
        else if (g_casUiInventoryView == 4)
            CasUiDrawSkinBrowser(hdc, width, height);
        else
            CasUiDrawInventoryHome(hdc, width, height);
    }
    else
    {
        CasUiDrawSkinchanger(hdc, width, height);
    }
}

static bool CasUiAddSpecificItem(int kind, int sourceIndex)
{
    if (!InventoryTryLock())
        return false;
    if (g_inventoryStore.itemCount >= INVENTORY_MAX_ITEMS)
    {
        InventoryUnlock();
        return false;
    }

    VirtualInventoryItem item;
    ZeroBytes(&item, sizeof(item));
    item.itemId = g_inventoryStore.nextItemId++;
    item.statTrak = -1;
    item.wear = 0.0001f;

    if (kind == 1 && sourceIndex >= 0 &&
        sourceIndex < InventoryCatalogWeaponCount())
    {
        const InventoryWeaponCatalogEntry& entry = kInventoryWeapons[sourceIndex];
        item.slotDefinitionIndex = InventoryIsKnifeDefinition(entry.definitionIndex) ?
            INVENTORY_SLOT_KNIFE : entry.definitionIndex;
        item.overrideDefinitionIndex = entry.definitionIndex;
        item.equippedTeams = InventoryDefaultEquipForCatalogWeapon(entry);
        item.quality = item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ? 3 : 0;
    }
    else if (kind == 2 && sourceIndex >= 0 &&
        sourceIndex < InventoryGloveCatalogCount())
    {
        item.slotDefinitionIndex = INVENTORY_SLOT_GLOVE;
        item.overrideDefinitionIndex = kInventoryGloves[sourceIndex].definitionIndex;
        item.equippedTeams = InventoryTeamBit(g_inventoryActiveTeam);
        if (item.equippedTeams == INVENTORY_TEAM_NONE)
            item.equippedTeams = INVENTORY_TEAM_BOTH;
        item.quality = 3;
    }
    else if (kind == 3 && sourceIndex >= 0 &&
        sourceIndex < InventoryAgentCatalogCount())
    {
        item.slotDefinitionIndex = INVENTORY_SLOT_AGENT;
        item.overrideDefinitionIndex = kInventoryAgents[sourceIndex].definitionIndex;
        item.equippedTeams = kInventoryAgents[sourceIndex].teamMask;
        item.quality = 4;
    }
    else if (kind == 4 && sourceIndex >= 0 &&
        sourceIndex < InventoryMusicCatalogCount())
    {
        item.slotDefinitionIndex = INVENTORY_SLOT_MUSIC;
        item.overrideDefinitionIndex = kInventoryMusicKits[sourceIndex].musicKitId;
        item.equippedTeams = INVENTORY_TEAM_BOTH;
        item.quality = 0;
    }
    else
    {
        InventoryUnlock();
        return false;
    }

    const int index = g_inventoryStore.itemCount++;
    g_inventoryStore.items[index] = item;
    InventoryEquipItemLocked(index, item.equippedTeams);
    g_inventoryUiSelected = index;
    g_inventoryUiPage = index / 7;
    MarkInventoryDirty();
    InventoryUnlock();
    g_casUiInventoryView = 3;
    return true;
}

static bool CasUiSelectInventoryCard(int visibleIndex)
{
    CasUiInventoryCard cards[CAS_UI_GRID_PAGE]{};
    int total = 0;
    const int count = CasUiCopyInventoryPage(cards, CAS_UI_GRID_PAGE, &total);
    if (visibleIndex < 0 || visibleIndex >= count)
        return false;
    if (!InventoryTryLock())
        return false;
    const int absolute = cards[visibleIndex].absoluteIndex;
    if (absolute < 0 || absolute >= g_inventoryStore.itemCount)
    {
        InventoryUnlock();
        return false;
    }
    g_inventoryUiSelected = absolute;
    g_inventoryUiPage = absolute / 7;
    InventoryUnlock();
    g_casUiInventoryView = 3;
    return true;
}

static bool CasUiHandleInventoryClick(int mouseX, int mouseY, int height)
{
    // Subtab bar clicks (Gloves | Knife | Weapon | My Items)
    if (mouseY >= 20 && mouseY <= 54)
    {
        const int subtabX[] = { 360, 434, 500, 584 };
        const int subtabW[] = { 68,  60,  78,  84 };
        for (int i = 0; i < 4; ++i)
        {
            if (CasUiPointInRect(mouseX, mouseY, subtabX[i], 22, subtabW[i], 32))
            {
                g_casUiSkinchangerSubtab = i;
                g_casUiSkinchangerScroll = 0;
                if (i == 3) g_casUiInventoryView = 0;
                return true;
            }
        }
    }

    // If on Skinchanger 2-Column view
    if (g_casUiSkinchangerSubtab != 3)
    {
        const int leftCardX = 174;
        const int leftCardY = 66;
        const int leftCardW = 370;

        const wchar_t* curModelName = nullptr;
        const unsigned short defIndex =
            CasUiGetCurrentDefinition(g_casUiSkinchangerSubtab, &curModelName);

        // Row 1: Enable Toggle
        const int row1Y = leftCardY + 48;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + leftCardW - 58, row1Y, 38, 20))
        {
            g_casUiSkinchangerEnable = !g_casUiSkinchangerEnable;
            return true;
        }

        // Row 2: Model Steppers / Selector
        const int row2Y = leftCardY + 98;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 150, row2Y, 28, 34)) // < Prev
        {
            if (g_casUiSkinchangerSubtab == 0)
                g_casUiSelectedGloveIndex = (g_casUiSelectedGloveIndex + kCasUiGloveCount - 1) % kCasUiGloveCount;
            else if (g_casUiSkinchangerSubtab == 1)
                g_casUiSelectedKnifeIndex = (g_casUiSelectedKnifeIndex + kCasUiKnifeCount - 1) % kCasUiKnifeCount;
            else
                g_casUiSelectedWeaponIndex = (g_casUiSelectedWeaponIndex + kCasUiWeaponCount - 1) % kCasUiWeaponCount;
            g_casUiSkinchangerScroll = 0;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 322, row2Y, 28, 34) ||
            CasUiPointInRect(mouseX, mouseY, leftCardX + 178, row2Y, 144, 34)) // > Next or Center
        {
            if (g_casUiSkinchangerSubtab == 0)
                g_casUiSelectedGloveIndex = (g_casUiSelectedGloveIndex + 1) % kCasUiGloveCount;
            else if (g_casUiSkinchangerSubtab == 1)
                g_casUiSelectedKnifeIndex = (g_casUiSelectedKnifeIndex + 1) % kCasUiKnifeCount;
            else
                g_casUiSelectedWeaponIndex = (g_casUiSelectedWeaponIndex + 1) % kCasUiWeaponCount;
            g_casUiSkinchangerScroll = 0;
            return true;
        }

        // Row 3: Seed [-] [+]
        const int row3Y = leftCardY + 150;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 276, row3Y, 34, 32))
        {
            if (g_casUiSkinchangerSeed > 0) --g_casUiSkinchangerSeed;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 316, row3Y, 34, 32))
        {
            if (g_casUiSkinchangerSeed < 1000) ++g_casUiSkinchangerSeed;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }

        // Row 4: Wear Slider
        const int row4Y = leftCardY + 204;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 140, row4Y, 150, 28))
        {
            float norm = static_cast<float>(mouseX - (leftCardX + 150)) / 130.0f;
            if (norm < 0.0001f) norm = 0.0001f;
            if (norm > 1.0f) norm = 1.0f;
            g_casUiSkinchangerWear = norm;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }

        // Row 5: StatTrak
        const int row5Y = leftCardY + 258;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 190, row5Y, 48, 28))
        {
            g_casUiSkinchangerStatTrak = -1;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 244, row5Y, 34, 28))
        {
            if (g_casUiSkinchangerStatTrak > 0) g_casUiSkinchangerStatTrak = (g_casUiSkinchangerStatTrak >= 100) ? g_casUiSkinchangerStatTrak - 100 : 0;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 282, row5Y, 34, 28))
        {
            g_casUiSkinchangerStatTrak = (g_casUiSkinchangerStatTrak < 0) ? 100 : g_casUiSkinchangerStatTrak + 100;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 320, row5Y, 30, 28))
        {
            g_casUiSkinchangerStatTrak = (g_casUiSkinchangerStatTrak >= 0) ? -1 : 0;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }

        // Row 6: Team
        const int row6Y = leftCardY + 310;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 150, row6Y, 58, 28))
        {
            g_casUiSkinchangerTeam = INVENTORY_TEAM_T;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 214, row6Y, 58, 28))
        {
            g_casUiSkinchangerTeam = INVENTORY_TEAM_CT;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 278, row6Y, 72, 28))
        {
            g_casUiSkinchangerTeam = INVENTORY_TEAM_BOTH;
            CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                0, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
            return true;
        }

        // Row 7: Add to Inventory
        const int row7Y = leftCardY + 380;
        if (CasUiPointInRect(mouseX, mouseY, leftCardX + 20, row7Y, leftCardW - 40, 38))
        {
            g_casUiSkinchangerSubtab = 3;
            g_casUiInventoryView = 0;
            return true;
        }

        // Right Card: PAINT KIT Skin Rows
        const int rightCardX = 560;
        const int rightCardY = 66;
        const int rightCardW = 380;
        const int totalSkins = CasUiSkinchangerTotalSkins(defIndex);
        constexpr int VISIBLE_SKINS = 9;

        for (int i = 0; i < VISIBLE_SKINS; ++i)
        {
            const int itemIdx = g_casUiSkinchangerScroll + i;
            if (itemIdx >= totalSkins)
                break;

            const int itemY = rightCardY + 44 + i * 48;
            if (CasUiPointInRect(mouseX, mouseY, rightCardX + 8, itemY, rightCardW - 30, 46))
            {
                const wchar_t* fName = nullptr;
                int fPaint = 0;
                if (CasUiSkinchangerGetSkin(defIndex, itemIdx, &fName, &fPaint))
                {
                    CasUiApplySkinchangerSelection(g_casUiSkinchangerSubtab,
                        (g_casUiSkinchangerSubtab == 0 ? g_casUiSelectedGloveIndex : (g_casUiSkinchangerSubtab == 1 ? g_casUiSelectedKnifeIndex : g_casUiSelectedWeaponIndex)),
                        fPaint, g_casUiSkinchangerSeed, g_casUiSkinchangerWear, g_casUiSkinchangerStatTrak, g_casUiSkinchangerTeam);
                }
                return true;
            }
        }

        // Scrollbar / Pagination
        if (CasUiPointInRect(mouseX, mouseY, rightCardX + rightCardW - 16, rightCardY + 44, 16, 432))
        {
            float norm = static_cast<float>(mouseY - (rightCardY + 44)) / 432.0f;
            const int maxScroll = totalSkins > VISIBLE_SKINS ? totalSkins - VISIBLE_SKINS : 0;
            g_casUiSkinchangerScroll = InventoryClampInt(static_cast<int>(norm * maxScroll), 0, maxScroll);
            return true;
        }

        return true;
    }

    // My Items / Advanced Views click handling
    if (g_casUiInventoryView == 3)
    {
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 0;
            return true;
        }
        return CasUiHandleInventoryDetailV2Click(mouseX, mouseY);
    }

    if (g_casUiInventoryView == 0)
    {
        if (CasUiPointInRect(mouseX, mouseY, 804, 66, 132, 34) ||
            CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X + 28, 270, 140, 34))
        {
            g_casUiInventoryView = 1;
            g_casUiCatalogPage = 0;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, 610, 66, 180, 34))
        {
            InventoryUiBeginSearchEdit();
            return true;
        }
        for (int i = 0; i < CAS_UI_GRID_PAGE; ++i)
        {
            const int column = i % CAS_UI_GRID_COLUMNS;
            const int row = i / CAS_UI_GRID_COLUMNS;
            const int x = CAS_UI_CONTENT_X + column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y + row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (CasUiPointInRect(mouseX, mouseY, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H))
                return CasUiSelectInventoryCard(i);
        }
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, height - 58, 78, 30))
        {
            if (g_casUiInventoryPage > 0) --g_casUiInventoryPage;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X + 86, height - 58, 78, 30))
        {
            ++g_casUiInventoryPage;
            return true;
        }
        return true;
    }

    if (g_casUiInventoryView == 1)
    {
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 0;
            return true;
        }
        for (int i = 0; i < 10; ++i)
        {
            const int column = i % CAS_UI_GRID_COLUMNS;
            const int row = i / CAS_UI_GRID_COLUMNS;
            const int x = CAS_UI_CONTENT_X + column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y + row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (CasUiPointInRect(mouseX, mouseY, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H))
            {
                g_casUiBrowseCategory = i + 1;
                g_casUiCatalogPage = 0;
                g_casUiInventoryView = 2;
                return true;
            }
        }
        return true;
    }

    if (g_casUiInventoryView == 2)
    {
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 1;
            return true;
        }
        const int total = CasUiCatalogCount(g_casUiBrowseCategory);
        const int first = g_casUiCatalogPage * CAS_UI_GRID_PAGE;
        int visible = total - first;
        if (visible > CAS_UI_GRID_PAGE) visible = CAS_UI_GRID_PAGE;
        if (visible < 0) visible = 0;
        for (int i = 0; i < visible; ++i)
        {
            const int column = i % CAS_UI_GRID_COLUMNS;
            const int row = i / CAS_UI_GRID_COLUMNS;
            const int x = CAS_UI_CONTENT_X + column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y + row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (!CasUiPointInRect(mouseX, mouseY, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H))
                continue;
            int kind = 0;
            int source = 0;
            const wchar_t* name = nullptr;
            if (CasUiResolveCatalogItem(g_casUiBrowseCategory, first + i, &kind, &source, &name))
            {
                if (kind == 1 || kind == 2)
                {
                    g_casUiSkinBrowserKind = kind;
                    g_casUiSkinBrowserSourceIndex = source;
                    if (kind == 1)
                        g_casUiSkinBrowserDefIndex = kInventoryWeapons[source].definitionIndex;
                    else if (kind == 2)
                        g_casUiSkinBrowserDefIndex = kInventoryGloves[source].definitionIndex;
                    g_casUiSkinBrowserPage = 0;
                    g_casUiSkinBrowserSelected = -1;
                    g_casUiSkinPreviewWear = 0.0001f;
                    g_casUiSkinPreviewStatTrak = -1;
                    g_casUiSkinPreviewTeam = InventoryTeamBit(g_inventoryActiveTeam);
                    if (g_casUiSkinPreviewTeam == INVENTORY_TEAM_NONE)
                        g_casUiSkinPreviewTeam = INVENTORY_TEAM_BOTH;
                    g_casUiInventoryView = 4;
                }
                else
                {
                    CasUiAddSpecificItem(kind, source);
                }
            }
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, height - 58, 78, 30))
        {
            if (g_casUiCatalogPage > 0) --g_casUiCatalogPage;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X + 86, height - 58, 78, 30))
        {
            ++g_casUiCatalogPage;
            return true;
        }
        return true;
    }

    if (g_casUiInventoryView == 4)
    {
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 2;
            return true;
        }

        constexpr int SKINS_PER_PAGE = 9;
        const int skinCount = CasUiSkinBrowserCount();
        const int first2 = g_casUiSkinBrowserPage * SKINS_PER_PAGE;
        int visible2 = skinCount - first2;
        if (visible2 > SKINS_PER_PAGE) visible2 = SKINS_PER_PAGE;
        if (visible2 < 0) visible2 = 0;

        for (int i = 0; i < visible2; ++i)
        {
            const int y = 128 + i * 46;
            if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, y, CAS_UI_CARD_W, 42))
            {
                g_casUiSkinBrowserSelected = first2 + i;
                return true;
            }
        }

        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X, height - 58, 78, 30))
        {
            if (g_casUiSkinBrowserPage > 0) --g_casUiSkinBrowserPage;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY, CAS_UI_CONTENT_X + 86, height - 58, 78, 30))
        {
            ++g_casUiSkinBrowserPage;
            return true;
        }

        const int panelX = CAS_UI_CONTENT_X + 400;
        const int panelW = 360;

        if (CasUiPointInRect(mouseX, mouseY, panelX + 16, 264, 56, 28))
            g_casUiSkinPreviewWear = 0.0001f;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 78, 264, 56, 28))
            g_casUiSkinPreviewWear = 0.08f;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 140, 264, 56, 28))
            g_casUiSkinPreviewWear = 0.20f;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 202, 264, 56, 28))
            g_casUiSkinPreviewWear = 0.40f;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 264, 264, 56, 28))
            g_casUiSkinPreviewWear = 0.55f;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 100, 304, 60, 28))
            g_casUiSkinPreviewStatTrak = -1;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 166, 304, 60, 28))
            g_casUiSkinPreviewStatTrak = 0;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 16, 374, 68, 28))
            g_casUiSkinPreviewTeam = INVENTORY_TEAM_T;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 90, 374, 68, 28))
            g_casUiSkinPreviewTeam = INVENTORY_TEAM_CT;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 164, 374, 82, 28))
            g_casUiSkinPreviewTeam = INVENTORY_TEAM_BOTH;
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 16, 420, panelW - 32, 38))
        {
            if (CasUiAddSkinFromBrowser())
                g_casUiInventoryView = 3;
            return true;
        }
        else if (CasUiPointInRect(mouseX, mouseY, panelX + 16, 466, panelW - 32, 30))
        {
            g_casUiSkinBrowserSelected = -1;
            if (CasUiAddSkinFromBrowser())
                g_casUiInventoryView = 3;
            return true;
        }
        return true;
    }
    return true;
}

static bool CasUiHandleSidebarClick(HWND wnd, int mouseX, int mouseY)
{
    if (mouseX < 14 || mouseX >= 144)
        return false;
    for (int i = 0; i < 5; ++i)
    {
        const int y = 116 + i * 50;
        if (!CasUiPointInRect(mouseX, mouseY, 14, y, 130, 38))
            continue;
        g_espConfig.selectedTab = i;
        if (i != 3)
            AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NONE);
        InvalidateRect(wnd, nullptr, FALSE);
        return true;
    }
    return false;
}

static bool CasUiPrepareMenuClick(HWND wnd, int* mouseX, int* mouseY,
    int windowHeight)
{
    if (!mouseX || !mouseY)
        return false;
    if (CasUiHandleSidebarClick(wnd, *mouseX, *mouseY))
        return true;

    if (g_espConfig.selectedTab == 3)
    {
        const bool consumed = CasUiHandleInventoryClick(
            *mouseX, *mouseY, windowHeight);
        if (consumed)
            InvalidateRect(wnd, nullptr, FALSE);
        return true;
    }

    // Visual modals are deliberately left in window coordinates.
    if (g_espConfig.showRgbPickerModal || g_espConfig.showChamsModal ||
        g_espConfig.showManageModal)
        return false;

    // Legacy feature panels are rendered through the same viewport offset.
    *mouseX -= 160;
    *mouseY -= 60;
    return false;
}