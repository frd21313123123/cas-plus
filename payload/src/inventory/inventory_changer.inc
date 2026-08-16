// cas+ virtual inventory / loadout changer runtime.
// Injected into the generated payload by tools/apply-inventory-changer.ps1.
//
// Design goals:
// - no hard-coded C_EconEntity offsets: every game-facing field is resolved
//   through Source 2 SchemaSystem and bounds-checked before use;
// - fixed-size/no-CRT storage suitable for the manual-mapped payload;
// - virtual inventory instances are separate from entity cosmetics;
// - per-team equip state (T / CT / both);
// - original entity state is captured and restored on disable, unequip,
//   entity replacement, map transition, or payload shutdown;
// - persistence is local only. Nothing here writes Steam/GC inventory state.

extern "C" {
__declspec(dllimport) DWORD WINAPI GetTempPathW(DWORD, wchar_t*);
__declspec(dllimport) HANDLE WINAPI CreateFileW(
    LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI ReadFile(
    HANDLE, LPVOID, DWORD, DWORD*, LPVOID);
__declspec(dllimport) BOOL WINAPI WriteFile(
    HANDLE, const void*, DWORD, DWORD*, LPVOID);
}

#define CAS_GENERIC_READ        0x80000000UL
#define CAS_GENERIC_WRITE       0x40000000UL
#define CAS_FILE_SHARE_READ     0x00000001UL
#define CAS_CREATE_ALWAYS       2
#define CAS_OPEN_EXISTING       3
#define CAS_FILE_ATTRIBUTE_NORMAL 0x00000080UL
#define CAS_INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

enum InventoryTeamMask : BYTE {
    INVENTORY_TEAM_NONE = 0,
    INVENTORY_TEAM_T = 1,
    INVENTORY_TEAM_CT = 2,
    INVENTORY_TEAM_BOTH = 3
};

constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
constexpr int INVENTORY_MAX_ITEMS = 64;
constexpr int INVENTORY_MAX_ORIGINALS = 96;
constexpr int INVENTORY_CUSTOM_NAME_CAPACITY = 32;

struct VirtualInventoryItem {
    unsigned long long itemId;
    unsigned short slotDefinitionIndex;
    unsigned short overrideDefinitionIndex;
    int paintKit;
    int seed;
    int statTrak;
    float wear;
    BYTE equippedTeams;
    BYTE quality;
    BYTE customNameLength;
    BYTE reserved;
    char customName[INVENTORY_CUSTOM_NAME_CAPACITY];
};

struct InventoryStore {
    bool enabled;
    BYTE reserved0[7];
    unsigned long long nextItemId;
    int itemCount;
    VirtualInventoryItem items[INVENTORY_MAX_ITEMS];
};

static InventoryStore g_inventoryStore = {
    true,
    {},
    0xC450000000000001ULL,
    0,
    {}
};

static volatile LONG g_inventoryLock = 0;
static volatile LONG g_inventoryDirty = 0;
static volatile LONG g_inventoryAddCurrentRequest = 0;
static int g_inventoryUiSelected = -1;
static int g_inventoryUiPage = 0;
static unsigned short g_inventoryActiveDefinitionIndex = 0;
static BYTE g_inventoryActiveTeam = 0;
static int g_inventoryLastAppliedCount = 0;
static int g_inventoryLastOwnedEconCount = 0;

struct InventoryRuntime {
    bool ready;
    bool hasQuality;
    bool hasItemIdLow;
    bool hasItemInitialized;
    bool hasAttributesInitialized;
    bool hasAttachmentDirty;
    bool hasCustomName;
    unsigned int attributeManagerOffset;
    unsigned int itemOffset;
    unsigned int itemDefinitionIndexOffset;
    unsigned int entityQualityOffset;
    unsigned int itemIdHighOffset;
    unsigned int itemIdLowOffset;
    unsigned int itemInitializedOffset;
    unsigned int customNameOffset;
    unsigned int fallbackPaintKitOffset;
    unsigned int fallbackSeedOffset;
    unsigned int fallbackWearOffset;
    unsigned int fallbackStatTrakOffset;
    unsigned int attributesInitializedOffset;
    unsigned int attachmentDirtyOffset;
};

static InventoryRuntime g_inventoryRuntime{};

struct OriginalWeaponCosmetics {
    unsigned int entityHandle;
    void* entityAddress;
    void* identityAddress;
    unsigned short itemDefinitionIndex;
    int entityQuality;
    unsigned int itemIdHigh;
    unsigned int itemIdLow;
    int paintKit;
    int seed;
    int statTrak;
    float wear;
    BYTE itemInitialized;
    BYTE attributesInitialized;
    BYTE attachmentDirty;
    bool seen;
};

static OriginalWeaponCosmetics g_originalWeaponCosmetics[INVENTORY_MAX_ORIGINALS]{};
static int g_originalWeaponCosmeticsCount = 0;
static void* g_inventoryLastEntitySystem = nullptr;

struct InventoryPersistenceImage {
    unsigned int magic;
    unsigned short version;
    unsigned short itemCount;
    BYTE enabled;
    BYTE reserved[7];
    unsigned long long nextItemId;
    VirtualInventoryItem items[INVENTORY_MAX_ITEMS];
    unsigned int checksum;
};

constexpr unsigned int INVENTORY_PERSIST_MAGIC = 0x564E4943u; // "CINV"
constexpr unsigned short INVENTORY_PERSIST_VERSION = 2;

struct InventoryUiSnapshot {
    bool enabled;
    int itemCount;
    int selectedIndex;
    int page;
    unsigned short activeDefinitionIndex;
    BYTE activeTeam;
    int appliedCount;
    int ownedEconCount;
    VirtualInventoryItem selectedItem;
    bool hasSelectedItem;
    VirtualInventoryItem visibleItems[7];
    int visibleIndices[7];
    int visibleCount;
};

static bool InventoryTryLock()
{
    return AtomicExchange(&g_inventoryLock, 1) == 0;
}

static void InventoryUnlock()
{
    AtomicExchange(&g_inventoryLock, 0);
}

static int InventoryClampInt(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static float InventoryClampWear(float value)
{
    if (value < 0.0001f)
        return 0.0001f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static bool InventoryIsKnifeDefinition(unsigned short definitionIndex)
{
    if (definitionIndex == 42 || definitionIndex == 59)
        return true;
    // CS knife definitions occupy the 500-series. Keeping this as a range
    // means newly-added knife variants keep using the generic knife loadout
    // slot without requiring a payload rebuild.
    return definitionIndex >= 500 && definitionIndex < 600;
}

static unsigned short InventoryNormalizeSlot(unsigned short definitionIndex)
{
    return InventoryIsKnifeDefinition(definitionIndex) ?
        INVENTORY_SLOT_KNIFE : definitionIndex;
}

static BYTE InventoryTeamBit(BYTE team)
{
    if (team == 2)
        return INVENTORY_TEAM_T;
    if (team == 3)
        return INVENTORY_TEAM_CT;
    return INVENTORY_TEAM_NONE;
}

static unsigned int InventoryChecksum(const BYTE* data, SIZE_T size)
{
    unsigned int hash = 2166136261u;
    for (SIZE_T i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static bool InventoryBuildPersistencePath(wchar_t* path, DWORD capacity)
{
    if (!path || capacity < 64)
        return false;
    DWORD length = GetTempPathW(capacity, path);
    if (length == 0 || length >= capacity)
        return false;
    static const wchar_t suffix[] = L"cas_plus_inventory_v2.bin";
    DWORD suffixLength = 0;
    while (suffix[suffixLength])
        ++suffixLength;
    if (length + suffixLength + 1 >= capacity)
        return false;
    for (DWORD i = 0; i <= suffixLength; ++i)
        path[length + i] = suffix[i];
    return true;
}

static void InventorySanitizeItem(VirtualInventoryItem* item)
{
    if (!item)
        return;
    if (item->slotDefinitionIndex == 0)
        item->slotDefinitionIndex =
            InventoryNormalizeSlot(item->overrideDefinitionIndex);
    if (item->overrideDefinitionIndex == 0 ||
        item->overrideDefinitionIndex > 4095)
        item->overrideDefinitionIndex =
            item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE ?
                42 : item->slotDefinitionIndex;
    item->paintKit = InventoryClampInt(item->paintKit, 0, 100000);
    item->seed = InventoryClampInt(item->seed, 0, 1000);
    item->wear = InventoryClampWear(item->wear);
    if ((item->equippedTeams & ~INVENTORY_TEAM_BOTH) != 0)
        item->equippedTeams = INVENTORY_TEAM_NONE;
    if (item->quality > 20)
        item->quality = 0;
    if (item->customNameLength >= INVENTORY_CUSTOM_NAME_CAPACITY)
        item->customNameLength = INVENTORY_CUSTOM_NAME_CAPACITY - 1;
    item->customName[item->customNameLength] = 0;
}

static bool SaveInventoryStoreLocked()
{
    wchar_t path[512];
    if (!InventoryBuildPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return false;

    InventoryPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    image.magic = INVENTORY_PERSIST_MAGIC;
    image.version = INVENTORY_PERSIST_VERSION;
    image.itemCount = static_cast<unsigned short>(
        InventoryClampInt(g_inventoryStore.itemCount, 0, INVENTORY_MAX_ITEMS));
    image.enabled = g_inventoryStore.enabled ? 1 : 0;
    image.nextItemId = g_inventoryStore.nextItemId;
    for (int i = 0; i < image.itemCount; ++i)
        image.items[i] = g_inventoryStore.items[i];
    image.checksum = InventoryChecksum(
        reinterpret_cast<const BYTE*>(&image),
        static_cast<SIZE_T>(reinterpret_cast<BYTE*>(&image.checksum) -
            reinterpret_cast<BYTE*>(&image)));

    HANDLE file = CreateFileW(path, CAS_GENERIC_WRITE, 0, nullptr,
        CAS_CREATE_ALWAYS, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, &image, sizeof(image), &written, nullptr);
    CloseHandle(file);
    return ok != 0 && written == sizeof(image);
}

static void MarkInventoryDirty()
{
    AtomicExchange(&g_inventoryDirty, 1);
}

static void FlushInventoryPersistenceIfNeeded()
{
    if (AtomicExchange(&g_inventoryDirty, 0) == 0)
        return;
    if (!InventoryTryLock())
    {
        MarkInventoryDirty();
        return;
    }
    const bool saved = SaveInventoryStoreLocked();
    InventoryUnlock();
    if (!saved)
        MarkInventoryDirty();
}

static void LoadInventoryStore()
{
    wchar_t path[512];
    if (!InventoryBuildPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return;

    HANDLE file = CreateFileW(path, CAS_GENERIC_READ, CAS_FILE_SHARE_READ,
        nullptr, CAS_OPEN_EXISTING, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return;

    InventoryPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &image, sizeof(image), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(image) ||
        image.magic != INVENTORY_PERSIST_MAGIC ||
        image.version != INVENTORY_PERSIST_VERSION ||
        image.itemCount > INVENTORY_MAX_ITEMS)
        return;

    const unsigned int expected = InventoryChecksum(
        reinterpret_cast<const BYTE*>(&image),
        static_cast<SIZE_T>(reinterpret_cast<BYTE*>(&image.checksum) -
            reinterpret_cast<BYTE*>(&image)));
    if (expected != image.checksum)
        return;

    if (!InventoryTryLock())
        return;
    g_inventoryStore.enabled = image.enabled != 0;
    g_inventoryStore.itemCount = image.itemCount;
    g_inventoryStore.nextItemId =
        image.nextItemId < 0xC450000000000001ULL ?
            0xC450000000000001ULL : image.nextItemId;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
    {
        g_inventoryStore.items[i] = image.items[i];
        InventorySanitizeItem(&g_inventoryStore.items[i]);
        if (g_inventoryStore.items[i].itemId >= g_inventoryStore.nextItemId)
            g_inventoryStore.nextItemId =
                g_inventoryStore.items[i].itemId + 1;
    }
    for (int i = g_inventoryStore.itemCount; i < INVENTORY_MAX_ITEMS; ++i)
        ZeroBytes(&g_inventoryStore.items[i], sizeof(VirtualInventoryItem));
    g_inventoryUiSelected = g_inventoryStore.itemCount > 0 ? 0 : -1;
    g_inventoryUiPage = 0;
    InventoryUnlock();
}

static bool ResolveInventoryRuntime(InventoryRuntime* runtime)
{
    if (!runtime)
        return false;

    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;

    SchemaClassInfoView* econEntity =
        FindSchemaClass(scope, "C_EconEntity");
    SchemaClassInfoView* attributeContainer =
        FindSchemaClass(scope, "C_AttributeContainer");
    SchemaClassInfoView* econItemView =
        FindSchemaClass(scope, "C_EconItemView");
    if (!econEntity || !attributeContainer || !econItemView)
        return false;

    unsigned int attributeManager = 0;
    unsigned int item = 0;
    unsigned int definition = 0;
    unsigned int quality = 0;
    unsigned int itemIdHigh = 0;
    unsigned int itemIdLow = 0;
    unsigned int itemInitialized = 0;
    unsigned int customName = 0;
    unsigned int paintKit = 0;
    unsigned int seed = 0;
    unsigned int wear = 0;
    unsigned int statTrak = 0;
    unsigned int attributesInitialized = 0;
    unsigned int attachmentDirty = 0;

    if (!FindSchemaField(econEntity, "m_AttributeManager",
            &attributeManager) ||
        !FindSchemaField(attributeContainer, "m_Item", &item) ||
        !FindSchemaField(econItemView, "m_iItemDefinitionIndex",
            &definition) ||
        !FindSchemaField(econItemView, "m_iItemIDHigh", &itemIdHigh) ||
        !FindSchemaField(econEntity, "m_nFallbackPaintKit", &paintKit) ||
        !FindSchemaField(econEntity, "m_nFallbackSeed", &seed) ||
        !FindSchemaField(econEntity, "m_flFallbackWear", &wear) ||
        !FindSchemaField(econEntity, "m_nFallbackStatTrak", &statTrak))
        return false;

    const bool hasQuality =
        FindSchemaField(econItemView, "m_iEntityQuality", &quality);
    const bool hasItemIdLow =
        FindSchemaField(econItemView, "m_iItemIDLow", &itemIdLow);
    const bool hasItemInitialized =
        FindSchemaField(econItemView, "m_bInitialized", &itemInitialized);
    const bool hasCustomName =
        FindSchemaField(econItemView, "m_szCustomName", &customName);
    const bool hasAttributesInitialized =
        FindSchemaField(econEntity, "m_bAttributesInitialized",
            &attributesInitialized);
    const bool hasAttachmentDirty =
        FindSchemaField(econEntity, "m_bAttachmentDirty",
            &attachmentDirty);

    if (attributeManager >= static_cast<unsigned int>(econEntity->size) ||
        item >= static_cast<unsigned int>(attributeContainer->size) ||
        definition + sizeof(unsigned short) >
            static_cast<unsigned int>(econItemView->size) ||
        itemIdHigh + sizeof(unsigned int) >
            static_cast<unsigned int>(econItemView->size) ||
        paintKit + sizeof(int) >
            static_cast<unsigned int>(econEntity->size) ||
        seed + sizeof(int) >
            static_cast<unsigned int>(econEntity->size) ||
        wear + sizeof(float) >
            static_cast<unsigned int>(econEntity->size) ||
        statTrak + sizeof(int) >
            static_cast<unsigned int>(econEntity->size))
        return false;

    if ((hasQuality && quality + sizeof(int) >
            static_cast<unsigned int>(econItemView->size)) ||
        (hasItemIdLow && itemIdLow + sizeof(unsigned int) >
            static_cast<unsigned int>(econItemView->size)) ||
        (hasItemInitialized && itemInitialized + sizeof(BYTE) >
            static_cast<unsigned int>(econItemView->size)) ||
        (hasCustomName && customName + INVENTORY_CUSTOM_NAME_CAPACITY >
            static_cast<unsigned int>(econItemView->size)) ||
        (hasAttributesInitialized &&
            attributesInitialized + sizeof(BYTE) >
                static_cast<unsigned int>(econEntity->size)) ||
        (hasAttachmentDirty && attachmentDirty + sizeof(BYTE) >
            static_cast<unsigned int>(econEntity->size)))
        return false;

    runtime->ready = true;
    runtime->hasQuality = hasQuality;
    runtime->hasItemIdLow = hasItemIdLow;
    runtime->hasItemInitialized = hasItemInitialized;
    runtime->hasAttributesInitialized = hasAttributesInitialized;
    runtime->hasAttachmentDirty = hasAttachmentDirty;
    runtime->hasCustomName = hasCustomName;
    runtime->attributeManagerOffset = attributeManager;
    runtime->itemOffset = item;
    runtime->itemDefinitionIndexOffset = definition;
    runtime->entityQualityOffset = quality;
    runtime->itemIdHighOffset = itemIdHigh;
    runtime->itemIdLowOffset = itemIdLow;
    runtime->itemInitializedOffset = itemInitialized;
    runtime->customNameOffset = customName;
    runtime->fallbackPaintKitOffset = paintKit;
    runtime->fallbackSeedOffset = seed;
    runtime->fallbackWearOffset = wear;
    runtime->fallbackStatTrakOffset = statTrak;
    runtime->attributesInitializedOffset = attributesInitialized;
    runtime->attachmentDirtyOffset = attachmentDirty;
    return true;
}

static BYTE* InventoryItemView(void* weapon)
{
    if (!weapon || !g_inventoryRuntime.ready)
        return nullptr;
    return reinterpret_cast<BYTE*>(weapon) +
        g_inventoryRuntime.attributeManagerOffset +
        g_inventoryRuntime.itemOffset;
}

static bool InventoryReadDefinition(void* weapon,
    unsigned short* definitionIndex)
{
    if (!weapon || !definitionIndex || !g_inventoryRuntime.ready)
        return false;
    BYTE* itemView = InventoryItemView(weapon);
    if (!itemView)
        return false;
    unsigned short* field = reinterpret_cast<unsigned short*>(
        itemView + g_inventoryRuntime.itemDefinitionIndexOffset);
    if (!IsAccessible(field, sizeof(unsigned short), false))
        return false;
    const unsigned short value = *field;
    if (value == 0 || value > 4095)
        return false;
    *definitionIndex = value;
    return true;
}

static OriginalWeaponCosmetics* FindOriginalWeaponCosmetics(
    unsigned int entityHandle)
{
    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
        if (g_originalWeaponCosmetics[i].entityHandle == entityHandle)
            return &g_originalWeaponCosmetics[i];
    return nullptr;
}

static bool InventoryOriginalMatchesEntity(
    const OriginalWeaponCosmetics& original, void* entity)
{
    if (!entity || entity != original.entityAddress ||
        !IsAccessible(entity, 0x18, false))
        return false;
    void* identity = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(entity) + 0x10);
    return identity == original.identityAddress;
}

static bool CaptureOriginalWeaponCosmetics(void* weapon,
    unsigned int handle)
{
    if (!weapon || handle == 0xFFFFFFFFu)
        return false;

    OriginalWeaponCosmetics* existing =
        FindOriginalWeaponCosmetics(handle);
    if (existing)
    {
        if (InventoryOriginalMatchesEntity(*existing, weapon))
        {
            existing->seen = true;
            return true;
        }

        const int staleIndex = static_cast<int>(
            existing - g_originalWeaponCosmetics);
        for (int i = staleIndex + 1;
            i < g_originalWeaponCosmeticsCount; ++i)
            g_originalWeaponCosmetics[i - 1] =
                g_originalWeaponCosmetics[i];
        --g_originalWeaponCosmeticsCount;
    }

    if (g_originalWeaponCosmeticsCount >= INVENTORY_MAX_ORIGINALS)
        return false;

    BYTE* base = reinterpret_cast<BYTE*>(weapon);
    BYTE* itemView = InventoryItemView(weapon);
    if (!itemView || !IsAccessible(weapon, 0x18, false))
        return false;

    auto* definition = reinterpret_cast<unsigned short*>(
        itemView + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        itemView + g_inventoryRuntime.itemIdHighOffset);
    auto* paintKit = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackPaintKitOffset);
    auto* seed = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackSeedOffset);
    auto* wear = reinterpret_cast<float*>(
        base + g_inventoryRuntime.fallbackWearOffset);
    auto* statTrak = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackStatTrakOffset);

    if (!IsAccessible(definition, sizeof(unsigned short), false) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), false) ||
        !IsAccessible(paintKit, sizeof(int), false) ||
        !IsAccessible(seed, sizeof(int), false) ||
        !IsAccessible(wear, sizeof(float), false) ||
        !IsAccessible(statTrak, sizeof(int), false))
        return false;

    OriginalWeaponCosmetics& original =
        g_originalWeaponCosmetics[g_originalWeaponCosmeticsCount++];
    ZeroBytes(&original, sizeof(original));
    original.entityHandle = handle;
    original.entityAddress = weapon;
    original.identityAddress = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(weapon) + 0x10);
    original.itemDefinitionIndex = *definition;
    original.itemIdHigh = *itemIdHigh;
    original.paintKit = *paintKit;
    original.seed = *seed;
    original.wear = *wear;
    original.statTrak = *statTrak;
    original.seen = true;

    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            itemView + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), false))
            original.entityQuality = *quality;
    }
    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            itemView + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), false))
            original.itemIdLow = *itemIdLow;
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = itemView +
            g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), false))
            original.itemInitialized = *initialized;
    }
    if (g_inventoryRuntime.hasAttributesInitialized)
    {
        BYTE* initialized = base +
            g_inventoryRuntime.attributesInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), false))
            original.attributesInitialized = *initialized;
    }
    if (g_inventoryRuntime.hasAttachmentDirty)
    {
        BYTE* dirty = base + g_inventoryRuntime.attachmentDirtyOffset;
        if (IsAccessible(dirty, sizeof(BYTE), false))
            original.attachmentDirty = *dirty;
    }
    return true;
}

