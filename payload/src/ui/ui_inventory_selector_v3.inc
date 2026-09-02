// Verified local-catalog selector. Inject LAST, after legacy UI transformations.
// All editable state belongs to the menu thread. Browsing never mutates items.
// Selection uses definition/paint IDs, never a row ordinal that can change under a filter.

constexpr int CAS_SELECTOR_ROWS = 10;
constexpr int CAS_SELECTOR_ROW_H = 34;
constexpr int CAS_SELECTOR_LIST_Y = 166;
constexpr int CAS_SELECTOR_MAX_MODELS = 1024;
static const COLORREF CAS_SELECTOR_SECONDARY = RGB_COLOR(179, 186, 201);
static const COLORREF CAS_SELECTOR_FOCUS = RGB_COLOR(138, 180, 255);
static const COLORREF CAS_SELECTOR_SELECTED = RGB_COLOR(38, 59, 89);
static const COLORREF CAS_SELECTOR_ACTION = RGB_COLOR(35, 88, 173);

enum CasSelectorFocus {
    CAS_SEL_CATEGORY, CAS_SEL_ITEM_SEARCH, CAS_SEL_ITEMS, CAS_SEL_FINISH_SEARCH,
    CAS_SEL_FINISHES, CAS_SEL_WEAR, CAS_SEL_SEED, CAS_SEL_TRACKING,
    CAS_SEL_COUNTER, CAS_SEL_TEAM, CAS_SEL_SUBMIT, CAS_SEL_ENABLED, CAS_SEL_MY_ITEMS,
    CAS_SEL_RETRY, CAS_SEL_FOCUS_COUNT
};

static int g_casSelectorCategory = 0; // weapons, knives, gloves, agents
static int g_casSelectorFocus = CAS_SEL_ITEMS;
static unsigned short g_casSelectorDefinition = 0;
static int g_casSelectorPaint = -1;
static unsigned long long g_casSelectorEditId = 0;
static wchar_t g_casSelectorItemSearch[64]{};
static wchar_t g_casSelectorFinishSearch[64]{};
static wchar_t g_casSelectorWear[16] = L"0.0001";
static wchar_t g_casSelectorSeed[16] = L"0";
static wchar_t g_casSelectorCounter[16] = L"0";
static bool g_casSelectorTracking = false;
static BYTE g_casSelectorTeams = INVENTORY_TEAM_NONE;
static bool g_casSelectorSelectAll = false;
static bool g_casSelectorListsDirty = true;
static unsigned int g_casSelectorCatalogCount = 0;
static int g_casSelectorModelRows[CAS_SELECTOR_MAX_MODELS]{};
static int g_casSelectorModelCount = 0;
static int g_casSelectorFinishRows[INVENTORY_GAME_CATALOG_MAX_RECORDS]{};
static int g_casSelectorFinishCount = 0;
static int g_casSelectorModelScroll = 0;
static int g_casSelectorFinishScroll = 0;
static const wchar_t* g_casSelectorNotice = nullptr;

static wchar_t CasSelectorFold(wchar_t value)
{
    if (value >= L'A' && value <= L'Z') return value + (L'a' - L'A');
    if (value >= 0x0410 && value <= 0x042f) return value + 0x20;
    if (value == 0x0401) return 0x0451;
    return value;
}

static bool CasSelectorContains(const wchar_t* text, const wchar_t* query)
{
    if (!query || !query[0]) return true;
    if (!text) return false;
    for (int start = 0; text[start]; ++start)
    {
        int offset = 0;
        while (query[offset] && text[start + offset] &&
            CasSelectorFold(text[start + offset]) == CasSelectorFold(query[offset])) ++offset;
        if (!query[offset]) return true;
    }
    return false;
}

static int CasSelectorCompare(const wchar_t* a, const wchar_t* b)
{
    for (int i = 0;; ++i)
    {
        const wchar_t left = CasSelectorFold(a[i]), right = CasSelectorFold(b[i]);
        if (left != right) return left < right ? -1 : 1;
        if (!left) return 0;
    }
}

static void CasSelectorCopy(wchar_t* out, int capacity, const wchar_t* text)
{
    int i = 0;
    if (!out || capacity <= 0) return;
    while (text && text[i] && i + 1 < capacity) { out[i] = text[i]; ++i; }
    out[i] = 0;
}

static void CasSelectorNumber(wchar_t* out, unsigned int number)
{
    wchar_t reverse[12]{};
    int count = 0;
    do { reverse[count++] = static_cast<wchar_t>(L'0' + number % 10); number /= 10; } while (number);
    for (int i = 0; i < count; ++i) out[i] = reverse[count - i - 1];
    out[count] = 0;
}

static bool CasSelectorParseNumber(const wchar_t* text, int maximum, int* result)
{
    if (!text || !text[0] || !result) return false;
    int value = 0;
    for (int i = 0; text[i]; ++i)
    {
        if (text[i] < L'0' || text[i] > L'9') return false;
        const int digit = text[i] - L'0';
        if (value > (maximum - digit) / 10 || digit > maximum) return false;
        value = value * 10 + digit;
    }
    *result = value;
    return true;
}

static bool CasSelectorParseWear(const wchar_t* text, float* result)
{
    if (!text || !text[0] || !result) return false;
    bool decimal = false, digitSeen = false;
    float value = 0.0f, divisor = 1.0f;
    for (int i = 0; text[i]; ++i)
    {
        if ((text[i] == L'.' || text[i] == L',') && !decimal) { decimal = true; continue; }
        if (text[i] < L'0' || text[i] > L'9') return false;
        digitSeen = true;
        if (decimal) { divisor *= 10.0f; value += (text[i] - L'0') / divisor; }
        else value = value * 10.0f + (text[i] - L'0');
        if (value > 1.0f) return false;
    }
    if (!digitSeen) return false;
    *result = value;
    return true;
}

static const wchar_t* CasSelectorWeaponName(const InventoryGameCatalogRecord* record)
{
    return record ? (record->weaponName[0] ? record->weaponName : record->displayName) : L"Choose an item";
}

static const wchar_t* CasSelectorFinishName(const InventoryGameCatalogRecord* record)
{
    if (!record) return L"Choose a finish";
    if (record->paintKit == 0) return record->category == 9 ? L"Original appearance" : L"Default / no finish";
    return record->finishName[0] ? record->finishName : record->displayName;
}

static bool CasSelectorRarityEquals(const char* token, const char* expected)
{
    if (!token || !expected) return false;
    for (int i = 0;; ++i)
    {
        if (token[i] != expected[i]) return false;
        if (!token[i]) return true;
    }
}

