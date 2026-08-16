// cas+ extended inventory runtime.
//
// Adds the remaining local-only inventory features without changing the v2
// VirtualInventoryItem persistence ABI:
// - sticker stacks live in a separate versioned sidecar keyed by virtual itemId;
// - a guarded client CEconItem/SharedObject mirror provides engine-owned dynamic
//   attributes and loadout integration when the current client ABI validates;
// - agent entries use the same virtual store and loadout model;
// - custom-name/search keyboard capture subclasses only the game window and is
//   fully reversible on shutdown;
// - StatTrak swap is a local InventoryStore operation.
//
// Nothing in this module sends GC messages or modifies the Steam inventory.

constexpr int INVENTORY_STICKER_SLOT_COUNT = 5;
constexpr int INVENTORY_EXT_MAX_RECORDS = INVENTORY_MAX_ITEMS;
constexpr int INVENTORY_AGENT_LOADOUT_SLOT = 38;
constexpr int INVENTORY_GLOVE_LOADOUT_SLOT = 41;
constexpr int INVENTORY_MUSIC_LOADOUT_SLOT = 54;
constexpr int INVENTORY_ECON_TYPE_ITEM = 1;
constexpr int INVENTORY_SO_EVENT_INCREMENTAL = 4;
constexpr int INVENTORY_TEXT_NONE = 0;
constexpr int INVENTORY_TEXT_NAME = 1;
constexpr int INVENTORY_TEXT_SEARCH = 2;

struct InventoryStickerSlot {
    int id;
    float wear;
    float scale;
    float rotation;
    float offsetX;
    float offsetY;
    BYTE schema;
    BYTE reserved[3];
};

struct InventoryStickerRecord {
    unsigned long long itemId;
    InventoryStickerSlot slots[INVENTORY_STICKER_SLOT_COUNT];
};

struct InventoryStickerPersistenceImage {
    unsigned int magic;
    unsigned short version;
    unsigned short count;
    InventoryStickerRecord records[INVENTORY_EXT_MAX_RECORDS];
    unsigned int checksum;
};

constexpr unsigned int INVENTORY_STICKER_PERSIST_MAGIC = 0x4B545343u; // CSTK
constexpr unsigned short INVENTORY_STICKER_PERSIST_VERSION = 1;

static InventoryStickerRecord g_inventoryStickerRecords[INVENTORY_EXT_MAX_RECORDS]{};
static int g_inventoryStickerRecordCount = 0;
static volatile LONG g_inventoryStickerLock = 0;
static volatile LONG g_inventoryStickerDirty = 0;
static int g_inventoryStickerUiSlot = 0;
static bool g_inventoryStickerModal = false;

static bool InventoryStickerTryLock()
{
    return AtomicExchange(&g_inventoryStickerLock, 1) == 0;
}

static void InventoryStickerUnlock()
{
    AtomicExchange(&g_inventoryStickerLock, 0);
}

static float InventoryStickerClamp(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void InventoryStickerSanitize(InventoryStickerSlot* slot)
{
    if (!slot)
        return;
    if (slot->id < 0)
        slot->id = 0;
    slot->wear = InventoryStickerClamp(slot->wear, 0.0f, 1.0f);
    if (slot->scale <= 0.0f)
        slot->scale = 1.0f;
    slot->scale = InventoryStickerClamp(slot->scale, 0.1f, 5.0f);
    slot->rotation = InventoryStickerClamp(slot->rotation, -180.0f, 180.0f);
    slot->offsetX = InventoryStickerClamp(slot->offsetX, -2.0f, 2.0f);
    slot->offsetY = InventoryStickerClamp(slot->offsetY, -2.0f, 2.0f);
    if (slot->schema >= INVENTORY_STICKER_SLOT_COUNT)
        slot->schema = 0;
    if (slot->id == 0)
    {
        slot->wear = 0.0f;
        slot->scale = 1.0f;
        slot->rotation = 0.0f;
        slot->offsetX = 0.0f;
        slot->offsetY = 0.0f;
        slot->schema = 0;
    }
}

static InventoryStickerRecord* InventoryStickerFindLocked(
    unsigned long long itemId, bool create)
{
    if (!itemId)
        return nullptr;
    for (int i = 0; i < g_inventoryStickerRecordCount; ++i)
        if (g_inventoryStickerRecords[i].itemId == itemId)
            return &g_inventoryStickerRecords[i];
    if (!create || g_inventoryStickerRecordCount >= INVENTORY_EXT_MAX_RECORDS)
        return nullptr;
    InventoryStickerRecord& record =
        g_inventoryStickerRecords[g_inventoryStickerRecordCount++];
    ZeroBytes(&record, sizeof(record));
    record.itemId = itemId;
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        record.slots[i].scale = 1.0f;
        record.slots[i].schema = static_cast<BYTE>(i);
    }
    return &record;
}

static bool InventoryStickerCopyRecord(unsigned long long itemId,
    InventoryStickerRecord* output)
{
    if (!output || !InventoryStickerTryLock())
        return false;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, false);
    if (record)
        *output = *record;
    else
    {
        ZeroBytes(output, sizeof(*output));
        output->itemId = itemId;
        for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
        {
            output->slots[i].scale = 1.0f;
            output->slots[i].schema = static_cast<BYTE>(i);
        }
    }
    InventoryStickerUnlock();
    return record != nullptr;
}

static bool InventoryBuildStickerPersistencePath(wchar_t* path, DWORD capacity)
{
    if (!path || capacity < 64)
        return false;
    DWORD length = GetTempPathW(capacity, path);
    if (length == 0 || length >= capacity)
        return false;
    static const wchar_t suffix[] = L"cas_plus_inventory_stickers_v1.bin";
    DWORD suffixLength = 0;
    while (suffix[suffixLength]) ++suffixLength;
    if (length + suffixLength + 1 >= capacity)
        return false;
    for (DWORD i = 0; i <= suffixLength; ++i)
        path[length + i] = suffix[i];
    return true;
}

static void MarkInventoryStickerDirty()
{
    AtomicExchange(&g_inventoryStickerDirty, 1);
}

