// cas+ local-only advanced inventory operations.
//
// These operations intentionally stay above the game adapter:
// - sandbox containers are VirtualInventoryItem records and opening one only
//   creates another local virtual item;
// - collection/storage grouping is a versioned sidecar keyed by virtual itemId;
// - no Game Coordinator messages, Steam inventory mutation or server ownership
//   claims are performed here.

constexpr int INVENTORY_LOCAL_GROUP_MAX = INVENTORY_MAX_ITEMS;
constexpr unsigned int INVENTORY_GROUP_PERSIST_MAGIC = 0x50524743u; // CGRP
constexpr unsigned short INVENTORY_GROUP_PERSIST_VERSION = 1;

struct InventoryGroupRecord {
    unsigned long long itemId;
    unsigned short collectionId;
    unsigned short storageId;
};

struct InventoryGroupPersistenceImage {
    unsigned int magic;
    unsigned short version;
    unsigned short count;
    InventoryGroupRecord records[INVENTORY_LOCAL_GROUP_MAX];
    unsigned int checksum;
};

static InventoryGroupRecord g_inventoryGroupRecords[INVENTORY_LOCAL_GROUP_MAX]{};
static int g_inventoryGroupRecordCount = 0;
static volatile LONG g_inventoryGroupLock = 0;
static volatile LONG g_inventoryGroupDirty = 0;
static bool g_inventoryAdvancedModal = false;
static unsigned int g_inventorySandboxRng = 0x6D2B79F5u;
static unsigned int g_inventorySandboxOpens = 0;

static bool InventoryGroupTryLock()
{
    return AtomicExchange(&g_inventoryGroupLock, 1) == 0;
}

static void InventoryGroupUnlock()
{
    AtomicExchange(&g_inventoryGroupLock, 0);
}

static InventoryGroupRecord* InventoryGroupFindLocked(
    unsigned long long itemId, bool create)
{
    if (!itemId)
        return nullptr;
    for (int i = 0; i < g_inventoryGroupRecordCount; ++i)
        if (g_inventoryGroupRecords[i].itemId == itemId)
            return &g_inventoryGroupRecords[i];
    if (!create || g_inventoryGroupRecordCount >= INVENTORY_LOCAL_GROUP_MAX)
        return nullptr;
    InventoryGroupRecord& record =
        g_inventoryGroupRecords[g_inventoryGroupRecordCount++];
    ZeroBytes(&record, sizeof(record));
    record.itemId = itemId;
    return &record;
}

static bool InventoryBuildGroupPersistencePath(wchar_t* path, DWORD capacity)
{
    if (!path || capacity < 64)
        return false;
    DWORD length = GetTempPathW(capacity, path);
    if (length == 0 || length >= capacity)
        return false;
    static const wchar_t suffix[] = L"cas_plus_inventory_groups_v1.bin";
    DWORD suffixLength = 0;
    while (suffix[suffixLength]) ++suffixLength;
    if (length + suffixLength + 1 >= capacity)
        return false;
    for (DWORD i = 0; i <= suffixLength; ++i)
        path[length + i] = suffix[i];
    return true;
}

static void MarkInventoryGroupDirty()
{
    AtomicExchange(&g_inventoryGroupDirty, 1);
}

