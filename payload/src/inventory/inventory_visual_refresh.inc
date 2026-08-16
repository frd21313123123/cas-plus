// cas+ guarded visual refresh backend for projected virtual inventory items.
//
// Fallback econ fields can be written safely through schema-resolved offsets,
// but CS2's composite material and weapon subclass caches are separate. This
// backend resolves two current client functions by unique executable patterns:
//   C_CSWeaponBase::UpdateSubclass(this)
//   C_CSWeaponBase::UpdateSkin(this, bool)
// If either pattern is missing/ambiguous, that part of the backend stays off.
// Calls are restricted to locally-owned entities already captured by the
// inventory adapter and to definition indexes present in the weapon catalog.
//
// Refreshes are budgeted. UpdateSkin restarts composite-material work, so
// calling it every FrameStageNotify would continuously restart the build.

using InventoryUpdateSubclassFn = void (*)(void*);
using InventoryUpdateSkinFn = void (*)(void*, bool);

struct InventoryVisualRefreshRuntime {
    bool resolved;
    bool subclassReady;
    bool skinReady;
    InventoryUpdateSubclassFn updateSubclass;
    InventoryUpdateSkinFn updateSkin;
};

static InventoryVisualRefreshRuntime g_inventoryVisualRefreshRuntime{};

struct InventoryVisualRefreshRecord {
    unsigned int entityHandle;
    void* entityAddress;
    void* identityAddress;
    unsigned short definitionIndex;
    int paintKit;
    int seed;
    int statTrak;
    unsigned int wearBits;
    int refreshBudget;
    int refreshCooldown;
    bool seen;
};

static InventoryVisualRefreshRecord g_inventoryVisualRefreshRecords[INVENTORY_MAX_ORIGINALS]{};
static int g_inventoryVisualRefreshRecordCount = 0;
static unsigned int g_inventoryVisualRefreshCalls = 0;
static unsigned int g_inventorySubclassRefreshCalls = 0;

static unsigned int InventoryFloatBits(float value)
{
    unsigned int bits = 0;
    CopyFourBytes(reinterpret_cast<BYTE*>(&bits),
        reinterpret_cast<const BYTE*>(&value));
    return bits;
}

static bool ResolveInventoryVisualRefreshRuntime(HMODULE clientModule)
{
    g_inventoryVisualRefreshRuntime = {};
    g_inventoryVisualRefreshRecordCount = 0;
    if (!clientModule)
        return false;

    // C_CSWeaponBase::UpdateSubclass. Keep the pattern deliberately specific;
    // FindUniquePattern returns null if more than one candidate exists.
    static const int kUpdateSubclassPattern[] = {
        0x4C, 0x8B, 0xDC, 0x53, 0x48, 0x81, 0xEC,
        -1, -1, -1, -1, 0x48, 0x8B, 0x41
    };

    // C_CSWeaponBase::UpdateSkin(this, bool). This longer prologue includes
    // both internal calls and the item-view address calculation, reducing the
    // chance of accepting an unrelated short prologue after a game update.
    static const int kUpdateSkinPattern[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20,
        0x8B, 0xDA, 0x48, 0x8B, 0xF9, 0xE8, -1, -1, -1, -1,
        0xF6, 0xC3, 0x01, 0x74, 0x0A, 0x33, 0xD2, 0x48, 0x8B, 0xCF,
        0xE8, -1, -1, -1, -1, 0x48, 0x8D, 0x8F,
        0x90, 0x19, 0x00, 0x00
    };

    BYTE* subclass = FindUniquePattern(clientModule,
        kUpdateSubclassPattern,
        sizeof(kUpdateSubclassPattern) / sizeof(kUpdateSubclassPattern[0]));
    BYTE* skin = FindUniquePattern(clientModule,
        kUpdateSkinPattern,
        sizeof(kUpdateSkinPattern) / sizeof(kUpdateSkinPattern[0]));

    if (subclass && IsExecutable(subclass))
    {
        g_inventoryVisualRefreshRuntime.updateSubclass =
            reinterpret_cast<InventoryUpdateSubclassFn>(subclass);
        g_inventoryVisualRefreshRuntime.subclassReady = true;
    }
    if (skin && IsExecutable(skin))
    {
        g_inventoryVisualRefreshRuntime.updateSkin =
            reinterpret_cast<InventoryUpdateSkinFn>(skin);
        g_inventoryVisualRefreshRuntime.skinReady = true;
    }

    g_inventoryVisualRefreshRuntime.resolved = true;
    return g_inventoryVisualRefreshRuntime.subclassReady ||
        g_inventoryVisualRefreshRuntime.skinReady;
}