static bool RestoreOneInventoryEntity(
    const OriginalWeaponCosmetics& original, void* entity)
{
    if (!InventoryOriginalMatchesEntity(original, entity) ||
        !g_inventoryRuntime.ready)
        return false;

    BYTE* base = reinterpret_cast<BYTE*>(entity);
    BYTE* itemView = InventoryItemView(entity);
    if (!itemView)
        return false;

    auto* definition = reinterpret_cast<unsigned short*>(
        itemView + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        itemView + g_inventoryRuntime.itemIdHighOffset);
    auto* paintKit = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackPaintKitOffset);
    auto* seed = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackSeedOffset);
    auto* wear = reinterpret_cast<float*>(
        base + g_inventoryRuntime.fallbackWearOffset);
    auto* statTrak = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackStatTrakOffset);

    if (!IsAccessible(definition, sizeof(unsigned short), true) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), true) ||
        !IsAccessible(paintKit, sizeof(int), true) ||
        !IsAccessible(seed, sizeof(int), true) ||
        !IsAccessible(wear, sizeof(float), true) ||
        !IsAccessible(statTrak, sizeof(int), true))
        return false;

    *definition = original.itemDefinitionIndex;
    *itemIdHigh = original.itemIdHigh;
    *paintKit = original.paintKit;
    *seed = original.seed;
    *wear = original.wear;
    *statTrak = original.statTrak;

    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            itemView + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), true))
            *quality = original.entityQuality;
    }
    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            itemView + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), true))
            *itemIdLow = original.itemIdLow;
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = itemView +
            g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = original.itemInitialized;
    }
    if (g_inventoryRuntime.hasAttributesInitialized)
    {
        BYTE* initialized = base +
            g_inventoryRuntime.attributesInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = original.attributesInitialized;
    }
    if (g_inventoryRuntime.hasAttachmentDirty)
    {
        BYTE* dirty = base + g_inventoryRuntime.attachmentDirtyOffset;
        if (IsAccessible(dirty, sizeof(BYTE), true))
            *dirty = 1;
    }
    return true;
}