static COLORREF CasSelectorRarityColor(const InventoryGameCatalogRecord* record)
{
    const char* rarity = InventoryGameCatalogRarity(record);
    // Values are the exact Source 2 paint_kits rarity tokens.  Unknown or
    // absent values stay neutral; paint-kit IDs must never be used as colors.
    if (CasSelectorRarityEquals(rarity, "rarity_common")) return RGB_COLOR(176, 195, 217);
    if (CasSelectorRarityEquals(rarity, "rarity_uncommon")) return RGB_COLOR(94, 152, 217);
    if (CasSelectorRarityEquals(rarity, "rarity_rare")) return RGB_COLOR(75, 105, 255);
    if (CasSelectorRarityEquals(rarity, "rarity_mythical")) return RGB_COLOR(136, 71, 255);
    if (CasSelectorRarityEquals(rarity, "rarity_legendary")) return RGB_COLOR(211, 44, 230);
    if (CasSelectorRarityEquals(rarity, "rarity_ancient")) return RGB_COLOR(235, 75, 75);
    if (CasSelectorRarityEquals(rarity, "rarity_immortal")) return RGB_COLOR(232, 187, 64);
    return RGB_COLOR(225, 227, 235);
}

static const wchar_t* CasSelectorRarityName(const InventoryGameCatalogRecord* record)
{
    const char* rarity = InventoryGameCatalogRarity(record);
    if (CasSelectorRarityEquals(rarity, "rarity_common")) return L"Consumer grade";
    if (CasSelectorRarityEquals(rarity, "rarity_uncommon")) return L"Industrial grade";
    if (CasSelectorRarityEquals(rarity, "rarity_rare")) return L"Mil-Spec grade";
    if (CasSelectorRarityEquals(rarity, "rarity_mythical")) return L"Restricted";
    if (CasSelectorRarityEquals(rarity, "rarity_legendary")) return L"Classified";
    if (CasSelectorRarityEquals(rarity, "rarity_ancient")) return L"Covert";
    if (CasSelectorRarityEquals(rarity, "rarity_immortal")) return L"Extraordinary";
    return record && record->paintKit == 0 ? L"Default" : L"Rarity unavailable";
}

static bool CasSelectorCategoryMatches(int category)
{
    if (g_casSelectorCategory == 0) return category >= 1 && category <= 6;
    return category == g_casSelectorCategory + 6;
}

static const InventoryGameCatalogRecord* CasSelectorSelectedRecord()
{
    if (!InventoryGameCatalogValidatePair(g_casSelectorDefinition, g_casSelectorPaint)) return nullptr;
    return InventoryGameCatalogFindPair(g_casSelectorDefinition, g_casSelectorPaint);
}

static void CasSelectorRebuildLists()
{
    const unsigned int count = InventoryGameCatalogTotalCount();
    if (count != g_casSelectorCatalogCount) g_casSelectorListsDirty = true;
    if (!g_casSelectorListsDirty) return;
    g_casSelectorCatalogCount = count;
    g_casSelectorListsDirty = false;
    g_casSelectorModelCount = g_casSelectorFinishCount = 0;
    for (unsigned int i = 0; i < count; ++i)
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(i);
        if (!record) continue;
        if (record->definitionIndex == g_casSelectorDefinition &&
            CasSelectorContains(CasSelectorFinishName(record), g_casSelectorFinishSearch) &&
            g_casSelectorFinishCount < static_cast<int>(INVENTORY_GAME_CATALOG_MAX_RECORDS))
            g_casSelectorFinishRows[g_casSelectorFinishCount++] = static_cast<int>(i);
        if (!CasSelectorCategoryMatches(record->category) ||
            !CasSelectorContains(CasSelectorWeaponName(record), g_casSelectorItemSearch)) continue;
        bool duplicate = false;
        for (int j = 0; j < g_casSelectorModelCount; ++j)
        {
            const InventoryGameCatalogRecord* existing = InventoryGameCatalogAt(g_casSelectorModelRows[j]);
            if (existing && existing->definitionIndex == record->definitionIndex) { duplicate = true; break; }
        }
        if (!duplicate && g_casSelectorModelCount < CAS_SELECTOR_MAX_MODELS)
            g_casSelectorModelRows[g_casSelectorModelCount++] = static_cast<int>(i);
    }
    // Small stable item list; compatible finish rows preserve the catalog's order,
    // with an explicit default row first. There is never a synthetic default.
    for (int i = 1; i < g_casSelectorModelCount; ++i)
    {
        const int candidate = g_casSelectorModelRows[i];
        int j = i;
        while (j > 0 && CasSelectorCompare(CasSelectorWeaponName(InventoryGameCatalogAt(candidate)),
            CasSelectorWeaponName(InventoryGameCatalogAt(g_casSelectorModelRows[j - 1]))) < 0)
        { g_casSelectorModelRows[j] = g_casSelectorModelRows[j - 1]; --j; }
        g_casSelectorModelRows[j] = candidate;
    }
    for (int i = 0; i < g_casSelectorFinishCount; ++i)
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(g_casSelectorFinishRows[i]);
        if (record && record->paintKit == 0)
        {
            const int row = g_casSelectorFinishRows[i];
            for (int j = i; j > 0; --j) g_casSelectorFinishRows[j] = g_casSelectorFinishRows[j - 1];
            g_casSelectorFinishRows[0] = row;
            break;
        }
    }
    g_casSelectorModelScroll = InventoryClampInt(g_casSelectorModelScroll, 0,
        g_casSelectorModelCount > CAS_SELECTOR_ROWS ? g_casSelectorModelCount - CAS_SELECTOR_ROWS : 0);
    g_casSelectorFinishScroll = InventoryClampInt(g_casSelectorFinishScroll, 0,
        g_casSelectorFinishCount > CAS_SELECTOR_ROWS ? g_casSelectorFinishCount - CAS_SELECTOR_ROWS : 0);
}

static int CasSelectorSelectedRow(bool finishes)
{
    const int count = finishes ? g_casSelectorFinishCount : g_casSelectorModelCount;
    const int* rows = finishes ? g_casSelectorFinishRows : g_casSelectorModelRows;
    for (int i = 0; i < count; ++i)
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(rows[i]);
        if (record && record->definitionIndex == g_casSelectorDefinition &&
            (!finishes || record->paintKit == g_casSelectorPaint)) return i;
    }
    return -1;
}