static InventoryVisualRefreshRecord* FindInventoryVisualRefreshRecord(
    unsigned int entityHandle)
{
    for (int i = 0; i < g_inventoryVisualRefreshRecordCount; ++i)
        if (g_inventoryVisualRefreshRecords[i].entityHandle == entityHandle)
            return &g_inventoryVisualRefreshRecords[i];
    return nullptr;
}

static bool ReadInventoryProjectedState(void* entity,
    unsigned short* definitionIndex, int* paintKit, int* seed,
    int* statTrak, unsigned int* wearBits)
{
    if (!entity || !definitionIndex || !paintKit || !seed ||
        !statTrak || !wearBits || !g_inventoryRuntime.ready)
        return false;

    if (!InventoryReadDefinition(entity, definitionIndex))
        return false;
    if (!InventoryCatalogFindWeapon(*definitionIndex))
        return false;

    BYTE* base = reinterpret_cast<BYTE*>(entity);
    int* paint = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackPaintKitOffset);
    int* seedField = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackSeedOffset);
    float* wear = reinterpret_cast<float*>(
        base + g_inventoryRuntime.fallbackWearOffset);
    int* stat = reinterpret_cast<int*>(
        base + g_inventoryRuntime.fallbackStatTrakOffset);
    if (!IsAccessible(paint, sizeof(int), false) ||
        !IsAccessible(seedField, sizeof(int), false) ||
        !IsAccessible(wear, sizeof(float), false) ||
        !IsAccessible(stat, sizeof(int), false))
        return false;

    *paintKit = *paint;
    *seed = *seedField;
    *statTrak = *stat;
    *wearBits = InventoryFloatBits(*wear);
    return true;
}

static bool InventoryRefreshRecordMatchesEntity(
    const InventoryVisualRefreshRecord& record, void* entity)
{
    if (!entity || entity != record.entityAddress ||
        !IsAccessible(entity, 0x18, false))
        return false;
    return *reinterpret_cast<void**>(reinterpret_cast<BYTE*>(entity) + 0x10) ==
        record.identityAddress;
}