static void RestoreInventoryChanger(void* entitySystem)
{
    if (!entitySystem || !g_inventoryRuntime.ready)
    {
        g_originalWeaponCosmeticsCount = 0;
        return;
    }

    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
    {
        OriginalWeaponCosmetics& original =
            g_originalWeaponCosmetics[i];
        void* entity = EntityFromHandle(g_preResolvedEntityRuntime,
            entitySystem, original.entityHandle);
        if (entity)
            RestoreOneInventoryEntity(original, entity);
    }
    g_originalWeaponCosmeticsCount = 0;
    g_inventoryLastAppliedCount = 0;
}

static VirtualInventoryItem* FindEquippedVirtualItemLocked(
    unsigned short definitionIndex, BYTE teamBit)
{
    if (teamBit == INVENTORY_TEAM_NONE)
        return nullptr;
    const unsigned short slot = InventoryNormalizeSlot(definitionIndex);
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
    {
        VirtualInventoryItem& item = g_inventoryStore.items[i];
        if (item.slotDefinitionIndex == slot &&
            (item.equippedTeams & teamBit) != 0)
            return &item;
    }
    return nullptr;
}

static bool ApplyVirtualInventoryItem(void* weapon,
    const VirtualInventoryItem& source)
{
    if (!weapon || !g_inventoryRuntime.ready)
        return false;
    const unsigned int handle = EntityHandleFor(weapon);
    if (handle == 0xFFFFFFFFu ||
        !CaptureOriginalWeaponCosmetics(weapon, handle))
        return false;

    VirtualInventoryItem item = source;
    InventorySanitizeItem(&item);

    BYTE* base = reinterpret_cast<BYTE*>(weapon);
    BYTE* itemView = InventoryItemView(weapon);
    if (!itemView)
        return false;

    auto* definition = reinterpret_cast<unsigned short*>(
        itemView + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        itemView + g_inventoryRuntime.itemIdHighOffset);
    auto* paintKit = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackPaintKitOffset);
    auto* seed = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackSeedOffset);
    auto* wear = reinterpret_cast<float*>(
        base + g_inventoryRuntime.fallbackWearOffset);
    auto* statTrak = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackStatTrakOffset);

    if (!IsAccessible(definition, sizeof(unsigned short), true) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), true) ||
        !IsAccessible(paintKit, sizeof(int), true) ||
        !IsAccessible(seed, sizeof(int), true) ||
        !IsAccessible(wear, sizeof(float), true) ||
        !IsAccessible(statTrak, sizeof(int), true))
        return false;

    *definition = item.overrideDefinitionIndex;
    *itemIdHigh = 0xFFFFFFFFu;
    *paintKit = item.paintKit;
    *seed = item.seed;
    *wear = item.wear;
    *statTrak = item.statTrak;

    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            itemView + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), true))
            *itemIdLow = static_cast<unsigned int>(item.itemId);
    }
    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            itemView + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), true))
            *quality = item.quality != 0 ? item.quality :
                (InventoryIsKnifeDefinition(item.overrideDefinitionIndex) ?
                    3 : *quality);
    }
    if (g_inventoryRuntime.hasCustomName &&
        item.customNameLength > 0)
    {
        char* customName = reinterpret_cast<char*>(
            itemView + g_inventoryRuntime.customNameOffset);
        if (IsAccessible(customName,
            INVENTORY_CUSTOM_NAME_CAPACITY, true))
        {
            int i = 0;
            for (; i < item.customNameLength &&
                i + 1 < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
                customName[i] = item.customName[i];
            customName[i] = 0;
        }
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = itemView +
            g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    if (g_inventoryRuntime.hasAttributesInitialized)
    {
        BYTE* initialized = base +
            g_inventoryRuntime.attributesInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    if (g_inventoryRuntime.hasAttachmentDirty)
    {
        BYTE* dirty = base + g_inventoryRuntime.attachmentDirtyOffset;
        if (IsAccessible(dirty, sizeof(BYTE), true))
            *dirty = 1;
    }
    return true;
}

static bool InventoryReadWeaponFallbacks(void* weapon,
    VirtualInventoryItem* output)
{
    if (!weapon || !output || !g_inventoryRuntime.ready)
        return false;
    BYTE* base = reinterpret_cast<BYTE*>(weapon);
    BYTE* itemView = InventoryItemView(weapon);
    unsigned short definition = 0;
    if (!itemView || !InventoryReadDefinition(weapon, &definition))
        return false;

    int* paintKit = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackPaintKitOffset);
    int* seed = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackSeedOffset);
    float* wear = reinterpret_cast<float*>(
        base + g_inventoryRuntime.fallbackWearOffset);
    int* statTrak = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackStatTrakOffset);
    if (!IsAccessible(paintKit, sizeof(int), false) ||
        !IsAccessible(seed, sizeof(int), false) ||
        !IsAccessible(wear, sizeof(float), false) ||
        !IsAccessible(statTrak, sizeof(int), false))
        return false;

    ZeroBytes(output, sizeof(*output));
    output->slotDefinitionIndex = InventoryNormalizeSlot(definition);
    output->overrideDefinitionIndex = definition;
    output->paintKit = *paintKit;
    output->seed = *seed;
    output->wear = InventoryClampWear(*wear);
    output->statTrak = *statTrak;
    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            itemView + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), false))
            output->quality = static_cast<BYTE>(
                InventoryClampInt(*quality, 0, 20));
    }
    return true;
}

