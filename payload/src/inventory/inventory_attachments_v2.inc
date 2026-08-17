// cas+ attachment projection V2.
//
// Adapts the normalized attachment model used by the open-source inventory
// simulator to the local-only cas+ inventory. The available IDs are never
// guessed: loader.exe writes cas_plus_attachment_catalog_v1.bin directly from
// the installed CS2 items_game/VPK before injection.

constexpr unsigned int INVENTORY_ATTACHMENT_CATALOG_MAGIC = 0x31415443u; // CTA1
constexpr unsigned short INVENTORY_ATTACHMENT_CATALOG_VERSION = 1;
constexpr unsigned int INVENTORY_ATTACHMENT_CATALOG_MAX_RECORDS = 8192u;
constexpr BYTE INVENTORY_ATTACHMENT_STICKER = 1;
constexpr BYTE INVENTORY_ATTACHMENT_PATCH = 2;
constexpr BYTE INVENTORY_ATTACHMENT_KEYCHAIN = 3;
constexpr unsigned int INVENTORY_KEYCHAIN_PERSIST_MAGIC = 0x4E48434Bu; // KCHN
constexpr unsigned short INVENTORY_KEYCHAIN_PERSIST_VERSION = 1;

#pragma pack(push, 1)
struct InventoryAttachmentCatalogRecord {
    int id;
    BYTE kind;
    BYTE reserved[3];
    wchar_t displayName[80];
    char internalName[96];
    char iconResource[160];
    char modelResource[160];
};

struct InventoryAttachmentCatalogHeader {
    unsigned int magic;
    unsigned short version;
    unsigned short recordSize;
    unsigned int count;
    unsigned int checksum;
    BYTE maxStickers;
    BYTE maxPatches;
    BYTE reserved[2];
};
#pragma pack(pop)

static InventoryAttachmentCatalogRecord* g_inventoryAttachmentCatalog = nullptr;
static unsigned int g_inventoryAttachmentCatalogCount = 0;
static BYTE g_inventoryAttachmentMaxStickers = 5;
static BYTE g_inventoryAttachmentMaxPatches = 3;
static bool g_inventoryAttachmentCatalogLoaded = false;
static bool g_inventoryAttachmentCatalogAttempted = false;

struct InventoryKeychainState {
    int id;
    int seed;
    float offsetX;
    float offsetY;
    float offsetZ;
};

struct InventoryKeychainRecord {
    unsigned long long itemId;
    InventoryKeychainState state;
};

struct InventoryKeychainPersistenceImage {
    unsigned int magic;
    unsigned short version;
    unsigned short count;
    InventoryKeychainRecord records[INVENTORY_EXT_MAX_RECORDS];
    unsigned int checksum;
};

static InventoryKeychainRecord g_inventoryKeychainRecords[INVENTORY_EXT_MAX_RECORDS]{};
static int g_inventoryKeychainRecordCount = 0;
static volatile LONG g_inventoryKeychainLock = 0;
static volatile LONG g_inventoryKeychainDirty = 0;
static bool g_inventoryKeychainModal = false;

// Declared by the view-fallback layer later in the generated translation unit.
static bool InventoryEconSetViewFloat(BYTE* view, const char* name, float value);