static void CasSelectorChooseRow(bool finishes, int row)
{
    const int count = finishes ? g_casSelectorFinishCount : g_casSelectorModelCount;
    if (row < 0 || row >= count || (!finishes && g_casSelectorEditId)) return;
    const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(
        finishes ? g_casSelectorFinishRows[row] : g_casSelectorModelRows[row]);
    if (!record) return;
    if (finishes)
    {
        if (record->definitionIndex != g_casSelectorDefinition ||
            !InventoryGameCatalogValidatePair(record->definitionIndex, record->paintKit)) return;
        g_casSelectorPaint = record->paintKit;
        if (record->paintKit == 0 || record->category >= 8) g_casSelectorTracking = false;
        if (record->paintKit == 0)
        {
            CasSelectorCopy(g_casSelectorWear, 16, L"0.0001");
            CasSelectorCopy(g_casSelectorSeed, 16, L"0");
        }
    }
    else if (g_casSelectorDefinition != record->definitionIndex)
    {
        g_casSelectorDefinition = record->definitionIndex;
        g_casSelectorPaint = -1; // an explicit choice is required after changing model
        g_casSelectorFinishSearch[0] = 0;
        g_casSelectorFinishScroll = 0;
        g_casSelectorTeams = INVENTORY_TEAM_NONE;
        g_casSelectorTracking = false;
        g_casSelectorListsDirty = true;
    }
    g_casSelectorNotice = nullptr;
}

static const wchar_t* CasSelectorValidation(float* wear = nullptr, int* seed = nullptr, int* counter = nullptr)
{
    if (!InventoryGameCatalogReady()) return L"Catalog unavailable. Retry after running the loader.";
    if (!g_casSelectorDefinition) return L"1. Choose an item, then a compatible finish.";
    if (CasSelectorSelectedRow(false) < 0) return L"Selected item is hidden. Clear the item search.";
    const InventoryGameCatalogRecord* record = CasSelectorSelectedRecord();
    if (!record) return L"2. Choose one of this item's verified finishes.";
    if (CasSelectorSelectedRow(true) < 0) return L"Selected finish is hidden. Clear the finish search.";
    float parsedWear = 0.0f;
    int parsedSeed = 0, parsedCounter = 0;
    const bool cosmetic = record->paintKit > 0 && record->category <= 8;
    const bool tracking = cosmetic && record->category < 8 && g_casSelectorTracking;
    if (cosmetic && !CasSelectorParseWear(g_casSelectorWear, &parsedWear))
        return L"Wear must be a number between 0 and 1.";
    if (cosmetic && !CasSelectorParseNumber(g_casSelectorSeed, 1000, &parsedSeed))
        return L"Seed must be a whole number from 0 to 1000.";
    if (tracking && !CasSelectorParseNumber(g_casSelectorCounter, 999999, &parsedCounter))
        return L"StatTrak counter must be between 0 and 999999.";
    if (wear) *wear = parsedWear;
    if (seed) *seed = parsedSeed;
    if (counter) *counter = tracking ? parsedCounter : -1;
    return nullptr;
}

static bool CasSelectorSubmit()
{
    CasSelectorRebuildLists();
    float wear = 0.0f;
    int seed = 0, counter = -1;
    if (const wchar_t* error = CasSelectorValidation(&wear, &seed, &counter))
    { g_casSelectorNotice = error; return false; }
    const InventoryGameCatalogRecord* record = CasSelectorSelectedRecord();
    if (!record) return false;
    if (!InventoryTryLock()) { g_casSelectorNotice = L"Inventory busy. Your selection is kept; try again."; return false; }
    int index = -1;
    if (g_casSelectorEditId)
    {
        for (int i = 0; i < g_inventoryStore.itemCount; ++i)
            if (g_inventoryStore.items[i].itemId == g_casSelectorEditId) { index = i; break; }
        if (index < 0 || g_inventoryStore.items[index].overrideDefinitionIndex != g_casSelectorDefinition)
        { InventoryUnlock(); g_casSelectorNotice = L"Item changed or was removed. Reopen it from My items."; return false; }
    }
    else if (g_inventoryStore.itemCount >= INVENTORY_MAX_ITEMS)
    { InventoryUnlock(); g_casSelectorNotice = L"Inventory is full. Remove an item in My items first."; return false; }
    VirtualInventoryItem item{};
    if (index >= 0) item = g_inventoryStore.items[index];
    else
    {
        item.slotDefinitionIndex = record->category == 7 ? INVENTORY_SLOT_KNIFE :
            record->category == 8 ? INVENTORY_SLOT_GLOVE :
            record->category == 9 ? INVENTORY_SLOT_AGENT : record->definitionIndex;
        item.overrideDefinitionIndex = record->definitionIndex;
        item.quality = record->category == 7 || record->category == 8 ? 3 : record->category == 9 ? 4 : 0;
    }
    item.paintKit = record->paintKit;
    item.seed = seed;
    item.wear = wear;
    item.statTrak = record->category >= 8 || record->paintKit == 0 ? -1 : counter;
    // Adding is deliberately separate from equipping. The new virtual item is
    // available in My items immediately, while the user chooses its loadout
    // later through the item's own T/CT controls.
    item.equippedTeams = INVENTORY_TEAM_NONE;
    if (!InventoryGameCatalogValidateItem(item))
    { InventoryUnlock(); g_casSelectorNotice = L"Catalog rejected this combination. Choose another finish."; return false; }
    if (index < 0)
    {
        item.itemId = g_inventoryStore.nextItemId++;
        index = g_inventoryStore.itemCount++;
    }
    g_inventoryStore.items[index] = item;
    g_inventoryUiSelected = index;
    g_inventoryUiPage = index / 7;
    MarkInventoryDirty();
    InventoryUnlock();
    // The selector is a creation flow, not an implicit loadout action. Open
    // the persisted item directly so all subsequent actions happen in My items.
    g_casUiSkinchangerSubtab = 3;
    g_casUiInventoryView = 3;
    g_casSelectorEditId = 0;
    g_casSelectorPaint = -1;
    g_casSelectorNotice = L"Added to local inventory.";
    return true;
}

static void CasSelectorSetFocus(int focus)
{
    g_casSelectorFocus = focus;
    g_casSelectorSelectAll = focus == CAS_SEL_WEAR || focus == CAS_SEL_SEED || focus == CAS_SEL_COUNTER;
    AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NONE);
}