static bool SaveInventoryGroupStoreLocked()
{
    wchar_t path[512];
    if (!InventoryBuildGroupPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return false;
    InventoryGroupPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    image.magic = INVENTORY_GROUP_PERSIST_MAGIC;
    image.version = INVENTORY_GROUP_PERSIST_VERSION;
    image.count = static_cast<unsigned short>(InventoryClampInt(
        g_inventoryGroupRecordCount, 0, INVENTORY_LOCAL_GROUP_MAX));
    for (int i = 0; i < image.count; ++i)
        image.records[i] = g_inventoryGroupRecords[i];
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

static void LoadInventoryGroupStore()
{
    wchar_t path[512];
    if (!InventoryBuildGroupPersistencePath(path,
        static_cast<DWORD>(sizeof(path) / sizeof(path[0]))))
        return;
    HANDLE file = CreateFileW(path, CAS_GENERIC_READ, CAS_FILE_SHARE_READ,
        nullptr, CAS_OPEN_EXISTING, CAS_FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == CAS_INVALID_HANDLE_VALUE)
        return;
    InventoryGroupPersistenceImage image;
    ZeroBytes(&image, sizeof(image));
    DWORD read = 0;
    const BOOL ok = ReadFile(file, &image, sizeof(image), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(image) ||
        image.magic != INVENTORY_GROUP_PERSIST_MAGIC ||
        image.version != INVENTORY_GROUP_PERSIST_VERSION ||
        image.count > INVENTORY_LOCAL_GROUP_MAX)
        return;
    const unsigned int expected = InventoryChecksum(
        reinterpret_cast<const BYTE*>(&image),
        static_cast<SIZE_T>(reinterpret_cast<BYTE*>(&image.checksum) -
            reinterpret_cast<BYTE*>(&image)));
    if (expected != image.checksum || !InventoryGroupTryLock())
        return;
    g_inventoryGroupRecordCount = image.count;
    for (int i = 0; i < g_inventoryGroupRecordCount; ++i)
        g_inventoryGroupRecords[i] = image.records[i];
    InventoryGroupUnlock();
}

static void FlushInventoryGroupPersistenceIfNeeded()
{
    if (AtomicExchange(&g_inventoryGroupDirty, 0) == 0)
        return;
    if (!InventoryGroupTryLock())
    {
        MarkInventoryGroupDirty();
        return;
    }
    const bool saved = SaveInventoryGroupStoreLocked();
    InventoryGroupUnlock();
    if (!saved)
        MarkInventoryGroupDirty();
}

static void InventoryAdvancedOnItemDeleted(unsigned long long itemId)
{
    if (!itemId || !InventoryGroupTryLock())
        return;
    for (int i = 0; i < g_inventoryGroupRecordCount; ++i)
    {
        if (g_inventoryGroupRecords[i].itemId != itemId)
            continue;
        for (int j = i + 1; j < g_inventoryGroupRecordCount; ++j)
            g_inventoryGroupRecords[j - 1] = g_inventoryGroupRecords[j];
        --g_inventoryGroupRecordCount;
        ZeroBytes(&g_inventoryGroupRecords[g_inventoryGroupRecordCount],
            sizeof(InventoryGroupRecord));
        MarkInventoryGroupDirty();
        break;
    }
    InventoryGroupUnlock();
}

static void InventoryAdvancedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId)
{
    if (!sourceId || !newId || !InventoryGroupTryLock())
        return;
    InventoryGroupRecord* source = InventoryGroupFindLocked(sourceId, false);
    if (source)
    {
        const unsigned short collection = source->collectionId;
        const unsigned short storage = source->storageId;
        InventoryGroupRecord* target = InventoryGroupFindLocked(newId, true);
        if (target)
        {
            target->collectionId = collection;
            target->storageId = storage;
            MarkInventoryGroupDirty();
        }
    }
    InventoryGroupUnlock();
}

static bool InventoryGroupSnapshot(unsigned long long itemId,
    unsigned short* collectionId, unsigned short* storageId)
{
    if (collectionId) *collectionId = 0;
    if (storageId) *storageId = 0;
    if (!itemId || !InventoryGroupTryLock())
        return false;
    InventoryGroupRecord* record = InventoryGroupFindLocked(itemId, false);
    if (record)
    {
        if (collectionId) *collectionId = record->collectionId;
        if (storageId) *storageId = record->storageId;
    }
    InventoryGroupUnlock();
    return record != nullptr;
}

static void InventoryUiAdjustGroup(int collectionDelta, int storageDelta,
    bool clearCollection, bool clearStorage)
{
    unsigned long long itemId = 0;
    if (!InventorySelectedItemId(&itemId) || !InventoryGroupTryLock())
        return;
    InventoryGroupRecord* record = InventoryGroupFindLocked(itemId, true);
    if (record)
    {
        int collection = clearCollection ? 0 :
            static_cast<int>(record->collectionId) + collectionDelta;
        int storage = clearStorage ? 0 :
            static_cast<int>(record->storageId) + storageDelta;
        record->collectionId = static_cast<unsigned short>(
            InventoryClampInt(collection, 0, 9999));
        record->storageId = static_cast<unsigned short>(
            InventoryClampInt(storage, 0, 9999));
        MarkInventoryGroupDirty();
    }
    InventoryGroupUnlock();
}

static void InventoryUiOpenAdvancedModal()
{
    g_inventoryAdvancedModal = true;
}

static void InventoryUiCloseAdvancedModal()
{
    g_inventoryAdvancedModal = false;
}

static unsigned int InventorySandboxNextRandom()
{
    // Small deterministic local PRNG; no security property is claimed or needed.
    g_inventorySandboxRng = g_inventorySandboxRng * 1664525u + 1013904223u;
    return g_inventorySandboxRng;
}

static bool InventoryCatalogRandomWeapon(unsigned int random,
    const InventoryWeaponCatalogEntry** output)
{
    if (!output)
        return false;
    *output = nullptr;
    const int count = InventoryCatalogWeaponCount();
    if (count <= 0)
        return false;
    int selectable = 0;
    for (int i = 0; i < count; ++i)
        if (InventoryCatalogIsSelectableWeapon(kInventoryWeapons[i]))
            ++selectable;
    if (selectable <= 0)
        return false;
    int wanted = static_cast<int>(random % static_cast<unsigned int>(selectable));
    for (int i = 0; i < count; ++i)
    {
        if (!InventoryCatalogIsSelectableWeapon(kInventoryWeapons[i]))
            continue;
        if (wanted-- == 0)
        {
            *output = &kInventoryWeapons[i];
            return true;
        }
    }
    return false;
}

static int InventoryRandomKnifePaint(unsigned int random)
{
    const int count = InventoryCatalogPaintCount();
    int valid = 0;
    for (int i = 0; i < count; ++i)
        if (kInventoryPaints[i].slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
            ++valid;
    if (valid <= 0)
        return 0;
    int wanted = static_cast<int>(random % static_cast<unsigned int>(valid));
    for (int i = 0; i < count; ++i)
    {
        if (kInventoryPaints[i].slotDefinitionIndex != INVENTORY_SLOT_KNIFE)
            continue;
        if (wanted-- == 0)
            return kInventoryPaints[i].paintKit;
    }
    return 0;
}

static void InventoryUiAddSandboxCase()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        ZeroBytes(&item, sizeof(item));
        item.itemId = g_inventoryStore.nextItemId++;
        item.slotDefinitionIndex = INVENTORY_SLOT_CONTAINER;
        item.overrideDefinitionIndex = 4001;
        item.statTrak = -1;
        item.wear = 0.0001f;
        item.quality = 1;
        item.equippedTeams = INVENTORY_TEAM_NONE;
        const char label[] = "Local Sandbox Case";
        int length = 0;
        while (label[length] && length + 1 < INVENTORY_CUSTOM_NAME_CAPACITY)
        {
            item.customName[length] = label[length];
            ++length;
        }
        item.customName[length] = 0;
        item.customNameLength = static_cast<BYTE>(length);
        const int index = g_inventoryStore.itemCount++;
        g_inventoryStore.items[index] = item;
        g_inventoryUiSelected = index;
        g_inventoryUiPage = index / 7;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static bool InventoryBuildSandboxReward(VirtualInventoryItem* reward)
{
    if (!reward)
        return false;
    ZeroBytes(reward, sizeof(*reward));
    const unsigned int rarityRoll = InventorySandboxNextRandom() % 10000u;
    // Local simulation buckets only. They intentionally do not claim Valve/GC
    // drop probabilities; the purpose is exercising inventory operations.
    const bool rareKnife = rarityRoll < 30u;
    if (rareKnife)
    {
        unsigned short definition = 500;
        const int steps = 1 + static_cast<int>(
            InventorySandboxNextRandom() % 20u);
        for (int i = 0; i < steps; ++i)
            definition = InventoryCatalogNextKnife(definition, 1);
        reward->slotDefinitionIndex = INVENTORY_SLOT_KNIFE;
        reward->overrideDefinitionIndex = definition;
        reward->paintKit = InventoryRandomKnifePaint(InventorySandboxNextRandom());
        reward->quality = 3;
    }
    else
    {
        const InventoryWeaponCatalogEntry* entry = nullptr;
        if (!InventoryCatalogRandomWeapon(InventorySandboxNextRandom(), &entry) || !entry)
            return false;
        reward->slotDefinitionIndex = entry->definitionIndex;
        reward->overrideDefinitionIndex = entry->definitionIndex;
        reward->paintKit = 0;
        reward->quality = rarityRoll < 350u ? 3 :
            rarityRoll < 1500u ? 4 : 0;
    }
    reward->seed = static_cast<int>(1u + InventorySandboxNextRandom() % 1000u);
    const unsigned int wearUnits = 1u + InventorySandboxNextRandom() % 999999u;
    reward->wear = static_cast<float>(wearUnits) / 1000000.0f;
    reward->statTrak = (InventorySandboxNextRandom() % 10u) == 0u ? 0 : -1;
    reward->equippedTeams = INVENTORY_TEAM_NONE;
    return true;
}

static void InventoryUiOpenSandboxCase()
{
    if (!InventoryTryLock())
        return;
    const int selectedIndex = g_inventoryUiSelected;
    if (selectedIndex < 0 || selectedIndex >= g_inventoryStore.itemCount ||
        g_inventoryStore.items[selectedIndex].slotDefinitionIndex !=
            INVENTORY_SLOT_CONTAINER)
    {
        InventoryUnlock();
        return;
    }

    VirtualInventoryItem reward;
    if (!InventoryBuildSandboxReward(&reward))
    {
        InventoryUnlock();
        return;
    }

    const unsigned long long consumedId =
        g_inventoryStore.items[selectedIndex].itemId;
    InventoryExtendedOnItemDeleted(consumedId);
    InventoryAdvancedOnItemDeleted(consumedId);
    for (int i = selectedIndex + 1; i < g_inventoryStore.itemCount; ++i)
        g_inventoryStore.items[i - 1] = g_inventoryStore.items[i];
    --g_inventoryStore.itemCount;
    ZeroBytes(&g_inventoryStore.items[g_inventoryStore.itemCount],
        sizeof(VirtualInventoryItem));

    reward.itemId = g_inventoryStore.nextItemId++;
    const int rewardIndex = g_inventoryStore.itemCount++;
    g_inventoryStore.items[rewardIndex] = reward;
    g_inventoryUiSelected = rewardIndex;
    g_inventoryUiPage = rewardIndex / 7;
    ++g_inventorySandboxOpens;
    MarkInventoryDirty();
    InventoryUnlock();
}

static const wchar_t* InventoryCatalogFinalDomainName(
    unsigned short slotDefinitionIndex, unsigned short definitionIndex)
{
    if (slotDefinitionIndex == INVENTORY_SLOT_CONTAINER)
        return L"Local Sandbox Case";
    return InventoryCatalogFullDomainName(slotDefinitionIndex, definitionIndex);
}

static void InventoryAdvancedLoad()
{
    LoadInventoryGroupStore();
}

static void ShutdownInventoryAdvanced()
{
    FlushInventoryGroupPersistenceIfNeeded();
    g_inventoryAdvancedModal = false;
}