static bool SaveInventoryStickerStoreLocked()
{
    wchar_t path[512];
    if (!InventoryBuildStickerPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return false;
    InventoryStickerPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    image.magic = INVENTORY_STICKER_PERSIST_MAGIC;
    image.version = INVENTORY_STICKER_PERSIST_VERSION;
    image.count = static_cast<unsigned short>(InventoryClampInt(
        g_inventoryStickerRecordCount, 0, INVENTORY_EXT_MAX_RECORDS));
    for (int i = 0; i < image.count; ++i)
        image.records[i] = g_inventoryStickerRecords[i];
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

static void LoadInventoryStickerStore()
{
    wchar_t path[512];
    if (!InventoryBuildStickerPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return;
    HANDLE file = CreateFileW(path, CAS_GENERIC_READ, CAS_FILE_SHARE_READ,
        nullptr, CAS_OPEN_EXISTING, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return;
    InventoryStickerPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &image, sizeof(image), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(image) ||
        image.magic != INVENTORY_STICKER_PERSIST_MAGIC ||
        image.version != INVENTORY_STICKER_PERSIST_VERSION ||
        image.count > INVENTORY_EXT_MAX_RECORDS)
        return;
    const unsigned int expected = InventoryChecksum(
        reinterpret_cast<const BYTE*>(&image),
        static_cast<SIZE_T>(reinterpret_cast<BYTE*>(&image.checksum) -
            reinterpret_cast<BYTE*>(&image)));
    if (expected != image.checksum || !InventoryStickerTryLock())
        return;
    g_inventoryStickerRecordCount = image.count;
    for (int i = 0; i < g_inventoryStickerRecordCount; ++i)
    {
        g_inventoryStickerRecords[i] = image.records[i];
        for (int slot = 0; slot < INVENTORY_STICKER_SLOT_COUNT; ++slot)
            InventoryStickerSanitize(&g_inventoryStickerRecords[i].slots[slot]);
    }
    InventoryStickerUnlock();
}

static void FlushInventoryStickerPersistenceIfNeeded()
{
    if (AtomicExchange(&g_inventoryStickerDirty, 0) == 0)
        return;
    if (!InventoryStickerTryLock())
    {
        MarkInventoryStickerDirty();
        return;
    }
    const bool saved = SaveInventoryStickerStoreLocked();
    InventoryStickerUnlock();
    if (!saved)
        MarkInventoryStickerDirty();
}

static void InventoryExtendedOnItemDeleted(unsigned long long itemId)
{
    if (!itemId || !InventoryStickerTryLock())
        return;
    for (int i = 0; i < g_inventoryStickerRecordCount; ++i)
    {
        if (g_inventoryStickerRecords[i].itemId != itemId)
            continue;
        for (int j = i + 1; j < g_inventoryStickerRecordCount; ++j)
            g_inventoryStickerRecords[j - 1] = g_inventoryStickerRecords[j];
        --g_inventoryStickerRecordCount;
        ZeroBytes(&g_inventoryStickerRecords[g_inventoryStickerRecordCount],
            sizeof(InventoryStickerRecord));
        MarkInventoryStickerDirty();
        break;
    }
    InventoryStickerUnlock();
}

static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId)
{
    if (!sourceId || !newId || !InventoryStickerTryLock())
        return;
    InventoryStickerRecord* source = InventoryStickerFindLocked(sourceId, false);
    if (source)
    {
        InventoryStickerRecord copy = *source;
        InventoryStickerRecord* destination = InventoryStickerFindLocked(newId, true);
        if (destination)
        {
            *destination = copy;
            destination->itemId = newId;
            MarkInventoryStickerDirty();
        }
    }
    InventoryStickerUnlock();
}

static bool InventorySelectedItemId(unsigned long long* itemId,
    unsigned short* slotDefinition = nullptr)
{
    if (!itemId || !InventoryTryLock())
        return false;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (!item)
    {
        InventoryUnlock();
        return false;
    }
    *itemId = item->itemId;
    if (slotDefinition)
        *slotDefinition = item->slotDefinitionIndex;
    InventoryUnlock();
    return true;
}

static void InventoryUiOpenStickerModal()
{
    unsigned long long itemId = 0;
    unsigned short slot = 0;
    if (!InventorySelectedItemId(&itemId, &slot))
        return;
    if (slot == INVENTORY_SLOT_GLOVE || slot == INVENTORY_SLOT_MUSIC ||
        slot == INVENTORY_SLOT_AGENT)
        return;
    g_inventoryStickerUiSlot = InventoryClampInt(
        g_inventoryStickerUiSlot, 0, INVENTORY_STICKER_SLOT_COUNT - 1);
    g_inventoryStickerModal = true;
}

static void InventoryUiCloseStickerModal()
{
    g_inventoryStickerModal = false;
}

static void InventoryUiSelectStickerSlot(int slot)
{
    g_inventoryStickerUiSlot = InventoryClampInt(
        slot, 0, INVENTORY_STICKER_SLOT_COUNT - 1);
}

static bool InventoryMutateSelectedSticker(
    void (*mutation)(InventoryStickerSlot*))
{
    if (!mutation)
        return false;
    unsigned long long itemId = 0;
    unsigned short slotDefinition = 0;
    if (!InventorySelectedItemId(&itemId, &slotDefinition) ||
        slotDefinition == INVENTORY_SLOT_GLOVE ||
        slotDefinition == INVENTORY_SLOT_MUSIC ||
        slotDefinition == INVENTORY_SLOT_AGENT ||
        !InventoryStickerTryLock())
        return false;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, true);
    if (!record)
    {
        InventoryStickerUnlock();
        return false;
    }
    InventoryStickerSlot& slot = record->slots[InventoryClampInt(
        g_inventoryStickerUiSlot, 0, INVENTORY_STICKER_SLOT_COUNT - 1)];
    mutation(&slot);
    InventoryStickerSanitize(&slot);
    MarkInventoryStickerDirty();
    InventoryStickerUnlock();
    return true;
}

static int g_inventoryStickerDelta = 0;
static void InventoryStickerAdjustIdMutation(InventoryStickerSlot* slot)
{
    if (!slot) return;
    slot->id = InventoryClampInt(slot->id + g_inventoryStickerDelta, 0, 1000000);
    if (slot->id > 0 && slot->scale <= 0.0f)
        slot->scale = 1.0f;
}

static void InventoryUiAdjustStickerId(int delta)
{
    g_inventoryStickerDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerAdjustIdMutation);
}

static void InventoryStickerScrapeMutation(InventoryStickerSlot* slot)
{
    if (!slot || slot->id <= 0) return;
    const float next = slot->wear + 0.1f;
    if (next > 1.0001f)
    {
        ZeroBytes(slot, sizeof(*slot));
        slot->scale = 1.0f;
        return;
    }
    // 0.1 is exactly the user-facing scrape step; clamp protects accumulated
    // floating point noise without depending on the CRT math library.
    slot->wear = InventoryStickerClamp(next, 0.0f, 1.0f);
}

static void InventoryUiScrapeSticker()
{
    InventoryMutateSelectedSticker(InventoryStickerScrapeMutation);
}

static void InventoryStickerRemoveMutation(InventoryStickerSlot* slot)
{
    if (!slot) return;
    ZeroBytes(slot, sizeof(*slot));
    slot->scale = 1.0f;
}

static void InventoryUiRemoveSticker()
{
    InventoryMutateSelectedSticker(InventoryStickerRemoveMutation);
}

static float g_inventoryStickerFloatDelta = 0.0f;
static int g_inventoryStickerFloatField = 0;
static void InventoryStickerFloatMutation(InventoryStickerSlot* slot)
{
    if (!slot || slot->id <= 0) return;
    if (g_inventoryStickerFloatField == 1)
        slot->rotation = InventoryStickerClamp(
            slot->rotation + g_inventoryStickerFloatDelta, -180.0f, 180.0f);
    else if (g_inventoryStickerFloatField == 2)
        slot->scale = InventoryStickerClamp(
            slot->scale + g_inventoryStickerFloatDelta, 0.1f, 5.0f);
    else if (g_inventoryStickerFloatField == 3)
        slot->offsetX = InventoryStickerClamp(
            slot->offsetX + g_inventoryStickerFloatDelta, -2.0f, 2.0f);
    else if (g_inventoryStickerFloatField == 4)
        slot->offsetY = InventoryStickerClamp(
            slot->offsetY + g_inventoryStickerFloatDelta, -2.0f, 2.0f);
}

static void InventoryUiAdjustStickerRotation(float delta)
{
    g_inventoryStickerFloatField = 1;
    g_inventoryStickerFloatDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerFloatMutation);
}

static void InventoryUiAdjustStickerScale(float delta)
{
    g_inventoryStickerFloatField = 2;
    g_inventoryStickerFloatDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerFloatMutation);
}

static void InventoryUiAdjustStickerOffsetX(float delta)
{
    g_inventoryStickerFloatField = 3;
    g_inventoryStickerFloatDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerFloatMutation);
}

static void InventoryUiAdjustStickerOffsetY(float delta)
{
    g_inventoryStickerFloatField = 4;
    g_inventoryStickerFloatDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerFloatMutation);
}

static int g_inventoryStickerSchemaDelta = 0;
static void InventoryStickerSchemaMutation(InventoryStickerSlot* slot)
{
    if (!slot || slot->id <= 0) return;
    int schema = static_cast<int>(slot->schema) + g_inventoryStickerSchemaDelta;
    while (schema < 0) schema += INVENTORY_STICKER_SLOT_COUNT;
    while (schema >= INVENTORY_STICKER_SLOT_COUNT) schema -= INVENTORY_STICKER_SLOT_COUNT;
    slot->schema = static_cast<BYTE>(schema);
}

static void InventoryUiAdjustStickerSchema(int delta)
{
    g_inventoryStickerSchemaDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerSchemaMutation);
}

