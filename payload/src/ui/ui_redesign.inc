// cas+ unified menu redesign.
//
// Visual direction is intentionally inspired by modern dark inventory tools:
// a persistent vertical navigation rail, layered content surface, breadcrumbs,
// search, category cards and an inventory grid.  The implementation stays GDI
// and no-CRT friendly so it can live inside the existing manual-mapped payload.
// Existing feature logic is not duplicated: the new UI calls the current
// InventoryStore/catalog helpers and falls back to the legacy detail editor
// while that editor is migrated component-by-component.

extern "C" __declspec(dllimport) BOOL WINAPI SetViewportOrgEx(
    HDC, int, int, POINT*);

static const COLORREF CAS_UI_BG          = RGB_COLOR(10, 11, 17);
static const COLORREF CAS_UI_SIDEBAR     = RGB_COLOR(14, 16, 25);
static const COLORREF CAS_UI_SURFACE     = RGB_COLOR(17, 20, 31);
static const COLORREF CAS_UI_SURFACE_2   = RGB_COLOR(22, 25, 39);
static const COLORREF CAS_UI_SURFACE_3   = RGB_COLOR(28, 31, 47);
static const COLORREF CAS_UI_BORDER      = RGB_COLOR(39, 42, 61);
static const COLORREF CAS_UI_BORDER_HI   = RGB_COLOR(64, 57, 92);
static const COLORREF CAS_UI_TEXT        = RGB_COLOR(238, 236, 247);
static const COLORREF CAS_UI_MUTED       = RGB_COLOR(139, 143, 163);
static const COLORREF CAS_UI_MUTED_2     = RGB_COLOR(92, 97, 119);
static const COLORREF CAS_UI_ACCENT      = RGB_COLOR(124, 92, 255);
static const COLORREF CAS_UI_ACCENT_SOFT = RGB_COLOR(42, 34, 73);
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

static int g_casUiInventoryView = 0; // 0 items, 1 browse, 2 category, 3 detail
static int g_casUiBrowseCategory = 0;
static int g_casUiInventoryPage = 0;
static int g_casUiCatalogPage = 0;

struct CasUiInventoryCard {
    VirtualInventoryItem item;
    int absoluteIndex;
};

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