static void CasSelectorOpen(int category)
{
    g_casSelectorCategory = category;
    g_casUiSkinchangerSubtab = 2;
    g_casUiInventoryView = 0;
    g_casSelectorEditId = 0;
    g_casSelectorDefinition = 0;
    g_casSelectorPaint = -1;
    g_casSelectorTeams = INVENTORY_TEAM_NONE;
    g_casSelectorTracking = false;
    CasSelectorCopy(g_casSelectorWear, 16, L"0.0001");
    CasSelectorCopy(g_casSelectorSeed, 16, L"0");
    CasSelectorCopy(g_casSelectorCounter, 16, L"0");
    g_casSelectorItemSearch[0] = g_casSelectorFinishSearch[0] = 0;
    g_casSelectorModelScroll = g_casSelectorFinishScroll = 0;
    g_casSelectorListsDirty = true;
    g_casSelectorNotice = nullptr;
    CasSelectorSetFocus(CAS_SEL_ITEMS);
}

static void CasSelectorEditSelected()
{
    InventoryUiSnapshot snapshot{};
    if (!InventoryGetFilteredUiSnapshot(&snapshot) || !snapshot.hasSelectedItem) return;
    const VirtualInventoryItem& item = snapshot.selectedItem;
    const InventoryGameCatalogRecord* first = InventoryGameCatalogFindFirstDefinition(item.overrideDefinitionIndex);
    if (!first || item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC) return;
    CasSelectorOpen(first->category >= 7 ? first->category - 6 : 0);
    g_casSelectorEditId = item.itemId;
    g_casSelectorDefinition = item.overrideDefinitionIndex;
    g_casSelectorPaint = item.paintKit;
    g_casSelectorTracking = item.statTrak >= 0;
    g_casSelectorTeams = static_cast<BYTE>(item.equippedTeams & first->teamMask);
    CasSelectorNumber(g_casSelectorSeed, static_cast<unsigned int>(item.seed < 0 ? 0 : item.seed));
    CasSelectorNumber(g_casSelectorCounter, item.statTrak < 0 ? 0 : item.statTrak);
    const int scaled = InventoryClampInt(static_cast<int>(item.wear * 10000.0f + 0.5f), 0, 10000);
    g_casSelectorWear[0] = scaled == 10000 ? L'1' : L'0'; g_casSelectorWear[1] = L'.';
    int remainder = scaled % 10000;
    for (int i = 5; i >= 2; --i) { g_casSelectorWear[i] = L'0' + remainder % 10; remainder /= 10; }
    g_casSelectorWear[6] = 0;
    CasSelectorSetFocus(CAS_SEL_FINISHES);
    CasSelectorRebuildLists();
    const int selected = CasSelectorSelectedRow(true);
    if (selected >= CAS_SELECTOR_ROWS) g_casSelectorFinishScroll = selected - CAS_SELECTOR_ROWS + 1;
}

static void CasSelectorButton(HDC hdc, int x, int y, int w, const wchar_t* text,
    bool selected = false, bool enabled = true, bool focused = false, bool primary = false)
{
    const bool hover = enabled && CasUiMouseOver(x, y, w, 32);
    COLORREF bg = selected ? CAS_SELECTOR_SELECTED : CAS_UI_SURFACE_3;
    if (primary && enabled) bg = CAS_SELECTOR_ACTION;
    if (hover) bg = selected || primary ? RGB_COLOR(48, 85, 139) : RGB_COLOR(49, 53, 67);
    DrawRoundedCard(hdc, x, y, w, 32, bg,
        focused ? CAS_SELECTOR_FOCUS : selected ? RGB_COLOR(100, 139, 194) : CAS_UI_BORDER_HI, 6);
    CasUiDrawLabel(hdc, text, x + 4, y, w - 8, 32,
        enabled ? CAS_UI_TEXT : CAS_SELECTOR_SECONDARY, 13, selected ? 600 : 400, DT_CENTER);
}

static void CasSelectorField(HDC hdc, int x, int y, int w, const wchar_t* value,
    const wchar_t* placeholder, int focus, bool enabled = true)
{
    const bool focused = enabled && g_casSelectorFocus == focus;
    DrawRoundedCard(hdc, x, y, w, 32, focused ? CAS_UI_SURFACE_3 : CAS_UI_SURFACE,
        focused ? CAS_SELECTOR_FOCUS : CAS_UI_BORDER_HI, 5);
    if (focused && g_casSelectorSelectAll && value[0])
        CasUiDrawRect(hdc, x + 6, y + 5, x + w - 6, y + 27, CAS_SELECTOR_SELECTED);
    CasUiDrawLabel(hdc, value[0] ? value : placeholder, x + 9, y, w - 18, 32,
        value[0] && enabled ? CAS_UI_TEXT : CAS_SELECTOR_SECONDARY, 13, 400, DT_LEFT);
    if (focused) CasUiDrawRect(hdc, x + w - 7, y + 8, x + w - 6, y + 24, CAS_SELECTOR_FOCUS);
}

// -1 means the inventory mutex was busy. The menu never guesses a disabled
// state while the worker owns it, because that makes a second click invert an
// unknown value.
static int CasSelectorEnabledState()
{
    if (!InventoryTryLock()) return -1;
    const int state = g_inventoryStore.enabled ? 1 : 0;
    InventoryUnlock();
    return state;
}

static bool CasSelectorToggleEnabled()
{
    if (!InventoryTryLock())
    {
        g_casSelectorNotice = L"Inventory busy. Changer state was not modified; try again.";
        return false;
    }
    g_inventoryStore.enabled = !g_inventoryStore.enabled;
    const bool enabled = g_inventoryStore.enabled;
    MarkInventoryDirty();
    InventoryUnlock();
    g_casSelectorNotice = enabled ? L"Inventory changer enabled." : L"Inventory changer disabled; local items are preserved.";
    return true;
}

static void CasSelectorDrawEnabled(HDC hdc)
{
    const int state = CasSelectorEnabledState();
    CasSelectorButton(hdc, 668, 20, 138,
        state < 0 ? L"Changer: busy" : state ? L"Changer: on" : L"Changer: off",
        state > 0, state >= 0, g_casSelectorFocus == CAS_SEL_ENABLED);
}

static void CasSelectorText(HDC hdc, const wchar_t* text, int x, int y, int w, int h,
    COLORREF color = CAS_SELECTOR_SECONDARY, int size = 13)
{
    HFONT font = CasUiCreateFont(size, 400);
    HGDIOBJ old = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, color);
    RECT rc = { x, y, x + w, y + h };
    DrawTextW(hdc, text ? text : L"", -1, &rc, DT_LEFT | 0x0010 /* DT_WORDBREAK */ | 0x0800 /* DT_NOPREFIX */);
    SelectObject(hdc, old); DeleteObject(font);
}