static void InventoryUiMoveSticker(int direction)
{
    if (direction == 0)
        return;
    unsigned long long itemId = 0;
    if (!InventorySelectedItemId(&itemId) || !InventoryStickerTryLock())
        return;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, false);
    if (!record)
    {
        InventoryStickerUnlock();
        return;
    }
    const int from = InventoryClampInt(g_inventoryStickerUiSlot,
        0, INVENTORY_STICKER_SLOT_COUNT - 1);
    const int to = from + (direction > 0 ? 1 : -1);
    if (to >= 0 && to < INVENTORY_STICKER_SLOT_COUNT)
    {
        const InventoryStickerSlot tmp = record->slots[from];
        record->slots[from] = record->slots[to];
        record->slots[to] = tmp;
        g_inventoryStickerUiSlot = to;
        MarkInventoryStickerDirty();
    }
    InventoryStickerUnlock();
}

// -------------------------- client econ mirror --------------------------

struct InventorySOID {
    unsigned long long id;
    unsigned int type;
    unsigned int padding;
};

struct InventoryEconRuntime {
    bool resolved;
    bool sharedReady;
    bool attributeReady;
    bool equipReady;
    bool viewReady;
    void* manager;
    unsigned int localInventoryOffset;
    unsigned int sharedObjectCacheOffset;
    void* createEconItem;
    void* createBaseTypeCache;
    void* getEconItemSystem;
    void* getAttributeDefinition;
    void* setDynamicAttribute;
    void* equipItemInLoadout;
    void* getItemInLoadout;
    void* setViewAttributeByName;
    void* setModel;
    unsigned int viewDefinitionOffset;
    unsigned int viewItemIdOffset;
    unsigned int viewItemIdHighOffset;
    unsigned int viewItemIdLowOffset;
    unsigned int viewAccountIdOffset;
    unsigned int viewInitializedOffset;
    unsigned int viewDisallowSocOffset;
    unsigned int viewRestoreMaterialOffset;
};

static InventoryEconRuntime g_inventoryEconRuntime{};

struct InventoryEconMirror {
    bool active;
    bool seen;
    BYTE teams;
    BYTE reserved;
    unsigned short definitionIndex;
    unsigned long long virtualItemId;
    unsigned long long econItemId;
    void* object;
    void* inventory;
    unsigned int signature;
};

static InventoryEconMirror g_inventoryEconMirrors[INVENTORY_EXT_MAX_RECORDS]{};
static int g_inventoryEconMirrorCount = 0;
static void* g_inventoryEconLastInventory = nullptr;
static unsigned int g_inventoryEconCreates = 0;
static unsigned int g_inventoryEconRemoves = 0;
static unsigned int g_inventoryEconFailures = 0;

static BYTE* InventoryResolveRelative(BYTE* instruction,
    int displacementOffset, int instructionSize)
{
    if (!instruction || displacementOffset < 0 || instructionSize <= 0 ||
        !IsAccessible(instruction + displacementOffset, sizeof(LONG), false))
        return nullptr;
    const LONG displacement = *reinterpret_cast<LONG*>(
        instruction + displacementOffset);
    return instruction + instructionSize + displacement;
}

static void* InventoryEconVtableFunction(void* object, int index)
{
    if (!object || index < 0 || !IsAccessible(object, sizeof(void*), false))
        return nullptr;
    void** vtable = *reinterpret_cast<void***>(object);
    if (!vtable || !IsAccessible(vtable + index, sizeof(void*), false))
        return nullptr;
    void* function = vtable[index];
    return IsExecutable(function) ? function : nullptr;
}

static bool ResolveInventoryEconViewOffsets(InventoryEconRuntime* runtime)
{
    if (!runtime)
        return false;
    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;
    SchemaClassInfoView* view = FindSchemaClass(scope, "C_EconItemView");
    if (!view)
        return false;
    unsigned int definition = 0, itemId = 0, high = 0, low = 0;
    unsigned int account = 0, initialized = 0, disallowSoc = 0, restore = 0;
    if (!FindSchemaField(view, "m_iItemDefinitionIndex", &definition) ||
        !FindSchemaField(view, "m_iItemID", &itemId) ||
        !FindSchemaField(view, "m_iItemIDHigh", &high) ||
        !FindSchemaField(view, "m_iItemIDLow", &low) ||
        !FindSchemaField(view, "m_iAccountID", &account) ||
        !FindSchemaField(view, "m_bInitialized", &initialized) ||
        !FindSchemaField(view, "m_bDisallowSOC", &disallowSoc))
        return false;
    const bool hasRestore = FindSchemaField(view,
        "m_bRestoreCustomMaterialAfterPrecache", &restore);
    if (definition + sizeof(unsigned short) > static_cast<unsigned int>(view->size) ||
        itemId + sizeof(unsigned long long) > static_cast<unsigned int>(view->size) ||
        high + sizeof(unsigned int) > static_cast<unsigned int>(view->size) ||
        low + sizeof(unsigned int) > static_cast<unsigned int>(view->size) ||
        account + sizeof(unsigned int) > static_cast<unsigned int>(view->size) ||
        initialized + sizeof(BYTE) > static_cast<unsigned int>(view->size) ||
        disallowSoc + sizeof(BYTE) > static_cast<unsigned int>(view->size) ||
        (hasRestore && restore + sizeof(BYTE) > static_cast<unsigned int>(view->size)))
        return false;
    runtime->viewDefinitionOffset = definition;
    runtime->viewItemIdOffset = itemId;
    runtime->viewItemIdHighOffset = high;
    runtime->viewItemIdLowOffset = low;
    runtime->viewAccountIdOffset = account;
    runtime->viewInitializedOffset = initialized;
    runtime->viewDisallowSocOffset = disallowSoc;
    runtime->viewRestoreMaterialOffset = hasRestore ? restore : 0;
    runtime->viewReady = true;
    return true;
}