static void InventoryEquipItemLocked(int index, BYTE teams)
{
    if (index < 0 || index >= g_inventoryStore.itemCount)
        return;
    teams &= INVENTORY_TEAM_BOTH;
    VirtualInventoryItem& selected = g_inventoryStore.items[index];
    if (teams != INVENTORY_TEAM_NONE)
    {
        for (int i = 0; i < g_inventoryStore.itemCount; ++i)
        {
            if (i == index ||
                g_inventoryStore.items[i].slotDefinitionIndex !=
                    selected.slotDefinitionIndex)
                continue;
            g_inventoryStore.items[i].equippedTeams &=
                static_cast<BYTE>(~teams);
        }
    }
    selected.equippedTeams = teams;
    MarkInventoryDirty();
}

static bool InventoryAddCurrentWeapon(void* weapon, BYTE localTeam)
{
    if (!weapon || !InventoryTryLock())
        return false;
    bool added = false;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        if (InventoryReadWeaponFallbacks(weapon, &item))
        {
            item.itemId = g_inventoryStore.nextItemId++;
            item.equippedTeams = InventoryTeamBit(localTeam);
            InventorySanitizeItem(&item);
            const int index = g_inventoryStore.itemCount++;
            g_inventoryStore.items[index] = item;
            InventoryEquipItemLocked(index, item.equippedTeams);
            g_inventoryUiSelected = index;
            g_inventoryUiPage = index / 7;
            MarkInventoryDirty();
            added = true;
        }
    }
    InventoryUnlock();
    return added;
}