static void CasSelectorDrawList(HDC hdc, bool finishes, int x, int w)
{
    const int count = finishes ? g_casSelectorFinishCount : g_casSelectorModelCount;
    const int scroll = finishes ? g_casSelectorFinishScroll : g_casSelectorModelScroll;
    const int* rows = finishes ? g_casSelectorFinishRows : g_casSelectorModelRows;
    for (int i = 0; i < CAS_SELECTOR_ROWS && scroll + i < count; ++i)
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(rows[scroll + i]);
        if (!record) continue;
        const bool selected = record->definitionIndex == g_casSelectorDefinition &&
            (!finishes || record->paintKit == g_casSelectorPaint);
        const int y = CAS_SELECTOR_LIST_Y + i * CAS_SELECTOR_ROW_H;
        const bool focus = selected && g_casSelectorFocus == (finishes ? CAS_SEL_FINISHES : CAS_SEL_ITEMS);
        const bool hover = CasUiMouseOver(x, y, w, 32);
        DrawRoundedCard(hdc, x, y, w, 32,
            selected ? CAS_SELECTOR_SELECTED : hover ? CAS_UI_SURFACE_3 : CAS_UI_SURFACE,
            focus ? CAS_SELECTOR_FOCUS : selected ? RGB_COLOR(100, 139, 194) : CAS_UI_SURFACE, 4);
        if (finishes)
            CasUiDrawRect(hdc, x + 2, y + 4, x + 5, y + 28, CasSelectorRarityColor(record));
        if (selected) CasUiDrawCheckmark(hdc, x + 7, y + 10, CAS_UI_TEXT);
        const wchar_t* name = finishes ? CasSelectorFinishName(record) : CasSelectorWeaponName(record);
        CasUiDrawLabel(hdc, name, x + 25, y, w - 31, 32,
            finishes ? CasSelectorRarityColor(record) : CAS_UI_TEXT, 13,
            selected ? 600 : 400, DT_LEFT | 0x8000 /* DT_END_ELLIPSIS */);
        CasUiTooltipCandidate(x, y, w, 32, name, 8);
    }
    if (!count)
        CasSelectorText(hdc, finishes ? (!g_casSelectorDefinition ? L"Select an item on the left." : L"No matching finishes. Clear the search.") :
            L"No matching items. Clear the search or select another category.", x + 12, CAS_SELECTOR_LIST_Y + 22, w - 24, 90);
    wchar_t countText[16]{}; CasSelectorNumber(countText, static_cast<unsigned int>(count));
    CasUiDrawLabel(hdc, countText, x + 72, 523, 46, 32, CAS_SELECTOR_SECONDARY, 13, 400, DT_CENTER);
    CasSelectorButton(hdc, x, 523, 62, L"Prev", false, scroll > 0);
    CasSelectorButton(hdc, x + w - 62, 523, 62, L"Next", false, scroll + CAS_SELECTOR_ROWS < count);
}