static bool ResolveInventoryEconRuntime()
{
    if (g_inventoryEconRuntime.resolved)
        return g_inventoryEconRuntime.sharedReady ||
            g_inventoryEconRuntime.attributeReady;
    g_inventoryEconRuntime = {};
    g_inventoryEconRuntime.resolved = true;
    HMODULE client = GetModuleHandleW(L"client.dll");
    if (!client)
        return false;

    static const int managerPattern[] = {
        0x48,0x8D,0x05,-1,-1,-1,-1,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
        0x0F,0xB6,0x81,0x6B
    };
    static const int inventoryAccessorPattern[] = {
        0x48,0x8B,0x81,-1,-1,-1,-1,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
        0x45,0x33,0xC0,0x3B,0x91,0x10,0x02,0x00,0x00
    };
    static const int sharedCachePattern[] = {
        0x48,0x8B,0x56,-1,0x48,0x85,0xD2,0x74,-1,0x48,0x8B,0x52,0x08,0xEB,-1,
        0x48,0xC7,0xC2,0xFF,0xFF,0xFF,0xFF,0x48,0x8D,0x0D
    };
    static const int createTypeCacheCallPattern[] = {
        0xE8,-1,-1,-1,-1,0x41,0x8B,0xD5,0x49,0x8B,0xCD
    };
    static const int createEconPattern[] = {
        0x48,0x83,0xEC,-1,0xB9,-1,-1,-1,-1,0xE8,-1,-1,-1,-1,
        0x48,0x85,0xC0,0x74,-1,0x48,0x8D,0x0D,-1,-1,-1,-1,0xC7,0x40
    };
    static const int getSystemPattern[] = {
        0x48,0x83,0xEC,0x28,0x48,0x8B,0x05,-1,-1,-1,-1,0x48,0x85,0xC0,
        0x0F,0x85,-1,-1,-1,-1,0x48,0x89,0x5C,0x24
    };
    static const int attributeDefinitionCallPattern[] = {
        0xE8,-1,-1,-1,-1,0x48,0x85,0xC0,0x74,-1,0xE8,-1,-1,-1,-1,
        0x0F,0xB7,0x14,0x3B,0x48,0x8B,0xC8,0xE8,-1,-1,-1,-1,0x0F,0xB6,0x48
    };
    static const int dynamicThunkPattern[] = {
        0xE9,-1,-1,-1,-1,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,
        0xCC,0xCC,0xCC,0xCC,0xCC,0x49,0x8B,0xC0,0x48,0x8B,0xCA,0x48,0x8B,0xD0
    };
    static const int equipPattern[] = {
        0x48,0x89,0x5C,0x24,-1,0x48,0x89,0x6C,0x24,-1,0x48,0x89,0x74,0x24,-1,
        0x89,0x54,0x24,-1,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
        0x48,0x83,0xEC,-1,0x0F,0xB7,0xFA
    };
    static const int getLoadoutPattern[] = {
        0x40,0x55,0x48,0x83,0xEC,-1,0x49,0x63,0xE8,0x83,0xFD,0x38,
        0x0F,0x87,-1,-1,-1,-1,0x83,0xFA,0x03
    };
    static const int viewAttributeCallPattern[] = {
        0xE8,-1,-1,-1,-1,0x66,0x41,0x0F,0x6E,0xD4
    };

    BYTE* manager = FindUniquePattern(client, managerPattern,
        sizeof(managerPattern) / sizeof(managerPattern[0]));
    BYTE* inventoryAccessor = FindUniquePattern(client, inventoryAccessorPattern,
        sizeof(inventoryAccessorPattern) / sizeof(inventoryAccessorPattern[0]));
    BYTE* sharedCache = FindUniquePattern(client, sharedCachePattern,
        sizeof(sharedCachePattern) / sizeof(sharedCachePattern[0]));
    BYTE* createTypeCacheCall = FindUniquePattern(client, createTypeCacheCallPattern,
        sizeof(createTypeCacheCallPattern) / sizeof(createTypeCacheCallPattern[0]));
    BYTE* createEcon = FindUniquePattern(client, createEconPattern,
        sizeof(createEconPattern) / sizeof(createEconPattern[0]));
    BYTE* getSystem = FindUniquePattern(client, getSystemPattern,
        sizeof(getSystemPattern) / sizeof(getSystemPattern[0]));
    BYTE* attributeDefinitionCall = FindUniquePattern(client,
        attributeDefinitionCallPattern,
        sizeof(attributeDefinitionCallPattern) / sizeof(attributeDefinitionCallPattern[0]));
    BYTE* dynamicThunk = FindUniquePattern(client, dynamicThunkPattern,
        sizeof(dynamicThunkPattern) / sizeof(dynamicThunkPattern[0]));
    BYTE* equip = FindUniquePattern(client, equipPattern,
        sizeof(equipPattern) / sizeof(equipPattern[0]));
    BYTE* getLoadout = FindUniquePattern(client, getLoadoutPattern,
        sizeof(getLoadoutPattern) / sizeof(getLoadoutPattern[0]));
    BYTE* viewAttributeCall = FindUniquePattern(client, viewAttributeCallPattern,
        sizeof(viewAttributeCallPattern) / sizeof(viewAttributeCallPattern[0]));

    if (manager && IsAccessible(manager + 3, sizeof(LONG), false))
        g_inventoryEconRuntime.manager = InventoryResolveRelative(manager, 3, 7);
    if (inventoryAccessor && IsAccessible(inventoryAccessor + 3, sizeof(LONG), false))
        g_inventoryEconRuntime.localInventoryOffset =
            static_cast<unsigned int>(*reinterpret_cast<LONG*>(inventoryAccessor + 3));
    if (sharedCache && IsAccessible(sharedCache + 3, sizeof(BYTE), false))
        g_inventoryEconRuntime.sharedObjectCacheOffset = sharedCache[3];
    g_inventoryEconRuntime.createBaseTypeCache =
        InventoryResolveRelative(createTypeCacheCall, 1, 5);
    g_inventoryEconRuntime.createEconItem = createEcon;
    g_inventoryEconRuntime.getEconItemSystem = getSystem;
    g_inventoryEconRuntime.getAttributeDefinition =
        InventoryResolveRelative(attributeDefinitionCall, 1, 5);
    g_inventoryEconRuntime.setDynamicAttribute =
        InventoryResolveRelative(dynamicThunk, 1, 5);
    g_inventoryEconRuntime.equipItemInLoadout = equip;
    g_inventoryEconRuntime.getItemInLoadout = getLoadout;
    g_inventoryEconRuntime.setViewAttributeByName =
        InventoryResolveRelative(viewAttributeCall, 1, 5);

    const bool managerValid = g_inventoryEconRuntime.manager &&
        IsAccessible(g_inventoryEconRuntime.manager, sizeof(void*), false);
    const bool offsetsValid =
        g_inventoryEconRuntime.localInventoryOffset >= 0x1000 &&
        g_inventoryEconRuntime.localInventoryOffset <= 0x100000 &&
        g_inventoryEconRuntime.sharedObjectCacheOffset >= 0x20 &&
        g_inventoryEconRuntime.sharedObjectCacheOffset <= 0x200;
    const bool functionsValid = g_inventoryEconRuntime.createEconItem &&
        IsExecutable(g_inventoryEconRuntime.createEconItem) &&
        g_inventoryEconRuntime.createBaseTypeCache &&
        IsExecutable(g_inventoryEconRuntime.createBaseTypeCache);
    g_inventoryEconRuntime.sharedReady =
        managerValid && offsetsValid && functionsValid;
    g_inventoryEconRuntime.attributeReady =
        g_inventoryEconRuntime.getEconItemSystem &&
        IsExecutable(g_inventoryEconRuntime.getEconItemSystem) &&
        g_inventoryEconRuntime.getAttributeDefinition &&
        IsExecutable(g_inventoryEconRuntime.getAttributeDefinition) &&
        g_inventoryEconRuntime.setDynamicAttribute &&
        IsExecutable(g_inventoryEconRuntime.setDynamicAttribute);
    g_inventoryEconRuntime.equipReady =
        g_inventoryEconRuntime.equipItemInLoadout &&
        IsExecutable(g_inventoryEconRuntime.equipItemInLoadout) &&
        g_inventoryEconRuntime.getItemInLoadout &&
        IsExecutable(g_inventoryEconRuntime.getItemInLoadout);
    ResolveInventoryEconViewOffsets(&g_inventoryEconRuntime);
    return g_inventoryEconRuntime.sharedReady ||
        g_inventoryEconRuntime.attributeReady;
}

static bool InventoryEconResolveInventory(void** inventory, InventorySOID* owner)
{
    if (!inventory || !owner || !g_inventoryEconRuntime.sharedReady)
        return false;
    *inventory = nullptr;
    ZeroBytes(owner, sizeof(*owner));
    BYTE* manager = reinterpret_cast<BYTE*>(g_inventoryEconRuntime.manager);
    void** field = reinterpret_cast<void**>(manager +
        g_inventoryEconRuntime.localInventoryOffset);
    if (!IsAccessible(field, sizeof(void*), false) || !*field)
        return false;
    void* localInventory = *field;
    if (!IsAccessible(localInventory, 0x20, false))
        return false;
    *owner = *reinterpret_cast<InventorySOID*>(
        reinterpret_cast<BYTE*>(localInventory) + 0x10);
    if (!owner->id)
        return false;
    *inventory = localInventory;
    return true;
}