static bool InventoryResolveLocal(void* entitySystem,
    void** localController, void** localPawn,
    unsigned int* pawnHandle, BYTE* localTeam)
{
    if (!entitySystem || !localController || !localPawn ||
        !pawnHandle || !localTeam || !g_botHighlightRuntimeReady)
        return false;
    *localController = nullptr;
    *localPawn = nullptr;
    *pawnHandle = 0xFFFFFFFFu;
    *localTeam = 0;

    for (int index = 1; index <= 64; ++index)
    {
        void* controller = EntityAtIndex(g_preResolvedEntityRuntime,
            entitySystem, index);
        if (!HasDesignerName(controller, "cs_player_controller"))
            continue;
        BYTE* localFlag = reinterpret_cast<BYTE*>(controller) +
            g_botHighlightRuntime.localControllerOffset;
        BYTE* team = reinterpret_cast<BYTE*>(controller) +
            g_botHighlightRuntime.teamOffset;
        if (!IsAccessible(localFlag, 1, false) ||
            !IsAccessible(team, 1, false) || *localFlag == 0)
            continue;
        *localController = controller;
        *localTeam = *team;
        break;
    }

    if (!*localController || (*localTeam != 2 && *localTeam != 3))
        return false;
    unsigned int* handle = reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(*localController) +
        g_botHighlightRuntime.playerPawnHandleOffset);
    if (!IsAccessible(handle, sizeof(unsigned int), false))
        return false;
    *pawnHandle = *handle;
    *localPawn = EntityFromHandle(g_preResolvedEntityRuntime,
        entitySystem, *pawnHandle);
    return *localPawn != nullptr;
}