static void CasSelectorDraw(HDC hdc, int width, int height)
{
    CasSelectorRebuildLists();
    AtomicExchange(&g_casSelectorInputActive, 1);
    CasUiDrawRect(hdc, 162, 8, width - 8, height - 8, CAS_UI_BG);
    CasUiDrawLabel(hdc, g_casSelectorEditId ? L"Change finish" : L"Add to local inventory", 174, 20, 480, 30, CAS_UI_TEXT, 21, 600);
    CasSelectorDrawEnabled(hdc);
    CasSelectorButton(hdc, 818, 20, 128, L"My items", false, true, g_casSelectorFocus == CAS_SEL_MY_ITEMS);
    const wchar_t* categories[] = { L"Weapons", L"Knives", L"Gloves", L"Agents" };
    for (int i = 0; i < 4; ++i)
        CasSelectorButton(hdc, 174 + i * 104, 66, 96, categories[i], g_casSelectorCategory == i,
            !g_casSelectorEditId,
            g_casSelectorFocus == CAS_SEL_CATEGORY && g_casSelectorCategory == i);
    CasUiDrawLabel(hdc, L"Verified installed-game catalog", 602, 66, 232, 32, CAS_SELECTOR_SECONDARY, 13);
    CasSelectorButton(hdc, 842, 66, 104, L"Retry catalog", false, !InventoryGameCatalogReady(), g_casSelectorFocus == CAS_SEL_RETRY);
    if (!InventoryGameCatalogReady())
    {
        const bool attempted = InventoryGameCatalogLoadWasAttempted();
        DrawRoundedCard(hdc, 174, 116, 772, 438, CAS_UI_SURFACE, CAS_UI_BORDER_HI, 8);
        CasUiDrawLabel(hdc, attempted ? L"Catalog is unavailable" : L"Loading the game catalog", 206, 152, 690, 32, CAS_UI_TEXT, 22, 600);
        CasSelectorText(hdc, attempted ? L"No skins are offered until their item and finish pairing can be verified. Run the loader with your installed game files, then choose Retry catalog." :
            L"The worker is reading the snapshot prepared by the loader. No placeholder skins will be added.", 206, 208, 610, 90, CAS_SELECTOR_SECONDARY, 16);
        wchar_t status[16]{}; CasSelectorNumber(status, InventoryGameCatalogLastLoadStatus());
        CasUiDrawLabel(hdc, L"Catalog status code:", 206, 316, 180, 26, CAS_SELECTOR_SECONDARY, 13);
        CasUiDrawLabel(hdc, status, 388, 316, 70, 26, CAS_UI_TEXT, 13);
        if (g_casSelectorNotice) CasSelectorText(hdc, g_casSelectorNotice, 206, 364, 610, 54);
        return;
    }
    CasUiDrawLabel(hdc, g_casSelectorEditId ? L"1. Item (locked while editing)" : L"1. Item", 174, 106, 196, 24, CAS_SELECTOR_SECONDARY, 13, 600);
    CasUiDrawLabel(hdc, L"2. Compatible finish", 382, 106, 280, 24, CAS_SELECTOR_SECONDARY, 13, 600);
    CasSelectorField(hdc, 174, 130, 156, g_casSelectorItemSearch, L"Search items", CAS_SEL_ITEM_SEARCH, !g_casSelectorEditId);
    CasSelectorButton(hdc, 336, 130, 34, L"X", false, g_casSelectorItemSearch[0] != 0);
    CasSelectorField(hdc, 382, 130, 238, g_casSelectorFinishSearch, L"Search this item's finishes", CAS_SEL_FINISH_SEARCH);
    CasSelectorButton(hdc, 626, 130, 34, L"X", false, g_casSelectorFinishSearch[0] != 0);
    CasSelectorDrawList(hdc, false, 174, 196);
    CasSelectorDrawList(hdc, true, 382, 278);
    DrawRoundedCard(hdc, 672, 110, 274, 445, CAS_UI_SURFACE, CAS_UI_BORDER_HI, 7);
    const InventoryGameCatalogRecord* record = CasSelectorSelectedRecord();
    const InventoryGameCatalogRecord* model = record ? record : InventoryGameCatalogFindFirstDefinition(g_casSelectorDefinition);
    CasUiDrawLabel(hdc, L"3. Review and add", 686, 119, 246, 24, CAS_SELECTOR_SECONDARY, 13, 600);
    CasSelectorText(hdc, CasSelectorWeaponName(model), 686, 149, 246, 40, CAS_UI_TEXT, 17);
    CasSelectorText(hdc, CasSelectorFinishName(record), 686, 190, 246, 42,
        CasSelectorRarityColor(record), 14);
    // This is a data preview of the exact selected game resource.  The GDI
    // overlay cannot decode Source 2 .vtex_c images, so it exposes the model
    // and Panorama icon provenance rather than displaying fabricated artwork.
    DrawRoundedCard(hdc, 686, 234, 246, 52, CAS_UI_SURFACE_2,
        CasSelectorRarityColor(record), 5);
    CasUiDrawLabel(hdc, L"Installed-game weapon preview", 696, 240, 224, 16,
        CAS_UI_TEXT, 11, 600, DT_LEFT);
    CasUiDrawLabel(hdc, CasSelectorRarityName(record), 696, 258, 224, 18,
        CasSelectorRarityColor(record), 12, 600, DT_LEFT);
    const bool cosmetic = record && record->paintKit > 0 && record->category <= 8;
    CasUiDrawLabel(hdc, L"Wear (0 - 1)", 686, 296, 118, 25, CAS_SELECTOR_SECONDARY, 13);
    CasSelectorField(hdc, 810, 292, 122, g_casSelectorWear, L"0.0001", CAS_SEL_WEAR, cosmetic);
    const wchar_t* wearLabels[] = { L"FN", L"MW", L"FT", L"WW", L"BS" };
    for (int i = 0; i < 5; ++i) CasSelectorButton(hdc, 686 + i * 50, 332, 46, wearLabels[i], false, cosmetic);
    CasUiDrawLabel(hdc, L"Seed (0 - 1000)", 686, 377, 140, 28, CAS_SELECTOR_SECONDARY, 13);
    CasSelectorField(hdc, 838, 373, 94, g_casSelectorSeed, L"0", CAS_SEL_SEED, cosmetic);
    const bool tracking = cosmetic && record->category < 8;
    CasSelectorButton(hdc, 686, 414, 136, g_casSelectorTracking ? L"StatTrak: on" : L"StatTrak: off", g_casSelectorTracking,
        tracking, g_casSelectorFocus == CAS_SEL_TRACKING);
    CasSelectorField(hdc, 830, 414, 102, g_casSelectorCounter, L"0", CAS_SEL_COUNTER, tracking && g_casSelectorTracking);
    CasUiDrawLabel(hdc, L"Add first; equip later in My items", 686, 454, 246, 24,
        CAS_SELECTOR_SECONDARY, 13);
    const BYTE availability = model ? model->teamMask : INVENTORY_TEAM_NONE;
    CasSelectorButton(hdc, 686, 480, 118,
        (availability & INVENTORY_TEAM_T) ? L"T supported" : L"T unavailable", false, false);
    CasSelectorButton(hdc, 814, 480, 118,
        (availability & INVENTORY_TEAM_CT) ? L"CT supported" : L"CT unavailable", false, false);
    const wchar_t* error = CasSelectorValidation();
    bool full = false;
    if (InventoryTryLock()) { full = !g_casSelectorEditId && g_inventoryStore.itemCount >= INVENTORY_MAX_ITEMS; InventoryUnlock(); }
    CasSelectorButton(hdc, 686, 520, 246, full ? L"Inventory full" : g_casSelectorEditId ? L"Save changes" : L"Add item",
        false, !error && !full, g_casSelectorFocus == CAS_SEL_SUBMIT, true);
    CasSelectorText(hdc, g_casSelectorNotice ? g_casSelectorNotice : error ? error : L"Ready. Only Add item / Save changes modifies your local inventory.",
        174, 568, 772, 36, CAS_SELECTOR_SECONDARY, 13);
}