static void* InventoryEconTypeCache(void* inventory)
{
    if (!inventory || !g_inventoryEconRuntime.sharedReady)
        return nullptr;
    void** cacheField = reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(inventory) +
        g_inventoryEconRuntime.sharedObjectCacheOffset);
    if (!IsAccessible(cacheField, sizeof(void*), false) || !*cacheField)
        return nullptr;
    __try
    {
        using Fn = void* (*)(void*, int);
        return reinterpret_cast<Fn>(g_inventoryEconRuntime.createBaseTypeCache)(
            *cacheField, INVENTORY_ECON_TYPE_ITEM);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static bool InventoryEconSetDynamic(void* object, int index, const void* value)
{
    if (!object || !value || !g_inventoryEconRuntime.attributeReady)
        return false;
    __try
    {
        using GetSystemFn = void* (*)();
        void* system = reinterpret_cast<GetSystemFn>(
            g_inventoryEconRuntime.getEconItemSystem)();
        if (!system || !IsAccessible(reinterpret_cast<BYTE*>(system) +
            sizeof(void*), sizeof(void*), false))
            return false;
        void* schema = *reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(system) + sizeof(void*));
        if (!schema)
            return false;
        using GetDefinitionFn = void* (*)(void*, int);
        void* definition = reinterpret_cast<GetDefinitionFn>(
            g_inventoryEconRuntime.getAttributeDefinition)(schema, index);
        if (!definition)
            return false;
        using SetFn = void (*)(void*, void*, const void*);
        reinterpret_cast<SetFn>(g_inventoryEconRuntime.setDynamicAttribute)(
            object, definition, value);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static bool InventoryEconApplyAttributes(void* object,
    const VirtualInventoryItem& item)
{
    if (!object)
        return false;
    bool ok = true;
    if (item.paintKit > 0)
    {
        const float paint = static_cast<float>(item.paintKit);
        const float seed = static_cast<float>(InventoryClampInt(item.seed, 0, 1000));
        const float wear = InventoryClampWear(item.wear);
        ok = InventoryEconSetDynamic(object, 6, &paint) && ok;
        ok = InventoryEconSetDynamic(object, 7, &seed) && ok;
        ok = InventoryEconSetDynamic(object, 8, &wear) && ok;
    }
    if (item.statTrak >= 0)
    {
        const int score = InventoryClampInt(item.statTrak, 0, 999999);
        const int type = 0;
        ok = InventoryEconSetDynamic(object, 80, &score) && ok;
        ok = InventoryEconSetDynamic(object, 81, &type) && ok;
    }

    InventoryStickerRecord stickers;
    InventoryStickerCopyRecord(item.itemId, &stickers);
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        InventoryStickerSlot slot = stickers.slots[i];
        InventoryStickerSanitize(&slot);
        if (slot.id <= 0)
            continue;
        const int idIndex = 113 + i * 4;
        const int id = slot.id;
        const float wear = slot.wear;
        const float scale = slot.scale;
        const float rotation = slot.rotation;
        const int schema = static_cast<int>(slot.schema);
        ok = InventoryEconSetDynamic(object, idIndex, &id) && ok;
        ok = InventoryEconSetDynamic(object, idIndex + 1, &wear) && ok;
        ok = InventoryEconSetDynamic(object, idIndex + 2, &scale) && ok;
        ok = InventoryEconSetDynamic(object, idIndex + 3, &rotation) && ok;
        ok = InventoryEconSetDynamic(object, 278 + i * 2, &slot.offsetX) && ok;
        ok = InventoryEconSetDynamic(object, 279 + i * 2, &slot.offsetY) && ok;
        ok = InventoryEconSetDynamic(object, 290 + i, &schema) && ok;
    }
    return ok;
}

static unsigned int InventoryEconItemSignature(
    const VirtualInventoryItem& item)
{
    unsigned int hash = 2166136261u;
    const BYTE* data = reinterpret_cast<const BYTE*>(&item);
    for (SIZE_T i = 0; i < sizeof(item); ++i)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }
    InventoryStickerRecord stickers;
    InventoryStickerCopyRecord(item.itemId, &stickers);
    const BYTE* stickerBytes = reinterpret_cast<const BYTE*>(&stickers.slots[0]);
    for (SIZE_T i = 0; i < sizeof(stickers.slots); ++i)
    {
        hash ^= stickerBytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static InventoryEconMirror* InventoryEconFindMirror(unsigned long long virtualId)
{
    for (int i = 0; i < g_inventoryEconMirrorCount; ++i)
        if (g_inventoryEconMirrors[i].virtualItemId == virtualId)
            return &g_inventoryEconMirrors[i];
    return nullptr;
}

static bool InventoryEconNotify(void* inventory, int vtableIndex,
    InventorySOID owner, void* object)
{
    void* function = InventoryEconVtableFunction(inventory, vtableIndex);
    if (!function)
        return false;
    __try
    {
        using Fn = void (*)(void*, InventorySOID, void*, int);
        reinterpret_cast<Fn>(function)(inventory, owner, object,
            INVENTORY_SO_EVENT_INCREMENTAL);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void InventoryEconDestroyMirror(InventoryEconMirror* mirror,
    bool inventoryKnownValid)
{
    if (!mirror || !mirror->active)
        return;
    if (inventoryKnownValid && mirror->inventory && mirror->object)
    {
        InventorySOID owner{};
        void* current = nullptr;
        if (InventoryEconResolveInventory(&current, &owner) &&
            current == mirror->inventory)
        {
            void* typeCache = InventoryEconTypeCache(current);
            void* removeObject = InventoryEconVtableFunction(typeCache, 3);
            InventoryEconNotify(current, 2, owner, mirror->object);
            if (removeObject)
            {
                __try
                {
                    using Fn = void* (*)(void*, void*);
                    reinterpret_cast<Fn>(removeObject)(typeCache, mirror->object);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            void* destructor = InventoryEconVtableFunction(mirror->object, 1);
            if (destructor)
            {
                __try
                {
                    using Fn = void (*)(void*, bool);
                    reinterpret_cast<Fn>(destructor)(mirror->object, true);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            ++g_inventoryEconRemoves;
        }
    }
    ZeroBytes(mirror, sizeof(*mirror));
}

static int InventoryEconFindLoadoutSlot(void* inventory, int team,
    unsigned short definition)
{
    if (!inventory || !g_inventoryEconRuntime.equipReady ||
        (team != 2 && team != 3))
        return -1;
    __try
    {
        using Fn = void* (*)(void*, int, int);
        Fn getItem = reinterpret_cast<Fn>(g_inventoryEconRuntime.getItemInLoadout);
        for (int slot = 0; slot < 57; ++slot)
        {
            void* view = getItem(inventory, team, slot);
            if (!view || !g_inventoryEconRuntime.viewReady)
                continue;
            unsigned short* field = reinterpret_cast<unsigned short*>(
                reinterpret_cast<BYTE*>(view) +
                g_inventoryEconRuntime.viewDefinitionOffset);
            if (IsAccessible(field, sizeof(unsigned short), false) &&
                *field == definition)
                return slot;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
    return -1;
}

static int InventoryEconSlotForItem(void* inventory,
    const VirtualInventoryItem& item, int team)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        return INVENTORY_AGENT_LOADOUT_SLOT;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        return INVENTORY_GLOVE_LOADOUT_SLOT;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return INVENTORY_MUSIC_LOADOUT_SLOT;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        return 0;
    return InventoryEconFindLoadoutSlot(inventory, team,
        item.overrideDefinitionIndex);
}

static void InventoryEconEquipMirror(const InventoryEconMirror& mirror,
    const VirtualInventoryItem& item)
{
    if (!g_inventoryEconRuntime.equipReady ||
        !g_inventoryEconRuntime.manager || !mirror.inventory)
        return;
    __try
    {
        using Fn = bool (*)(void*, int, int, unsigned long long);
        Fn equip = reinterpret_cast<Fn>(g_inventoryEconRuntime.equipItemInLoadout);
        if ((item.equippedTeams & INVENTORY_TEAM_T) != 0)
        {
            const int slot = InventoryEconSlotForItem(mirror.inventory, item, 2);
            if (slot >= 0)
                equip(g_inventoryEconRuntime.manager, 2, slot, mirror.econItemId);
        }
        if ((item.equippedTeams & INVENTORY_TEAM_CT) != 0)
        {
            const int slot = InventoryEconSlotForItem(mirror.inventory, item, 3);
            if (slot >= 0)
                equip(g_inventoryEconRuntime.manager, 3, slot, mirror.econItemId);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static bool InventoryEconCreateMirror(const VirtualInventoryItem& item,
    void* inventory, InventorySOID owner, unsigned int signature)
{
    if (!inventory || !owner.id || g_inventoryEconMirrorCount >= INVENTORY_EXT_MAX_RECORDS ||
        !g_inventoryEconRuntime.sharedReady || !g_inventoryEconRuntime.attributeReady)
        return false;
    __try
    {
        using CreateFn = void* (*)();
        void* object = reinterpret_cast<CreateFn>(
            g_inventoryEconRuntime.createEconItem)();
        if (!object || !InventoryEconVtableFunction(object, 1) ||
            !IsAccessible(reinterpret_cast<BYTE*>(object) + 0x30,
                sizeof(unsigned short), true))
            return false;

        const unsigned long long econId = 0x0E00000000000000ULL |
            (item.itemId & 0x0000FFFFFFFFFFFFULL);
        const unsigned int inventoryPosition = 0x40000000u |
            static_cast<unsigned int>(econId & 0x00FFFFFFu);
        *reinterpret_cast<unsigned long long*>(
            reinterpret_cast<BYTE*>(object) + 0x10) = econId;
        *reinterpret_cast<unsigned long long*>(
            reinterpret_cast<BYTE*>(object) + 0x18) = econId;
        // 0x20 is the factory-owned custom-data object; intentionally untouched.
        *reinterpret_cast<unsigned int*>(
            reinterpret_cast<BYTE*>(object) + 0x28) =
            static_cast<unsigned int>(owner.id);
        *reinterpret_cast<unsigned int*>(
            reinterpret_cast<BYTE*>(object) + 0x2C) = inventoryPosition;
        *reinterpret_cast<unsigned short*>(
            reinterpret_cast<BYTE*>(object) + 0x30) = item.overrideDefinitionIndex;

        if (!InventoryEconApplyAttributes(object, item))
        {
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }
        void* typeCache = InventoryEconTypeCache(inventory);
        void* addObject = InventoryEconVtableFunction(typeCache, 1);
        if (!typeCache || !addObject)
            return false;
        using AddFn = bool (*)(void*, void*);
        if (!reinterpret_cast<AddFn>(addObject)(typeCache, object))
            return false;
        if (!InventoryEconNotify(inventory, 0, owner, object))
        {
            void* removeObject = InventoryEconVtableFunction(typeCache, 3);
            if (removeObject)
                reinterpret_cast<void* (*)(void*, void*)>(removeObject)(typeCache, object);
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }

        InventoryEconMirror& mirror =
            g_inventoryEconMirrors[g_inventoryEconMirrorCount++];
        ZeroBytes(&mirror, sizeof(mirror));
        mirror.active = true;
        mirror.seen = true;
        mirror.virtualItemId = item.itemId;
        mirror.econItemId = econId;
        mirror.object = object;
        mirror.inventory = inventory;
        mirror.signature = signature;
        mirror.definitionIndex = item.overrideDefinitionIndex;
        mirror.teams = item.equippedTeams;
        InventoryEconEquipMirror(mirror, item);
        InventoryEconNotify(inventory, 1, owner, object);
        ++g_inventoryEconCreates;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ++g_inventoryEconFailures;
        return false;
    }
}

static void InventoryEconRemoveMirrorAt(int index, bool validInventory)
{
    if (index < 0 || index >= g_inventoryEconMirrorCount)
        return;
    InventoryEconDestroyMirror(&g_inventoryEconMirrors[index], validInventory);
    for (int i = index + 1; i < g_inventoryEconMirrorCount; ++i)
        g_inventoryEconMirrors[i - 1] = g_inventoryEconMirrors[i];
    --g_inventoryEconMirrorCount;
    ZeroBytes(&g_inventoryEconMirrors[g_inventoryEconMirrorCount],
        sizeof(InventoryEconMirror));
}

static void UpdateInventoryEconBackend()
{
    if (!ResolveInventoryEconRuntime() || !g_inventoryEconRuntime.sharedReady ||
        !g_inventoryEconRuntime.attributeReady)
        return;
    void* inventory = nullptr;
    InventorySOID owner{};
    if (!InventoryEconResolveInventory(&inventory, &owner))
        return;
    if (g_inventoryEconLastInventory && g_inventoryEconLastInventory != inventory)
    {
        // The old cache belongs to another local-inventory lifetime. Do not call
        // destructors through stale objects; simply forget those mirrors.
        g_inventoryEconMirrorCount = 0;
        ZeroBytes(g_inventoryEconMirrors, sizeof(g_inventoryEconMirrors));
    }
    g_inventoryEconLastInventory = inventory;

    bool enabled = false;
    VirtualInventoryItem items[INVENTORY_MAX_ITEMS];
    int count = 0;
    if (!InventoryTryLock())
        return;
    enabled = g_inventoryStore.enabled;
    count = InventoryClampInt(g_inventoryStore.itemCount, 0, INVENTORY_MAX_ITEMS);
    for (int i = 0; i < count; ++i)
        items[i] = g_inventoryStore.items[i];
    InventoryUnlock();

    for (int i = 0; i < g_inventoryEconMirrorCount; ++i)
        g_inventoryEconMirrors[i].seen = false;

    if (enabled)
    {
        for (int i = 0; i < count; ++i)
        {
            const VirtualInventoryItem& item = items[i];
            // Current music virtual items store the music-kit ID rather than the
            // inventory item definition; keep the proven controller adapter for
            // this domain instead of fabricating the wrong CEconItem definition.
            if (!item.itemId || item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
                continue;
            const unsigned int signature = InventoryEconItemSignature(item);
            InventoryEconMirror* mirror = InventoryEconFindMirror(item.itemId);
            if (mirror && (mirror->signature != signature ||
                mirror->inventory != inventory))
            {
                const int index = static_cast<int>(mirror - g_inventoryEconMirrors);
                InventoryEconRemoveMirrorAt(index, mirror->inventory == inventory);
                mirror = nullptr;
            }
            if (!mirror)
            {
                if (!InventoryEconCreateMirror(item, inventory, owner, signature))
                {
                    ++g_inventoryEconFailures;
                    continue;
                }
                mirror = InventoryEconFindMirror(item.itemId);
            }
            if (mirror)
            {
                mirror->seen = true;
                if (mirror->teams != item.equippedTeams)
                {
                    mirror->teams = item.equippedTeams;
                    InventoryEconEquipMirror(*mirror, item);
                    InventoryEconNotify(inventory, 1, owner, mirror->object);
                }
            }
        }
    }

    for (int i = g_inventoryEconMirrorCount - 1; i >= 0; --i)
        if (!g_inventoryEconMirrors[i].seen)
            InventoryEconRemoveMirrorAt(i, true);
}

static bool InventoryEconBindItemView(BYTE* view,
    unsigned long long virtualItemId)
{
    if (!view || !virtualItemId || !g_inventoryEconRuntime.viewReady)
        return false;
    InventoryEconMirror* mirror = InventoryEconFindMirror(virtualItemId);
    if (!mirror || !mirror->active || !mirror->econItemId)
        return false;
    const unsigned long long id = mirror->econItemId;
    const unsigned int high = static_cast<unsigned int>(id >> 32);
    const unsigned int low = static_cast<unsigned int>(id);
    InventorySOID owner{};
    void* inventory = nullptr;
    if (!InventoryEconResolveInventory(&inventory, &owner))
        return false;
    if (!IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset,
            sizeof(unsigned long long), true) ||
        !IsAccessible(view + g_inventoryEconRuntime.viewItemIdHighOffset,
            sizeof(unsigned int), true) ||
        !IsAccessible(view + g_inventoryEconRuntime.viewItemIdLowOffset,
            sizeof(unsigned int), true) ||
        !IsAccessible(view + g_inventoryEconRuntime.viewAccountIdOffset,
            sizeof(unsigned int), true))
        return false;
    *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset) = id;
    *reinterpret_cast<unsigned int*>(view +
        g_inventoryEconRuntime.viewItemIdHighOffset) = high;
    *reinterpret_cast<unsigned int*>(view +
        g_inventoryEconRuntime.viewItemIdLowOffset) = low;
    *reinterpret_cast<unsigned int*>(view +
        g_inventoryEconRuntime.viewAccountIdOffset) =
        static_cast<unsigned int>(owner.id);
    if (IsAccessible(view + g_inventoryEconRuntime.viewInitializedOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewInitializedOffset) = 1;
    if (IsAccessible(view + g_inventoryEconRuntime.viewDisallowSocOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewDisallowSocOffset) = 0;
    if (g_inventoryEconRuntime.viewRestoreMaterialOffset &&
        IsAccessible(view + g_inventoryEconRuntime.viewRestoreMaterialOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewRestoreMaterialOffset) = 1;
    return true;
}

// -------------------------- agent domain --------------------------

struct InventoryAgentCatalogEntry {
    unsigned short definitionIndex;
    BYTE teamMask;
    const wchar_t* displayName;
};

static const InventoryAgentCatalogEntry kInventoryAgents[] = {
    { 5200, INVENTORY_TEAM_T,  L"Phoenix" },
    { 5100, INVENTORY_TEAM_T,  L"Leet Crew" },
    { 5088, INVENTORY_TEAM_T,  L"Balkan" },
    { 5048, INVENTORY_TEAM_T,  L"Professional" },
    { 5038, INVENTORY_TEAM_T,  L"Anarchist" },
    { 5600, INVENTORY_TEAM_CT, L"SAS" },
    { 5300, INVENTORY_TEAM_CT, L"FBI" },
    { 5074, INVENTORY_TEAM_CT, L"SWAT" },
    { 5058, INVENTORY_TEAM_CT, L"GIGN" },
    { 5063, INVENTORY_TEAM_CT, L"GSG-9" },
    { 5068, INVENTORY_TEAM_CT, L"IDF" },
    { 5083, INVENTORY_TEAM_CT, L"SEAL Team 6" }
};

static int InventoryAgentCatalogCount()
{
    return static_cast<int>(sizeof(kInventoryAgents) / sizeof(kInventoryAgents[0]));
}

static const InventoryAgentCatalogEntry* InventoryFindAgent(unsigned short definition)
{
    for (int i = 0; i < InventoryAgentCatalogCount(); ++i)
        if (kInventoryAgents[i].definitionIndex == definition)
            return &kInventoryAgents[i];
    return nullptr;
}

static const wchar_t* InventoryAgentName(unsigned short definition)
{
    const InventoryAgentCatalogEntry* agent = InventoryFindAgent(definition);
    return agent ? agent->displayName : L"Raw agent definition";
}

static const wchar_t* InventoryCatalogFullDomainName(
    unsigned short slotDefinitionIndex, unsigned short definitionIndex)
{
    if (slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        return InventoryAgentName(definitionIndex);
    return InventoryCatalogDomainName(slotDefinitionIndex, definitionIndex);
}

static unsigned short InventoryNextAgentDefinition(unsigned short current,
    BYTE desiredTeams, int direction)
{
    if (direction == 0 || InventoryAgentCatalogCount() <= 0)
        return current;
    int currentIndex = -1;
    for (int i = 0; i < InventoryAgentCatalogCount(); ++i)
        if (kInventoryAgents[i].definitionIndex == current)
        {
            currentIndex = i;
            break;
        }
    for (int step = 1; step <= InventoryAgentCatalogCount(); ++step)
    {
        int index = currentIndex < 0 ?
            (direction > 0 ? step - 1 : InventoryAgentCatalogCount() - step) :
            currentIndex + (direction > 0 ? step : -step);
        while (index < 0) index += InventoryAgentCatalogCount();
        while (index >= InventoryAgentCatalogCount()) index -= InventoryAgentCatalogCount();
        if ((kInventoryAgents[index].teamMask & desiredTeams) != 0)
            return kInventoryAgents[index].definitionIndex;
    }
    return current;
}

static void InventoryUiAddAgentPreset()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        ZeroBytes(&item, sizeof(item));
        item.itemId = g_inventoryStore.nextItemId++;
        item.slotDefinitionIndex = INVENTORY_SLOT_AGENT;
        const BYTE active = InventoryTeamBit(g_inventoryActiveTeam);
        item.equippedTeams = active == INVENTORY_TEAM_NONE ? INVENTORY_TEAM_BOTH : active;
        item.overrideDefinitionIndex =
            (item.equippedTeams & INVENTORY_TEAM_CT) != 0 &&
            (item.equippedTeams & INVENTORY_TEAM_T) == 0 ? 5600 : 5200;
        item.statTrak = -1;
        item.wear = 0.0001f;
        item.quality = 4;
        const int index = g_inventoryStore.itemCount++;
        g_inventoryStore.items[index] = item;
        InventoryEquipItemLocked(index, item.equippedTeams);
        g_inventoryUiSelected = index;
        g_inventoryUiPage = index / 7;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiCycleAgentCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_AGENT)
    {
        BYTE teams = item->equippedTeams & INVENTORY_TEAM_BOTH;
        if (teams == INVENTORY_TEAM_NONE)
            teams = INVENTORY_TEAM_BOTH;
        item->overrideDefinitionIndex = InventoryNextAgentDefinition(
            item->overrideDefinitionIndex, teams, direction);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

// -------------------------- StatTrak swap --------------------------

static int g_inventoryStatTrakSwapSource = -1;

static bool InventoryCanSwapStatTrak(const VirtualInventoryItem& from,
    const VirtualInventoryItem& to)
{
    if (from.statTrak < 0 || to.statTrak < 0)
        return false;
    if (from.slotDefinitionIndex == INVENTORY_SLOT_MUSIC &&
        to.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return true;
    if (from.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ||
        from.slotDefinitionIndex == INVENTORY_SLOT_AGENT ||
        to.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ||
        to.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        return false;
    return from.overrideDefinitionIndex == to.overrideDefinitionIndex;
}

static void InventoryUiArmStatTrakSwap()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* selected = InventorySelectedLocked();
    if (selected && selected->statTrak >= 0)
        g_inventoryStatTrakSwapSource = g_inventoryUiSelected;
    else
        g_inventoryStatTrakSwapSource = -1;
    InventoryUnlock();
}

static bool InventoryUiCompleteStatTrakSwap(int targetIndex)
{
    if (!InventoryTryLock())
        return false;
    const int sourceIndex = g_inventoryStatTrakSwapSource;
    g_inventoryStatTrakSwapSource = -1;
    if (sourceIndex < 0 || targetIndex < 0 || sourceIndex == targetIndex ||
        sourceIndex >= g_inventoryStore.itemCount ||
        targetIndex >= g_inventoryStore.itemCount)
    {
        InventoryUnlock();
        return false;
    }
    VirtualInventoryItem& from = g_inventoryStore.items[sourceIndex];
    VirtualInventoryItem& to = g_inventoryStore.items[targetIndex];
    if (!InventoryCanSwapStatTrak(from, to))
    {
        InventoryUnlock();
        return false;
    }
    const int tmp = from.statTrak;
    from.statTrak = to.statTrak;
    to.statTrak = tmp;
    g_inventoryUiSelected = targetIndex;
    g_inventoryUiPage = targetIndex / 7;
    MarkInventoryDirty();
    InventoryUnlock();
    return true;
}

// -------------------------- search / custom-name input --------------------------

static volatile LONG g_inventoryTextMode = INVENTORY_TEXT_NONE;
static char g_inventorySearchText[32]{};
static BYTE g_inventorySearchLength = 0;
static WNDPROC g_inventoryOriginalGameWndProc = nullptr;

static bool InventoryAsciiContainsInsensitive(const char* text, const char* needle)
{
    if (!needle || !needle[0])
        return true;
    if (!text)
        return false;
    for (int start = 0; text[start]; ++start)
    {
        int i = 0;
        while (needle[i] && text[start + i])
        {
            char a = text[start + i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
            if (a != b) break;
            ++i;
        }
        if (!needle[i])
            return true;
    }
    return false;
}

static void InventoryUnsignedToAscii(unsigned int value, char* out, int capacity)
{
    if (!out || capacity <= 1)
        return;
    char reverse[16];
    int count = 0;
    do
    {
        reverse[count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    } while (value && count < static_cast<int>(sizeof(reverse)));
    int written = 0;
    while (count > 0 && written + 1 < capacity)
        out[written++] = reverse[--count];
    out[written] = 0;
}

static bool InventorySearchMatches(const VirtualInventoryItem& item)
{
    if (g_inventorySearchLength == 0)
        return true;
    char number[16];
    InventoryUnsignedToAscii(item.overrideDefinitionIndex, number,
        static_cast<int>(sizeof(number)));
    if (InventoryAsciiContainsInsensitive(number, g_inventorySearchText) ||
        InventoryAsciiContainsInsensitive(item.customName, g_inventorySearchText))
        return true;
    const char* domain = "weapon";
    if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE) domain = "knife";
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE) domain = "gloves";
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC) domain = "music";
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT) domain = "agent";
    return InventoryAsciiContainsInsensitive(domain, g_inventorySearchText);
}

static bool InventoryGetFilteredUiSnapshot(InventoryUiSnapshot* snapshot)
{
    if (!snapshot)
        return false;
    ZeroBytes(snapshot, sizeof(*snapshot));
    if (!InventoryTryLock())
        return false;
    snapshot->enabled = g_inventoryStore.enabled;
    snapshot->itemCount = g_inventoryStore.itemCount;
    snapshot->selectedIndex = g_inventoryUiSelected;
    snapshot->activeDefinitionIndex = g_inventoryActiveDefinitionIndex;
    snapshot->activeTeam = g_inventoryActiveTeam;
    snapshot->appliedCount = g_inventoryLastAppliedCount;
    snapshot->ownedEconCount = g_inventoryLastOwnedEconCount;
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
    {
        snapshot->selectedItem = g_inventoryStore.items[g_inventoryUiSelected];
        snapshot->hasSelectedItem = true;
    }

    int matchingCount = 0;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
        if (InventorySearchMatches(g_inventoryStore.items[i]))
            ++matchingCount;
    const int maxPage = matchingCount > 0 ? (matchingCount - 1) / 7 : 0;
    g_inventoryUiPage = InventoryClampInt(g_inventoryUiPage, 0, maxPage);
    snapshot->page = g_inventoryUiPage;
    const int firstMatch = g_inventoryUiPage * 7;
    int matchIndex = 0;
    for (int i = 0; i < g_inventoryStore.itemCount && snapshot->visibleCount < 7; ++i)
    {
        if (!InventorySearchMatches(g_inventoryStore.items[i]))
            continue;
        if (matchIndex++ < firstMatch)
            continue;
        snapshot->visibleItems[snapshot->visibleCount] = g_inventoryStore.items[i];
        snapshot->visibleIndices[snapshot->visibleCount] = i;
        ++snapshot->visibleCount;
    }
    InventoryUnlock();
    return true;
}

static void InventoryUiBeginNameEdit()
{
    if (!InventoryTryLock())
        return;
    const bool valid = InventorySelectedLocked() != nullptr;
    InventoryUnlock();
    if (valid)
        AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NAME);
}

static void InventoryUiBeginSearchEdit()
{
    AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_SEARCH);
}

static void InventoryUiClearSearch()
{
    g_inventorySearchLength = 0;
    g_inventorySearchText[0] = 0;
    g_inventoryUiPage = 0;
}

static void InventoryFinishNameEdit()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        while (item->customNameLength > 0 &&
            item->customName[item->customNameLength - 1] == ' ')
            --item->customNameLength;
        item->customName[item->customNameLength] = 0;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryTextBackspace(int mode)
{
    if (mode == INVENTORY_TEXT_SEARCH)
    {
        if (g_inventorySearchLength > 0)
            g_inventorySearchText[--g_inventorySearchLength] = 0;
        g_inventoryUiPage = 0;
        return;
    }
    if (mode != INVENTORY_TEXT_NAME || !InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->customNameLength > 0)
    {
        item->customName[--item->customNameLength] = 0;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryTextAppend(int mode, char character)
{
    if (character < 0x20 || character > 0x7E)
        return;
    if (mode == INVENTORY_TEXT_SEARCH)
    {
        if (g_inventorySearchLength + 1 < sizeof(g_inventorySearchText))
        {
            g_inventorySearchText[g_inventorySearchLength++] = character;
            g_inventorySearchText[g_inventorySearchLength] = 0;
            g_inventoryUiPage = 0;
        }
        return;
    }
    if (mode != INVENTORY_TEXT_NAME || !InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->customNameLength < 20 &&
        item->customNameLength + 1 < INVENTORY_CUSTOM_NAME_CAPACITY)
    {
        if (item->customNameLength != 0 || character != ' ')
        {
            item->customName[item->customNameLength++] = character;
            item->customName[item->customNameLength] = 0;
            MarkInventoryDirty();
        }
    }
    InventoryUnlock();
}

static LRESULT CALLBACK InventoryGameWindowProc(HWND wnd, UINT msg,
    WPARAM wParam, LPARAM lParam)
{
    const int mode = static_cast<int>(g_inventoryTextMode);
    if (mode != INVENTORY_TEXT_NONE)
    {
        if (msg == 0x0102 /* WM_CHAR */)
        {
            const unsigned int value = static_cast<unsigned int>(wParam);
            if (value == 8)
                InventoryTextBackspace(mode);
            else if (value == 13)
            {
                if (mode == INVENTORY_TEXT_NAME)
                    InventoryFinishNameEdit();
                AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NONE);
            }
            else if (value >= 0x20 && value <= 0x7E)
                InventoryTextAppend(mode, static_cast<char>(value));
            if (g_menuWindow)
                InvalidateRect(g_menuWindow, nullptr, FALSE);
            return 0;
        }
        if (msg == 0x0100 /* WM_KEYDOWN */ && wParam == 0x1B /* ESC */)
        {
            if (mode == INVENTORY_TEXT_NAME)
                InventoryFinishNameEdit();
            AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NONE);
            if (g_menuWindow)
                InvalidateRect(g_menuWindow, nullptr, FALSE);
            return 0;
        }
    }
    return g_inventoryOriginalGameWndProc ?
        CallWindowProcW(g_inventoryOriginalGameWndProc, wnd, msg, wParam, lParam) :
        DefWindowProcW(wnd, msg, wParam, lParam);
}

static void InventoryInstallTextCapture()
{
    if (!g_gameWindow || g_inventoryOriginalGameWndProc)
        return;
    const LONG_PTR previous = SetWindowLongPtrW(g_gameWindow, -4 /* GWLP_WNDPROC */,
        reinterpret_cast<LONG_PTR>(InventoryGameWindowProc));
    if (previous)
        g_inventoryOriginalGameWndProc = reinterpret_cast<WNDPROC>(previous);
}

static void InventoryRemoveTextCapture()
{
    AtomicExchange(&g_inventoryTextMode, INVENTORY_TEXT_NONE);
    if (g_gameWindow && g_inventoryOriginalGameWndProc)
    {
        SetWindowLongPtrW(g_gameWindow, -4 /* GWLP_WNDPROC */,
            reinterpret_cast<LONG_PTR>(g_inventoryOriginalGameWndProc));
        g_inventoryOriginalGameWndProc = nullptr;
    }
}

static void InventoryExtendedLoad()
{
    LoadInventoryStickerStore();
    InventoryInstallTextCapture();
}

static void ShutdownInventoryExtended()
{
    InventoryRemoveTextCapture();
    FlushInventoryStickerPersistenceIfNeeded();
    if (g_inventoryEconLastInventory)
    {
        for (int i = g_inventoryEconMirrorCount - 1; i >= 0; --i)
            InventoryEconRemoveMirrorAt(i, true);
    }
    g_inventoryEconMirrorCount = 0;
    g_inventoryEconLastInventory = nullptr;
    g_inventoryStickerModal = false;
}