static void InventoryRestoreUnseen(void* entitySystem)
{
    int destination = 0;
    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
    {
        OriginalWeaponCosmetics& original =
            g_originalWeaponCosmetics[i];
        if (!original.seen)
        {
            void* entity = EntityFromHandle(g_preResolvedEntityRuntime,
                entitySystem, original.entityHandle);
            if (entity &&
                InventoryOriginalMatchesEntity(original, entity))
                RestoreOneInventoryEntity(original, entity);
            continue;
        }
        g_originalWeaponCosmetics[destination++] = original;
    }
    g_originalWeaponCosmeticsCount = destination;
}

static void UpdateInventoryChanger()
{
    if (!g_preResolvedEntityRuntimeReady || !g_botHighlightRuntimeReady)
        return;

    if (!g_inventoryRuntime.ready &&
        !ResolveInventoryRuntime(&g_inventoryRuntime))
        return;

    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        g_originalWeaponCosmeticsCount = 0;
        g_inventoryLastEntitySystem = nullptr;
        g_inventoryActiveDefinitionIndex = 0;
        g_inventoryActiveTeam = 0;
        return;
    }

    if (g_inventoryLastEntitySystem &&
        g_inventoryLastEntitySystem != entitySystem)
    {
        g_originalWeaponCosmeticsCount = 0;
        g_inventoryActiveDefinitionIndex = 0;
        g_inventoryActiveTeam = 0;
    }
    g_inventoryLastEntitySystem = entitySystem;

    void* localController = nullptr;
    void* localPawn = nullptr;
    unsigned int localPawnHandle = 0xFFFFFFFFu;
    BYTE localTeam = 0;
    if (!InventoryResolveLocal(entitySystem, &localController,
        &localPawn, &localPawnHandle, &localTeam))
        return;

    g_inventoryActiveTeam = localTeam;

    void* activeWeapon = nullptr;
    void** weaponServices = reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(localPawn) +
        g_botHighlightRuntime.weaponServicesOffset);
    if (IsAccessible(weaponServices, sizeof(void*), false) &&
        *weaponServices &&
        IsAccessible(reinterpret_cast<BYTE*>(*weaponServices) +
            g_botHighlightRuntime.activeWeaponOffset,
            sizeof(unsigned int), false))
    {
        const unsigned int activeHandle =
            *reinterpret_cast<unsigned int*>(
                reinterpret_cast<BYTE*>(*weaponServices) +
                g_botHighlightRuntime.activeWeaponOffset);
        activeWeapon = EntityFromHandle(g_preResolvedEntityRuntime,
            entitySystem, activeHandle);
    }

    unsigned short activeDefinition = 0;
    if (activeWeapon &&
        InventoryReadDefinition(activeWeapon, &activeDefinition))
        g_inventoryActiveDefinitionIndex = activeDefinition;
    else
        g_inventoryActiveDefinitionIndex = 0;

    if (AtomicExchange(&g_inventoryAddCurrentRequest, 0) > 0 &&
        activeWeapon)
        InventoryAddCurrentWeapon(activeWeapon, localTeam);

    bool enabled = false;
    if (InventoryTryLock())
    {
        enabled = g_inventoryStore.enabled;
        InventoryUnlock();
    }
    else
        return;

    if (!enabled)
    {
        if (g_originalWeaponCosmeticsCount > 0)
            RestoreInventoryChanger(entitySystem);
        return;
    }

    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
        g_originalWeaponCosmetics[i].seen = false;

    const int highest = *reinterpret_cast<int*>(
        reinterpret_cast<BYTE*>(entitySystem) +
        g_preResolvedEntityRuntime.highestEntityOffset);
    if (highest < 0 || highest > 32768)
        return;

    const BYTE teamBit = InventoryTeamBit(localTeam);
    int applied = 0;
    int ownedEcon = 0;

    for (int index = 1; index <= highest; ++index)
    {
        void* entity = EntityAtIndex(g_preResolvedEntityRuntime,
            entitySystem, index);
        if (!entity || entity == localPawn)
            continue;

        unsigned int* owner = reinterpret_cast<unsigned int*>(
            reinterpret_cast<BYTE*>(entity) +
            g_botHighlightRuntime.ownerEntityOffset);
        if (!IsAccessible(owner, sizeof(unsigned int), false) ||
            *owner != localPawnHandle)
            continue;

        unsigned short definition = 0;
        if (!InventoryReadDefinition(entity, &definition))
            continue;
        ++ownedEcon;

        VirtualInventoryItem selected;
        bool hasSelected = false;
        if (InventoryTryLock())
        {
            VirtualInventoryItem* item =
                FindEquippedVirtualItemLocked(definition, teamBit);
            if (item)
            {
                selected = *item;
                hasSelected = true;
            }
            InventoryUnlock();
        }
        else
            continue;

        OriginalWeaponCosmetics* original =
            FindOriginalWeaponCosmetics(EntityHandleFor(entity));
        if (!hasSelected)
        {
            if (original &&
                InventoryOriginalMatchesEntity(*original, entity))
                original->seen = false;
            continue;
        }

        if (ApplyVirtualInventoryItem(entity, selected))
        {
            OriginalWeaponCosmetics* captured =
                FindOriginalWeaponCosmetics(EntityHandleFor(entity));
            if (captured)
                captured->seen = true;
            ++applied;
        }
    }

    g_inventoryLastAppliedCount = applied;
    g_inventoryLastOwnedEconCount = ownedEcon;
    InventoryRestoreUnseen(entitySystem);
}