static bool CasSelectorClick(int x, int y)
{
    CasSelectorRebuildLists();
    if (CasUiPointInRect(x, y, 668, 20, 138, 32))
    { CasSelectorSetFocus(CAS_SEL_ENABLED); CasSelectorToggleEnabled(); return true; }
    if (CasUiPointInRect(x, y, 818, 20, 128, 32))
    { g_casUiSkinchangerSubtab = 3; g_casUiInventoryView = 0; g_casSelectorEditId = 0; AtomicExchange(&g_casSelectorInputActive, 0); return true; }
    for (int i = 0; i < 4; ++i)
        if (!g_casSelectorEditId && CasUiPointInRect(x, y, 174 + i * 104, 66, 96, 32))
        { CasSelectorOpen(i); return true; }
    if (CasUiPointInRect(x, y, 842, 66, 104, 32))
    {
        if (!InventoryGameCatalogReady())
        { ReloadInventoryGameCatalog(); g_casSelectorNotice = L"Retry requested. The worker will read the catalog snapshot."; }
        return true;
    }
    if (!InventoryGameCatalogReady()) return true;
    if (CasUiPointInRect(x, y, 174, 130, 156, 32) && !g_casSelectorEditId) CasSelectorSetFocus(CAS_SEL_ITEM_SEARCH);
    else if (CasUiPointInRect(x, y, 382, 130, 238, 32)) CasSelectorSetFocus(CAS_SEL_FINISH_SEARCH);
    else if (CasUiPointInRect(x, y, 336, 130, 34, 32))
    { g_casSelectorItemSearch[0] = 0; g_casSelectorListsDirty = true; CasSelectorSetFocus(CAS_SEL_ITEM_SEARCH); }
    else if (CasUiPointInRect(x, y, 626, 130, 34, 32))
    { g_casSelectorFinishSearch[0] = 0; g_casSelectorListsDirty = true; CasSelectorSetFocus(CAS_SEL_FINISH_SEARCH); }
    else if (CasUiPointInRect(x, y, 174, CAS_SELECTOR_LIST_Y, 196, CAS_SELECTOR_ROWS * CAS_SELECTOR_ROW_H))
    { CasSelectorSetFocus(CAS_SEL_ITEMS); CasSelectorChooseRow(false, g_casSelectorModelScroll + (y - CAS_SELECTOR_LIST_Y) / CAS_SELECTOR_ROW_H); }
    else if (CasUiPointInRect(x, y, 382, CAS_SELECTOR_LIST_Y, 278, CAS_SELECTOR_ROWS * CAS_SELECTOR_ROW_H))
    { CasSelectorSetFocus(CAS_SEL_FINISHES); CasSelectorChooseRow(true, g_casSelectorFinishScroll + (y - CAS_SELECTOR_LIST_Y) / CAS_SELECTOR_ROW_H); }
    else if (CasUiPointInRect(x, y, 174, 523, 62, 32)) { g_casSelectorModelScroll -= CAS_SELECTOR_ROWS; g_casSelectorListsDirty = true; }
    else if (CasUiPointInRect(x, y, 308, 523, 62, 32)) { g_casSelectorModelScroll += CAS_SELECTOR_ROWS; g_casSelectorListsDirty = true; }
    else if (CasUiPointInRect(x, y, 382, 523, 62, 32)) { g_casSelectorFinishScroll -= CAS_SELECTOR_ROWS; g_casSelectorListsDirty = true; }
    else if (CasUiPointInRect(x, y, 598, 523, 62, 32)) { g_casSelectorFinishScroll += CAS_SELECTOR_ROWS; g_casSelectorListsDirty = true; }
    else
    {
        const InventoryGameCatalogRecord* record = CasSelectorSelectedRecord();
        const bool cosmetic = record && record->paintKit > 0 && record->category <= 8;
        if (cosmetic && CasUiPointInRect(x, y, 810, 292, 122, 32)) CasSelectorSetFocus(CAS_SEL_WEAR);
        else if (cosmetic && CasUiPointInRect(x, y, 838, 373, 94, 32)) CasSelectorSetFocus(CAS_SEL_SEED);
        else if (cosmetic && record->category < 8 && CasUiPointInRect(x, y, 686, 414, 136, 32))
        { CasSelectorSetFocus(CAS_SEL_TRACKING); g_casSelectorTracking = !g_casSelectorTracking; }
        else if (cosmetic && record->category < 8 && g_casSelectorTracking && CasUiPointInRect(x, y, 830, 414, 102, 32)) CasSelectorSetFocus(CAS_SEL_COUNTER);
        else if (CasUiPointInRect(x, y, 686, 520, 246, 32)) { CasSelectorSetFocus(CAS_SEL_SUBMIT); CasSelectorSubmit(); }
        for (int i = 0; i < 5 && cosmetic; ++i)
            if (CasUiPointInRect(x, y, 686 + i * 50, 332, 46, 32))
            {
                const wchar_t* values[] = { L"0.0001", L"0.08", L"0.20", L"0.40", L"0.55" };
                CasSelectorCopy(g_casSelectorWear, 16, values[i]); CasSelectorSetFocus(CAS_SEL_WEAR);
            }
        const InventoryGameCatalogRecord* model = InventoryGameCatalogFindFirstDefinition(g_casSelectorDefinition);
        const BYTE masks[] = { INVENTORY_TEAM_NONE, INVENTORY_TEAM_T, INVENTORY_TEAM_CT, INVENTORY_TEAM_BOTH };
        for (int i = 0; i < 4 && model; ++i)
            if (CasUiPointInRect(x, y, 686 + i * 62, 480, 58, 32) && ((masks[i] & model->teamMask) == masks[i]))
            { g_casSelectorTeams = masks[i]; CasSelectorSetFocus(CAS_SEL_TEAM); }
    }
    return true;
}

static wchar_t* CasSelectorFocusedText(int* capacity)
{
    *capacity = 16;
    switch (g_casSelectorFocus)
    {
    case CAS_SEL_ITEM_SEARCH: *capacity = 64; return g_casSelectorEditId ? nullptr : g_casSelectorItemSearch;
    case CAS_SEL_FINISH_SEARCH: *capacity = 64; return g_casSelectorFinishSearch;
    case CAS_SEL_WEAR:
    case CAS_SEL_SEED:
    {
        const InventoryGameCatalogRecord* record = CasSelectorSelectedRecord();
        if (!record || record->paintKit <= 0 || record->category > 8) return nullptr;
        return g_casSelectorFocus == CAS_SEL_WEAR ? g_casSelectorWear : g_casSelectorSeed;
    }
    case CAS_SEL_COUNTER: return g_casSelectorTracking ? g_casSelectorCounter : nullptr;
    default: return nullptr;
    }
}

