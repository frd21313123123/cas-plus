// Text-only build-stage fixture. It is not a translation unit to compile.
struct RGBVal { int fixture; };

static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static bool InventoryGameCatalogReady();
static bool InventoryGameCatalogValidatePair(unsigned short definitionIndex, int paintKit);
static bool InventoryGameCatalogValidateItem(const VirtualInventoryItem& item);
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
    int currentPaint, int direction);

#pragma pack(push, 1)
struct InventoryGameCatalogRecord {
    unsigned short definitionIndex;
    BYTE category;
    BYTE teamMask;
    int paintKit;
    wchar_t displayName[80];
    wchar_t weaponName[48];
    wchar_t finishName[64];
    char rarity[32];
    char iconResource[160];
    char modelPlayer[160];
    char modelWorld[160];
};

struct InventoryGameCatalogHeader {
    unsigned int magic;
    unsigned short version;
    unsigned short recordSize;
    unsigned int count;
    unsigned int checksum;
    unsigned int processId;
    unsigned long long processCreated;
    unsigned int schemaChecksum;
};
#pragma pack(pop)

                g_casUiBrowseCategory = i + 1;
                g_casUiCatalogPage = 0;
                g_casUiInventoryView = 2;

    CasUiDrawLabel(hdc, L"ITEMS", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < visible; ++i)

        const int total = CasUiCatalogCount(g_casUiBrowseCategory);
        const int first = g_casUiCatalogPage * CAS_UI_GRID_PAGE;