static unsigned int InventoryAttachmentChecksum(const BYTE* data, SIZE_T size)
{
    if (!data && size != 0)
        return 0;
    unsigned int hash = 2166136261u;
    for (SIZE_T i = 0; i < size; ++i)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static bool InventoryAttachmentBuildCatalogPath(wchar_t* path, DWORD capacity)
{
    if (!path || capacity < 64)
        return false;
    DWORD length = GetTempPathW(capacity, path);
    if (length == 0 || length >= capacity)
        return false;
    static const wchar_t suffix[] = L"cas_plus_attachment_catalog_v1.bin";
    DWORD suffixLength = 0;
    while (suffix[suffixLength]) ++suffixLength;
    if (length + suffixLength + 1 >= capacity)
        return false;
    for (DWORD i = 0; i <= suffixLength; ++i)
        path[length + i] = suffix[i];
    return true;
}

static void ShutdownInventoryAttachmentCatalog()
{
    if (g_inventoryAttachmentCatalog)
        VirtualFree(g_inventoryAttachmentCatalog, 0, 0x8000u /* MEM_RELEASE */);
    g_inventoryAttachmentCatalog = nullptr;
    g_inventoryAttachmentCatalogCount = 0;
    g_inventoryAttachmentCatalogLoaded = false;
    g_inventoryAttachmentCatalogAttempted = false;
    g_inventoryAttachmentMaxStickers = 5;
    g_inventoryAttachmentMaxPatches = 3;
}

static bool LoadInventoryAttachmentCatalog()
{
    if (g_inventoryAttachmentCatalogLoaded)
        return true;
    if (g_inventoryAttachmentCatalogAttempted)
        return false;
    g_inventoryAttachmentCatalogAttempted = true;

    wchar_t path[512]{};
    if (!InventoryAttachmentBuildCatalogPath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return false;
    HANDLE file = CreateFileW(path, CAS_GENERIC_READ, CAS_FILE_SHARE_READ,
        nullptr, CAS_OPEN_EXISTING, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return false;

    InventoryAttachmentCatalogHeader header{};
    DWORD read = 0;
    if (!ReadFile(file, &header, sizeof(header), &read, nullptr) ||
        read != sizeof(header) ||
        header.magic != INVENTORY_ATTACHMENT_CATALOG_MAGIC ||
        header.version != INVENTORY_ATTACHMENT_CATALOG_VERSION ||
        header.recordSize != sizeof(InventoryAttachmentCatalogRecord) ||
        header.count == 0 ||
        header.count > INVENTORY_ATTACHMENT_CATALOG_MAX_RECORDS)
    {
        CloseHandle(file);
        return false;
    }

    const SIZE_T bytes = static_cast<SIZE_T>(header.count) *
        sizeof(InventoryAttachmentCatalogRecord);
    auto* records = reinterpret_cast<InventoryAttachmentCatalogRecord*>(
        VirtualAlloc(nullptr, bytes, 0x3000u /* MEM_COMMIT|MEM_RESERVE */,
            0x04u /* PAGE_READWRITE */));
    if (!records)
    {
        CloseHandle(file);
        return false;
    }
    DWORD payloadRead = 0;
    const BOOL ok = ReadFile(file, records, static_cast<DWORD>(bytes),
        &payloadRead, nullptr);
    CloseHandle(file);
    if (!ok || payloadRead != bytes ||
        InventoryAttachmentChecksum(reinterpret_cast<const BYTE*>(records),
            bytes) != header.checksum)
    {
        VirtualFree(records, 0, 0x8000u);
        return false;
    }

    g_inventoryAttachmentCatalog = records;
    g_inventoryAttachmentCatalogCount = header.count;
    g_inventoryAttachmentMaxStickers = static_cast<BYTE>(InventoryClampInt(
        static_cast<int>(header.maxStickers), 1, INVENTORY_STICKER_SLOT_COUNT));
    g_inventoryAttachmentMaxPatches = static_cast<BYTE>(InventoryClampInt(
        static_cast<int>(header.maxPatches), 1, INVENTORY_STICKER_SLOT_COUNT));
    g_inventoryAttachmentCatalogLoaded = true;
    return true;
}

static const InventoryAttachmentCatalogRecord* InventoryAttachmentFind(
    BYTE kind, int id)
{
    if (id <= 0 || !LoadInventoryAttachmentCatalog())
        return nullptr;
    for (unsigned int i = 0; i < g_inventoryAttachmentCatalogCount; ++i)
    {
        const InventoryAttachmentCatalogRecord& record =
            g_inventoryAttachmentCatalog[i];
        if (record.kind == kind && record.id == id)
            return &record;
    }
    return nullptr;
}

static const InventoryAttachmentCatalogRecord* InventoryAttachmentCycle(
    BYTE kind, int currentId, int direction)
{
    if (!LoadInventoryAttachmentCatalog() || direction == 0)
        return InventoryAttachmentFind(kind, currentId);
    int first = -1;
    int last = -1;
    int current = -1;
    for (unsigned int i = 0; i < g_inventoryAttachmentCatalogCount; ++i)
    {
        if (g_inventoryAttachmentCatalog[i].kind != kind)
            continue;
        if (first < 0) first = static_cast<int>(i);
        last = static_cast<int>(i);
        if (g_inventoryAttachmentCatalog[i].id == currentId)
            current = static_cast<int>(i);
    }
    if (first < 0)
        return nullptr;
    if (current < 0)
        return direction > 0 ? &g_inventoryAttachmentCatalog[first] :
            &g_inventoryAttachmentCatalog[last];
    int index = current;
    for (unsigned int step = 0; step < g_inventoryAttachmentCatalogCount; ++step)
    {
        index += direction > 0 ? 1 : -1;
        if (index > last) index = first;
        if (index < first) index = last;
        if (g_inventoryAttachmentCatalog[index].kind == kind)
            return &g_inventoryAttachmentCatalog[index];
    }
    return &g_inventoryAttachmentCatalog[current];
}

static const wchar_t* InventoryAttachmentName(BYTE kind, int id)
{
    const InventoryAttachmentCatalogRecord* record =
        InventoryAttachmentFind(kind, id);
    return record && record->displayName[0] ? record->displayName : L"None";
}

static BYTE InventoryAttachmentKindForSlot(unsigned short slotDefinition)
{
    return slotDefinition == INVENTORY_SLOT_AGENT ?
        INVENTORY_ATTACHMENT_PATCH : INVENTORY_ATTACHMENT_STICKER;
}

static int InventoryAttachmentSlotLimit(unsigned short slotDefinition)
{
    if (!LoadInventoryAttachmentCatalog())
        return slotDefinition == INVENTORY_SLOT_AGENT ? 3 :
            INVENTORY_STICKER_SLOT_COUNT;
    return slotDefinition == INVENTORY_SLOT_AGENT ?
        static_cast<int>(g_inventoryAttachmentMaxPatches) :
        static_cast<int>(g_inventoryAttachmentMaxStickers);
}

static bool InventoryKeychainTryLock()
{
    return AtomicExchange(&g_inventoryKeychainLock, 1) == 0;
}

static void InventoryKeychainUnlock()
{
    AtomicExchange(&g_inventoryKeychainLock, 0);
}

static bool InventoryBuildKeychainPersistencePath(wchar_t* path,
    DWORD capacity)
{
    if (!path || capacity < 64)
        return false;
    DWORD length = GetTempPathW(capacity, path);
    if (length == 0 || length >= capacity)
        return false;
    static const wchar_t suffix[] = L"cas_plus_inventory_keychains_v1.bin";
    DWORD suffixLength = 0;
    while (suffix[suffixLength]) ++suffixLength;
    if (length + suffixLength + 1 >= capacity)
        return false;
    for (DWORD i = 0; i <= suffixLength; ++i)
        path[length + i] = suffix[i];
    return true;
}

static InventoryKeychainRecord* InventoryKeychainFindLocked(
    unsigned long long itemId, bool create)
{
    if (!itemId)
        return nullptr;
    for (int i = 0; i < g_inventoryKeychainRecordCount; ++i)
        if (g_inventoryKeychainRecords[i].itemId == itemId)
            return &g_inventoryKeychainRecords[i];
    if (!create || g_inventoryKeychainRecordCount >= INVENTORY_EXT_MAX_RECORDS)
        return nullptr;
    InventoryKeychainRecord& record =
        g_inventoryKeychainRecords[g_inventoryKeychainRecordCount++];
    ZeroBytes(&record, sizeof(record));
    record.itemId = itemId;
    return &record;
}

static void InventoryKeychainSanitize(InventoryKeychainState* state)
{
    if (!state)
        return;
    state->seed = InventoryClampInt(state->seed, 0, 0x7FFFFFFF);
    state->offsetX = InventoryStickerClamp(state->offsetX, -2.0f, 2.0f);
    state->offsetY = InventoryStickerClamp(state->offsetY, -2.0f, 2.0f);
    state->offsetZ = InventoryStickerClamp(state->offsetZ, -2.0f, 2.0f);
    if (state->id <= 0 ||
        !InventoryAttachmentFind(INVENTORY_ATTACHMENT_KEYCHAIN, state->id))
        ZeroBytes(state, sizeof(*state));
}

static bool InventoryKeychainCopy(unsigned long long itemId,
    InventoryKeychainState* output)
{
    if (!output || !InventoryKeychainTryLock())
        return false;
    InventoryKeychainRecord* record =
        InventoryKeychainFindLocked(itemId, false);
    if (record)
        *output = record->state;
    else
        ZeroBytes(output, sizeof(*output));
    InventoryKeychainUnlock();
    return record != nullptr;
}

static void MarkInventoryKeychainDirty()
{
    AtomicExchange(&g_inventoryKeychainDirty, 1);
}

static bool SaveInventoryKeychainStoreLocked()
{
    wchar_t path[512]{};
    if (!InventoryBuildKeychainPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return false;
    InventoryKeychainPersistenceImage image{};
    image.magic = INVENTORY_KEYCHAIN_PERSIST_MAGIC;
    image.version = INVENTORY_KEYCHAIN_PERSIST_VERSION;
    image.count = static_cast<unsigned short>(InventoryClampInt(
        g_inventoryKeychainRecordCount, 0, INVENTORY_EXT_MAX_RECORDS));
    for (int i = 0; i < image.count; ++i)
        image.records[i] = g_inventoryKeychainRecords[i];
    image.checksum = InventoryAttachmentChecksum(
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

static void LoadInventoryKeychainStore()
{
    if (!LoadInventoryAttachmentCatalog())
        return;
    wchar_t path[512]{};
    if (!InventoryBuildKeychainPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return;
    HANDLE file = CreateFileW(path, CAS_GENERIC_READ, CAS_FILE_SHARE_READ,
        nullptr, CAS_OPEN_EXISTING, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return;
    InventoryKeychainPersistenceImage image{};
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &image, sizeof(image), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(image) ||
        image.magic != INVENTORY_KEYCHAIN_PERSIST_MAGIC ||
        image.version != INVENTORY_KEYCHAIN_PERSIST_VERSION ||
        image.count > INVENTORY_EXT_MAX_RECORDS)
        return;
    const unsigned int expected = InventoryAttachmentChecksum(
        reinterpret_cast<const BYTE*>(&image),
        static_cast<SIZE_T>(reinterpret_cast<BYTE*>(&image.checksum) -
            reinterpret_cast<BYTE*>(&image)));
    if (expected != image.checksum || !InventoryKeychainTryLock())
        return;
    g_inventoryKeychainRecordCount = image.count;
    for (int i = 0; i < g_inventoryKeychainRecordCount; ++i)
    {
        g_inventoryKeychainRecords[i] = image.records[i];
        InventoryKeychainSanitize(&g_inventoryKeychainRecords[i].state);
    }
    InventoryKeychainUnlock();
}

static void FlushInventoryKeychainPersistenceIfNeeded()
{
    if (AtomicExchange(&g_inventoryKeychainDirty, 0) == 0)
        return;
    if (!InventoryKeychainTryLock())
    {
        MarkInventoryKeychainDirty();
        return;
    }
    const bool saved = SaveInventoryKeychainStoreLocked();
    InventoryKeychainUnlock();
    if (!saved)
        MarkInventoryKeychainDirty();
}

static void InventoryKeychainOnItemDeleted(unsigned long long itemId)
{
    if (!itemId || !InventoryKeychainTryLock())
        return;
    for (int i = 0; i < g_inventoryKeychainRecordCount; ++i)
    {
        if (g_inventoryKeychainRecords[i].itemId != itemId)
            continue;
        for (int j = i + 1; j < g_inventoryKeychainRecordCount; ++j)
            g_inventoryKeychainRecords[j - 1] = g_inventoryKeychainRecords[j];
        --g_inventoryKeychainRecordCount;
        ZeroBytes(&g_inventoryKeychainRecords[g_inventoryKeychainRecordCount],
            sizeof(InventoryKeychainRecord));
        MarkInventoryKeychainDirty();
        break;
    }
    InventoryKeychainUnlock();
}

static void InventoryKeychainOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId)
{
    if (!sourceId || !newId || !InventoryKeychainTryLock())
        return;
    InventoryKeychainRecord* source =
        InventoryKeychainFindLocked(sourceId, false);
    if (source)
    {
        InventoryKeychainRecord copy = *source;
        InventoryKeychainRecord* destination =
            InventoryKeychainFindLocked(newId, true);
        if (destination)
        {
            *destination = copy;
            destination->itemId = newId;
            MarkInventoryKeychainDirty();
        }
    }
    InventoryKeychainUnlock();
}

static bool InventoryKeychainSupportedSlot(unsigned short slotDefinition)
{
    return slotDefinition != INVENTORY_SLOT_AGENT &&
        slotDefinition != INVENTORY_SLOT_GLOVE &&
        slotDefinition != INVENTORY_SLOT_MUSIC &&
        slotDefinition != INVENTORY_SLOT_CONTAINER;
}

static bool InventoryKeychainMutateSelected(int operation, float delta)
{
    if (!LoadInventoryAttachmentCatalog() || !InventoryTryLock())
        return false;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (!item || !InventoryKeychainSupportedSlot(item->slotDefinitionIndex))
    {
        InventoryUnlock();
        return false;
    }
    const unsigned long long itemId = item->itemId;
    InventoryUnlock();
    if (!InventoryKeychainTryLock())
        return false;
    InventoryKeychainRecord* record =
        InventoryKeychainFindLocked(itemId, true);
    if (!record)
    {
        InventoryKeychainUnlock();
        return false;
    }
    InventoryKeychainState& state = record->state;
    if (operation == 1)
    {
        const InventoryAttachmentCatalogRecord* next = InventoryAttachmentCycle(
            INVENTORY_ATTACHMENT_KEYCHAIN, state.id, delta >= 0.0f ? 1 : -1);
        if (next)
            state.id = next->id;
    }
    else if (operation == 2)
        state.seed = InventoryClampInt(state.seed + static_cast<int>(delta),
            0, 0x7FFFFFFF);
    else if (operation == 3)
        state.offsetX = InventoryStickerClamp(state.offsetX + delta, -2.0f, 2.0f);
    else if (operation == 4)
        state.offsetY = InventoryStickerClamp(state.offsetY + delta, -2.0f, 2.0f);
    else if (operation == 5)
        state.offsetZ = InventoryStickerClamp(state.offsetZ + delta, -2.0f, 2.0f);
    else if (operation == 6)
        ZeroBytes(&state, sizeof(state));
    InventoryKeychainSanitize(&state);
    MarkInventoryKeychainDirty();
    InventoryKeychainUnlock();
    MarkInventoryDirty();
    return true;
}

static void InventoryUiOpenKeychainModal()
{
    if (!LoadInventoryAttachmentCatalog() || !InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    const bool supported = item &&
        InventoryKeychainSupportedSlot(item->slotDefinitionIndex);
    InventoryUnlock();
    if (supported)
        g_inventoryKeychainModal = true;
}

static void InventoryUiCloseKeychainModal()
{
    g_inventoryKeychainModal = false;
}

static void InventoryUiCycleKeychain(int direction)
{
    InventoryKeychainMutateSelected(1, direction >= 0 ? 1.0f : -1.0f);
}

static void InventoryUiAdjustKeychainSeed(int delta)
{
    InventoryKeychainMutateSelected(2, static_cast<float>(delta));
}

static void InventoryUiAdjustKeychainOffsetX(float delta)
{
    InventoryKeychainMutateSelected(3, delta);
}

static void InventoryUiAdjustKeychainOffsetY(float delta)
{
    InventoryKeychainMutateSelected(4, delta);
}

static void InventoryUiAdjustKeychainOffsetZ(float delta)
{
    InventoryKeychainMutateSelected(5, delta);
}

static void InventoryUiRemoveKeychain()
{
    InventoryKeychainMutateSelected(6, 0.0f);
}

static float InventoryAttachmentUIntBits(unsigned int value)
{
    union Bits { unsigned int integer; float floating; } bits{};
    bits.integer = value;
    return bits.floating;
}

static float InventoryAttachmentIntBits(int value)
{
    union Bits { int integer; float floating; } bits{};
    bits.integer = value;
    return bits.floating;
}

static bool InventoryAttachmentApplyKeychainView(BYTE* view,
    const VirtualInventoryItem& item)
{
    if (!view || !InventoryKeychainSupportedSlot(item.slotDefinitionIndex))
        return view != nullptr;
    InventoryKeychainState state{};
    InventoryKeychainCopy(item.itemId, &state);
    InventoryKeychainSanitize(&state);
    bool ok = true;
    // Integer econ attributes passed through SetAttributeValueByName use their
    // exact 32-bit representation, matching the current CounterStrikeSharp
    // inventory simulator rather than converting the ID to a numeric float.
    ok = InventoryEconSetViewFloat(view, "keychain slot 0 id",
        InventoryAttachmentUIntBits(static_cast<unsigned int>(state.id))) && ok;
    ok = InventoryEconSetViewFloat(view, "keychain slot 0 seed",
        InventoryAttachmentIntBits(state.seed)) && ok;
    ok = InventoryEconSetViewFloat(view, "keychain slot 0 offset x",
        state.offsetX) && ok;
    ok = InventoryEconSetViewFloat(view, "keychain slot 0 offset y",
        state.offsetY) && ok;
    ok = InventoryEconSetViewFloat(view, "keychain slot 0 offset z",
        state.offsetZ) && ok;
    return ok;
}

static void InventoryAttachmentHashKeychain(unsigned long long itemId,
    unsigned int* hash)
{
    if (!hash)
        return;
    InventoryKeychainState state{};
    InventoryKeychainCopy(itemId, &state);
    const BYTE* bytes = reinterpret_cast<const BYTE*>(&state);
    for (SIZE_T i = 0; i < sizeof(state); ++i)
    {
        *hash ^= bytes[i];
        *hash *= 16777619u;
    }
}