static const wchar_t* CasUiTabTitle(int tab)
{
    switch (tab)
    {
    case 0: return L"Ragebot";
    case 1: return L"Anti-Aim";
    case 2: return L"Visuals";
    case 3: return L"Inventory";
    case 4: return L"Configs";
    default: return L"cas+";
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
    const wchar_t* name = CasUiInventoryDomainName(item);
    const COLORREF accent = CasUiItemAccent(item);

    DrawRoundedCard(hdc, x, y, CAS_UI_CARD_W, CAS_UI_CARD_H,
        CAS_UI_SURFACE_2, CAS_UI_BORDER, 7);
    CasUiDrawRect(hdc, x, y + CAS_UI_CARD_H - 3,
        x + CAS_UI_CARD_W, y + CAS_UI_CARD_H, accent);
    CasUiDrawItemGlyph(hdc, x + 14, y + 14, name, accent);

    CasUiDrawLabel(hdc, name, x + 14, y + 60,
        CAS_UI_CARD_W - 28, 18, CAS_UI_TEXT, 12, 600, DT_LEFT);

    wchar_t subtitle[40]{};
    if (item.customNameLength > 0)
        CasUiAsciiToWide(item.customName, subtitle,
            static_cast<int>(sizeof(subtitle) / sizeof(subtitle[0])));
    else if (item.paintKit > 0)
    {
        subtitle[0] = L'P'; subtitle[1] = L'a'; subtitle[2] = L'i';
        subtitle[3] = L'n'; subtitle[4] = L't'; subtitle[5] = L' ';
        subtitle[6] = L'#'; subtitle[7] = 0;
    }
    else
    {
        subtitle[0] = L'D'; subtitle[1] = L'e'; subtitle[2] = L'f';
        subtitle[3] = L'a'; subtitle[4] = L'u'; subtitle[5] = L'l';
        subtitle[6] = L't'; subtitle[7] = 0;
    }
    CasUiDrawLabel(hdc, subtitle, x + 14, y + 80,
        92, 17, CAS_UI_MUTED, 11, 400, DT_LEFT);

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

static const wchar_t* CasUiCategoryName(int category)
{
    switch (category)
    {
    case 1: return L"Pistols";
    case 2: return L"Rifles";
    case 3: return L"SMG";
    case 4: return L"Heavy";
    case 5: return L"Snipers";
    case 6: return L"Shotguns";
    case 7: return L"Knives";
    case 8: return L"Gloves";
    case 9: return L"Agents";
    case 10: return L"Music kits";
    default: return L"Browse";
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
    case 6: return L"SG";
    case 7: return L"K";
    case 8: return L"GL";
    case 9: return L"AG";
    case 10: return L"M";
    default: return L"+";
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
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawButton(hdc, CAS_UI_CONTENT_X, 66, 86, 30, L"<  Items");
    CasUiDrawLabel(hdc, L"Item editor", CAS_UI_CONTENT_X + 102, 66,
        220, 30, CAS_UI_TEXT, 15, 650, DT_LEFT);

    POINT oldOrigin{};
    SetViewportOrgEx(hdc, 150, 48, &oldOrigin);
    DrawInventoryChangerPanel(hdc);
    SetViewportOrgEx(hdc, oldOrigin.x, oldOrigin.y, nullptr);
}

static void CasUiDrawInventoryScreen(HDC hdc, int width, int height)
{
    if (g_casUiInventoryView == 1)
        CasUiDrawBrowse(hdc, width, height);
    else if (g_casUiInventoryView == 2)
        CasUiDrawCategory(hdc, width, height);
    else if (g_casUiInventoryView == 3)
        CasUiDrawInventoryDetail(hdc, width, height);
    else
        CasUiDrawInventoryHome(hdc, width, height);
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
    if (g_casUiInventoryView == 3)
    {
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 0;
            return true;
        }
        // The detail screen contains the existing editor shifted by +150,+48.
        // Returning false lets MenuWindowProc translate the click and reuse all
        // existing editor hit-testing while the controls are migrated.
        return false;
    }

    if (g_casUiInventoryView == 0)
    {
        if (CasUiPointInRect(mouseX, mouseY, 804, 66, 132, 34) ||
            CasUiPointInRect(mouseX, mouseY,
                CAS_UI_CONTENT_X + 28, 270, 140, 34))
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
            const int x = CAS_UI_CONTENT_X +
                column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y +
                row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (CasUiPointInRect(mouseX, mouseY, x, y,
                CAS_UI_CARD_W, CAS_UI_CARD_H))
                return CasUiSelectInventoryCard(i);
        }
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X, height - 58, 78, 30))
        {
            if (g_casUiInventoryPage > 0) --g_casUiInventoryPage;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X + 86, height - 58, 78, 30))
        {
            ++g_casUiInventoryPage;
            return true;
        }
        return true;
    }

    if (g_casUiInventoryView == 1)
    {
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X, 66, 86, 30))
        {
            g_casUiInventoryView = 0;
            return true;
        }
        for (int i = 0; i < 10; ++i)
        {
            const int column = i % CAS_UI_GRID_COLUMNS;
            const int row = i / CAS_UI_GRID_COLUMNS;
            const int x = CAS_UI_CONTENT_X +
                column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y +
                row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (CasUiPointInRect(mouseX, mouseY, x, y,
                CAS_UI_CARD_W, CAS_UI_CARD_H))
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
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X, 66, 86, 30))
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
            const int x = CAS_UI_CONTENT_X +
                column * (CAS_UI_CARD_W + CAS_UI_CARD_GAP_X);
            const int y = CAS_UI_GRID_Y +
                row * (CAS_UI_CARD_H + CAS_UI_CARD_GAP_Y);
            if (!CasUiPointInRect(mouseX, mouseY, x, y,
                CAS_UI_CARD_W, CAS_UI_CARD_H))
                continue;
            int kind = 0;
            int source = 0;
            const wchar_t* name = nullptr;
            if (CasUiResolveCatalogItem(g_casUiBrowseCategory,
                first + i, &kind, &source, &name))
                CasUiAddSpecificItem(kind, source);
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X, height - 58, 78, 30))
        {
            if (g_casUiCatalogPage > 0) --g_casUiCatalogPage;
            return true;
        }
        if (CasUiPointInRect(mouseX, mouseY,
            CAS_UI_CONTENT_X + 86, height - 58, 78, 30))
        {
            ++g_casUiCatalogPage;
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
        {
            InvalidateRect(wnd, nullptr, FALSE);
            return true;
        }
        // Detail editor: map the new content coordinates back to the legacy
        // panel's local coordinate system for its existing click handlers.
        *mouseX -= 150;
        *mouseY -= 48;
        return false;
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