static void ShutdownInventoryChanger()
{
    if (!g_inventoryRuntime.ready ||
        !g_preResolvedEntityRuntimeReady)
        return;
    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (entitySystem)
        RestoreInventoryChanger(entitySystem);
    FlushInventoryPersistenceIfNeeded();
}

static bool InventoryGetUiSnapshot(InventoryUiSnapshot* snapshot)
{
    if (!snapshot)
        return false;
    ZeroBytes(snapshot, sizeof(*snapshot));
    if (!InventoryTryLock())
        return false;

    snapshot->enabled = g_inventoryStore.enabled;
    snapshot->itemCount = g_inventoryStore.itemCount;
    if (g_inventoryUiSelected >= g_inventoryStore.itemCount)
        g_inventoryUiSelected = g_inventoryStore.itemCount - 1;
    if (g_inventoryUiSelected < -1)
        g_inventoryUiSelected = -1;
    const int maxPage = g_inventoryStore.itemCount > 0 ?
        (g_inventoryStore.itemCount - 1) / 7 : 0;
    g_inventoryUiPage =
        InventoryClampInt(g_inventoryUiPage, 0, maxPage);
    snapshot->selectedIndex = g_inventoryUiSelected;
    snapshot->page = g_inventoryUiPage;
    snapshot->activeDefinitionIndex =
        g_inventoryActiveDefinitionIndex;
    snapshot->activeTeam = g_inventoryActiveTeam;
    snapshot->appliedCount = g_inventoryLastAppliedCount;
    snapshot->ownedEconCount = g_inventoryLastOwnedEconCount;
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
    {
        snapshot->selectedItem =
            g_inventoryStore.items[g_inventoryUiSelected];
        snapshot->hasSelectedItem = true;
    }

    const int first = g_inventoryUiPage * 7;
    for (int i = 0; i < 7 && first + i < g_inventoryStore.itemCount; ++i)
    {
        snapshot->visibleItems[snapshot->visibleCount] =
            g_inventoryStore.items[first + i];
        snapshot->visibleIndices[snapshot->visibleCount] = first + i;
        ++snapshot->visibleCount;
    }
    InventoryUnlock();
    return true;
}