static InventoryVisualRefreshRecord* CaptureInventoryVisualRefreshRecord(
    void* entity, unsigned int handle, unsigned short definitionIndex,
    int paintKit, int seed, int statTrak, unsigned int wearBits)
{
    InventoryVisualRefreshRecord* record =
        FindInventoryVisualRefreshRecord(handle);
    if (record && !InventoryRefreshRecordMatchesEntity(*record, entity))
    {
        const int stale = static_cast<int>(record - g_inventoryVisualRefreshRecords);
        for (int i = stale + 1; i < g_inventoryVisualRefreshRecordCount; ++i)
            g_inventoryVisualRefreshRecords[i - 1] =
                g_inventoryVisualRefreshRecords[i];
        --g_inventoryVisualRefreshRecordCount;
        record = nullptr;
    }

    if (!record)
    {
        if (g_inventoryVisualRefreshRecordCount >= INVENTORY_MAX_ORIGINALS)
            return nullptr;
        record = &g_inventoryVisualRefreshRecords[
            g_inventoryVisualRefreshRecordCount++];
        ZeroBytes(record, sizeof(*record));
        record->entityHandle = handle;
        record->entityAddress = entity;
        record->identityAddress = *reinterpret_cast<void**>(
            reinterpret_cast<BYTE*>(entity) + 0x10);
        record->definitionIndex = definitionIndex;
        record->paintKit = paintKit;
        record->seed = seed;
        record->statTrak = statTrak;
        record->wearBits = wearBits;
        record->refreshBudget = 3;
        record->refreshCooldown = 0;
        record->seen = true;
        return record;
    }

    const bool definitionChanged =
        record->definitionIndex != definitionIndex;
    const bool cosmeticsChanged = definitionChanged ||
        record->paintKit != paintKit || record->seed != seed ||
        record->statTrak != statTrak || record->wearBits != wearBits;

    record->entityAddress = entity;
    record->identityAddress = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(entity) + 0x10);
    record->seen = true;

    if (cosmeticsChanged)
    {
        if (definitionChanged &&
            g_inventoryVisualRefreshRuntime.subclassReady)
        {
            g_inventoryVisualRefreshRuntime.updateSubclass(entity);
            ++g_inventorySubclassRefreshCalls;
        }
        record->definitionIndex = definitionIndex;
        record->paintKit = paintKit;
        record->seed = seed;
        record->statTrak = statTrak;
        record->wearBits = wearBits;
        record->refreshBudget = 3;
        record->refreshCooldown = 0;
    }
    return record;
}

static void UpdateInventoryVisualRefresh()
{
    if (!g_inventoryVisualRefreshRuntime.resolved ||
        !g_inventoryVisualRefreshRuntime.skinReady ||
        !g_inventoryRuntime.ready || !g_preResolvedEntityRuntimeReady ||
        g_originalWeaponCosmeticsCount <= 0)
        return;

    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        g_inventoryVisualRefreshRecordCount = 0;
        return;
    }

    for (int i = 0; i < g_inventoryVisualRefreshRecordCount; ++i)
        g_inventoryVisualRefreshRecords[i].seen = false;

    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
    {
        const OriginalWeaponCosmetics& original =
            g_originalWeaponCosmetics[i];
        void* entity = EntityFromHandle(g_preResolvedEntityRuntime,
            entitySystem, original.entityHandle);
        if (!entity || !InventoryOriginalMatchesEntity(original, entity))
            continue;

        unsigned short definitionIndex = 0;
        int paintKit = 0;
        int seed = 0;
        int statTrak = -1;
        unsigned int wearBits = 0;
        if (!ReadInventoryProjectedState(entity, &definitionIndex,
            &paintKit, &seed, &statTrak, &wearBits))
            continue;

        InventoryVisualRefreshRecord* record =
            CaptureInventoryVisualRefreshRecord(entity,
                original.entityHandle, definitionIndex, paintKit,
                seed, statTrak, wearBits);
        if (!record)
            continue;

        if (record->refreshCooldown > 0)
            --record->refreshCooldown;
        if (record->refreshBudget > 0 && record->refreshCooldown == 0)
        {
            g_inventoryVisualRefreshRuntime.updateSkin(entity, true);
            ++g_inventoryVisualRefreshCalls;
            --record->refreshBudget;
            // ~40 frame-stage passes between material re-kicks. This keeps the
            // composite builder progressing instead of restarting each frame.
            record->refreshCooldown = 40;
        }
    }

    int destination = 0;
    for (int i = 0; i < g_inventoryVisualRefreshRecordCount; ++i)
    {
        if (!g_inventoryVisualRefreshRecords[i].seen)
            continue;
        g_inventoryVisualRefreshRecords[destination++] =
            g_inventoryVisualRefreshRecords[i];
    }
    g_inventoryVisualRefreshRecordCount = destination;
}

static void ResetInventoryVisualRefresh()
{
    g_inventoryVisualRefreshRecordCount = 0;
}