static bool CasSelectorHandleInput(UINT msg, WPARAM key, LPARAM flags)
{
    if (g_espConfig.selectedTab != 3 || g_casUiSkinchangerSubtab == 3) return false;
    CasSelectorRebuildLists();
    if (msg == 0x020a /* WM_MOUSEWHEEL */)
    {
        CasUiRefreshMouse(g_menuWindow);
        const int delta = static_cast<short>((key >> 16) & 0xffff);
        if (CasUiMouseOver(174, 130, 196, 426)) g_casSelectorModelScroll += delta > 0 ? -3 : 3;
        else if (CasUiMouseOver(382, 130, 278, 426)) g_casSelectorFinishScroll += delta > 0 ? -3 : 3;
        g_casSelectorListsDirty = true;
        return true;
    }
    int capacity = 0;
    wchar_t* text = CasSelectorFocusedText(&capacity);
    if (msg == 0x0102 /* WM_CHAR */)
    {
        if (!text || key == 9 || key == 13 || key == 27) return true;
        int length = 0; while (text[length]) ++length;
        if (key == 8)
        { if (g_casSelectorSelectAll) text[0] = 0; else if (length > 0) text[length - 1] = 0; }
        else if (key >= 0x20 && key <= 0xffff)
        {
            const bool number = g_casSelectorFocus == CAS_SEL_WEAR || g_casSelectorFocus == CAS_SEL_SEED || g_casSelectorFocus == CAS_SEL_COUNTER;
            if (number && !(key >= L'0' && key <= L'9') && !(g_casSelectorFocus == CAS_SEL_WEAR && (key == L'.' || key == L','))) return true;
            if (g_casSelectorSelectAll) { text[0] = 0; length = 0; }
            if (length + 1 < capacity) { text[length] = static_cast<wchar_t>(key); text[length + 1] = 0; }
        }
        g_casSelectorSelectAll = false;
        g_casSelectorListsDirty = true;
        g_casSelectorNotice = nullptr;
        return true;
    }
    if (msg != 0x0100 /* WM_KEYDOWN */) return false;
    if (key == 9)
    {
        const int direction = (GetAsyncKeyState(0x10) & 0x8000) ? -1 : 1;
        CasSelectorSetFocus((g_casSelectorFocus + CAS_SEL_FOCUS_COUNT + direction) % CAS_SEL_FOCUS_COUNT);
    }
    else if (key == 27)
    {
        if (text && text[0]) { text[0] = 0; g_casSelectorListsDirty = true; }
        else { g_casUiSkinchangerSubtab = 3; g_casUiInventoryView = 0; AtomicExchange(&g_casSelectorInputActive, 0); }
    }
    else if (text && key == L'A' && (GetAsyncKeyState(0x11) & 0x8000)) g_casSelectorSelectAll = true;
    else if (key == 0x2e && text) { text[0] = 0; g_casSelectorListsDirty = true; }
    else if ((key == 0x26 || key == 0x28 || key == 0x21 || key == 0x22 || key == 0x24 || key == 0x23) &&
        (g_casSelectorFocus == CAS_SEL_ITEMS || g_casSelectorFocus == CAS_SEL_FINISHES))
    {
        const bool finishes = g_casSelectorFocus == CAS_SEL_FINISHES;
        const int count = finishes ? g_casSelectorFinishCount : g_casSelectorModelCount;
        int row = CasSelectorSelectedRow(finishes);
        if (key == 0x24) row = 0;
        else if (key == 0x23) row = count - 1;
        else row += key == 0x26 ? -1 : key == 0x28 ? 1 : key == 0x21 ? -CAS_SELECTOR_ROWS : CAS_SELECTOR_ROWS;
        row = InventoryClampInt(row, 0, count > 0 ? count - 1 : 0);
        CasSelectorChooseRow(finishes, row);
        int& scroll = finishes ? g_casSelectorFinishScroll : g_casSelectorModelScroll;
        if (row < scroll) scroll = row;
        if (row >= scroll + CAS_SELECTOR_ROWS) scroll = row - CAS_SELECTOR_ROWS + 1;
    }
    else if (!g_casSelectorEditId && g_casSelectorFocus == CAS_SEL_CATEGORY &&
        (key == 0x25 || key == 0x27))
    { CasSelectorOpen((g_casSelectorCategory + (key == 0x25 ? 3 : 1)) % 4); CasSelectorSetFocus(CAS_SEL_CATEGORY); }
    else if (g_casSelectorFocus == CAS_SEL_TEAM && (key == 0x25 || key == 0x27))
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogFindFirstDefinition(g_casSelectorDefinition);
        if (record) for (int i = 0; i < 4; ++i)
        { g_casSelectorTeams = static_cast<BYTE>((g_casSelectorTeams + (key == 0x25 ? 3 : 1)) % 4); if ((g_casSelectorTeams & record->teamMask) == g_casSelectorTeams) break; }
    }
    else if ((key == 13 || key == 32) && !(flags & (static_cast<LPARAM>(1) << 30)))
    {
        if (g_casSelectorFocus == CAS_SEL_SUBMIT) CasSelectorSubmit();
        else if (g_casSelectorFocus == CAS_SEL_ENABLED) CasSelectorToggleEnabled();
        else if (g_casSelectorFocus == CAS_SEL_TRACKING)
        { const InventoryGameCatalogRecord* r = CasSelectorSelectedRecord(); if (r && r->paintKit > 0 && r->category < 8) g_casSelectorTracking = !g_casSelectorTracking; }
        else if (g_casSelectorFocus == CAS_SEL_MY_ITEMS) { g_casUiSkinchangerSubtab = 3; g_casUiInventoryView = 0; AtomicExchange(&g_casSelectorInputActive, 0); }
        else if (g_casSelectorFocus == CAS_SEL_RETRY && !InventoryGameCatalogReady()) ReloadInventoryGameCatalog();
        else if (g_casSelectorFocus == CAS_SEL_ITEMS) CasSelectorSetFocus(CAS_SEL_FINISHES);
        else if (g_casSelectorFocus == CAS_SEL_FINISHES) CasSelectorSetFocus(CAS_SEL_WEAR);
    }
    return true;
}

static const wchar_t* CasUiItemDisplayName(const VirtualInventoryItem& item)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC) return InventoryMusicName(item.overrideDefinitionIndex);
    const InventoryGameCatalogRecord* record = InventoryGameCatalogForItem(item);
    if (record && InventoryGameCatalogValidateItem(item)) return record->displayName[0] ? record->displayName : CasSelectorWeaponName(record);
    return L"Unverified item - choose a valid finish";
}

static const wchar_t* CasUiItemFinishName(const VirtualInventoryItem& item)
{
    const InventoryGameCatalogRecord* record = InventoryGameCatalogForItem(item);
    return record && InventoryGameCatalogValidateItem(item) ? CasSelectorFinishName(record) : L"Unavailable in installed catalog";
}

static void CasUiDrawInventoryScreen(HDC hdc, int width, int height)
{
    if (g_casUiSkinchangerSubtab != 3 || (g_casUiInventoryView != 0 && g_casUiInventoryView != 3))
    { if (g_casUiSkinchangerSubtab == 3) CasSelectorOpen(0); CasSelectorDraw(hdc, width, height); }
    else
    {
        AtomicExchange(&g_casSelectorInputActive, 0);
        CasUiDrawInventoryScreenLegacy(hdc, width, height);
        CasSelectorDrawEnabled(hdc);
        CasSelectorButton(hdc, 818, 20, 128, L"Add item");
    }
    CasUiDrawTooltipV3(hdc, width, height);
}

static bool CasUiHandleInventoryClick(int x, int y, int height)
{
    if (g_casUiSkinchangerSubtab != 3) return CasSelectorClick(x, y);
    if (CasUiPointInRect(x, y, 668, 20, 138, 32))
    { CasSelectorSetFocus(CAS_SEL_ENABLED); CasSelectorToggleEnabled(); return true; }
    if (CasUiPointInRect(x, y, 818, 20, 128, 32) ||
        (g_casUiInventoryView == 0 && (CasUiPointInRect(x, y, 804, 66, 132, 34) || CasUiPointInRect(x, y, 202, 270, 140, 34))))
    { CasSelectorOpen(0); return true; }
    if (g_casUiInventoryView == 3 && (CasUiPointInRect(x, y, 742, 124, 180, 43) ||
        CasUiPointInRect(x, y, 742, 215, 180, 35)))
    { CasSelectorEditSelected(); return true; }
    // Raw paint-ID increment/decrement is intentionally not an editing path.
    if (g_casUiInventoryView == 3 && CasUiPointInRect(x, y, 742, 250, 80, 34)) return true;
    // Legacy subtab hitboxes no longer exist in the header we draw.
    if (y >= 20 && y <= 54) return true;
    return CasUiHandleInventoryClickLegacy(x, y, height);
}