static void InventoryUiToggleEnabled()
{
    if (!InventoryTryLock())
        return;
    g_inventoryStore.enabled = !g_inventoryStore.enabled;
    MarkInventoryDirty();
    InventoryUnlock();
}

static void InventoryUiRequestAddCurrent()
{
    AtomicExchange(&g_inventoryAddCurrentRequest, 1);
}

static void InventoryUiSelect(int index)
{
    if (!InventoryTryLock())
        return;
    if (index >= 0 && index < g_inventoryStore.itemCount)
        g_inventoryUiSelected = index;
    InventoryUnlock();
}

static void InventoryUiChangePage(int delta)
{
    if (!InventoryTryLock())
        return;
    const int maxPage = g_inventoryStore.itemCount > 0 ?
        (g_inventoryStore.itemCount - 1) / 7 : 0;
    g_inventoryUiPage =
        InventoryClampInt(g_inventoryUiPage + delta, 0, maxPage);
    InventoryUnlock();
}

static VirtualInventoryItem* InventorySelectedLocked()
{
    if (g_inventoryUiSelected < 0 ||
        g_inventoryUiSelected >= g_inventoryStore.itemCount)
        return nullptr;
    return &g_inventoryStore.items[g_inventoryUiSelected];
}

static void InventoryUiDeleteSelected()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
    {
        for (int i = g_inventoryUiSelected + 1;
            i < g_inventoryStore.itemCount; ++i)
            g_inventoryStore.items[i - 1] = g_inventoryStore.items[i];
        --g_inventoryStore.itemCount;
        ZeroBytes(&g_inventoryStore.items[g_inventoryStore.itemCount],
            sizeof(VirtualInventoryItem));
        if (g_inventoryUiSelected >= g_inventoryStore.itemCount)
            g_inventoryUiSelected = g_inventoryStore.itemCount - 1;
        g_inventoryUiPage =
            g_inventoryUiSelected > 0 ? g_inventoryUiSelected / 7 : 0;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiAdjustPaint(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->paintKit = InventoryClampInt(
            item->paintKit + delta, 0, 100000);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiAdjustSeed(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->seed = InventoryClampInt(item->seed + delta, 0, 1000);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiAdjustDefinition(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
    {
        int value = static_cast<int>(item->overrideDefinitionIndex) + delta;
        value = InventoryClampInt(value, 1, 4095);
        item->overrideDefinitionIndex =
            static_cast<unsigned short>(value);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiSetDefinition(unsigned short definitionIndex)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE &&
        definitionIndex > 0 && definitionIndex <= 4095 &&
        InventoryIsKnifeDefinition(definitionIndex))
    {
        item->overrideDefinitionIndex = definitionIndex;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiSetWear(float wear)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->wear = InventoryClampWear(wear);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiToggleStatTrak()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->statTrak = item->statTrak >= 0 ? -1 : 0;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiSetEquipMask(BYTE teams)
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
        InventoryEquipItemLocked(g_inventoryUiSelected, teams);
    InventoryUnlock();
}
