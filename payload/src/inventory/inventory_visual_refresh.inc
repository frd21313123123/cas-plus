// cas+ guarded live weapon material refresh backend.
//
// Projecting fallback econ fields is not sufficient on current CS2 builds:
// weapon skins are backed by a C_EconItemView attribute set and a separate
// composite-material cache.  This backend therefore mirrors paint attributes
// onto the live weapon item view and rebuilds the material cache with the
// current client functions.  Every pointer/function/embedded-owner offset is
// validated at runtime and all calls remain restricted to locally-owned weapon
// entities that were already captured by the inventory adapter.

using InventoryUpdateSubclassFn = void (*)(void*);
using InventoryUpdateSkinFn = void (*)(void*, bool);
using InventoryUpdateCompositeMaterialFn = void (*)(void*, char);
using InventoryUpdateCompositeMaterialSetFn = void (*)(void*, char);
using InventorySetViewAttributeByNameFn = void (*)(void*, const char*, float);

struct InventoryVisualRefreshRuntime {
    bool resolved;
    bool subclassReady;
    bool skinReady;
    bool compositeReady;
    bool compositeSetReady;
    bool viewAttributeReady;
    bool compositeOwnerOffsetReady;
    bool hasRestoreMaterial;
    bool hasDisallowSoc;
    unsigned int compositeOwnerOffset;
    unsigned int restoreMaterialOffset;
    unsigned int disallowSocOffset;
    InventoryUpdateSubclassFn updateSubclass;
    InventoryUpdateSkinFn updateSkin;
    InventoryUpdateCompositeMaterialFn updateCompositeMaterial;
    InventoryUpdateCompositeMaterialSetFn updateCompositeMaterialSet;
    InventorySetViewAttributeByNameFn setViewAttributeByName;
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
static unsigned int g_inventoryCompositeRefreshCalls = 0;
static unsigned int g_inventoryCompositeSetRefreshCalls = 0;
static unsigned int g_inventoryViewAttributeRefreshCalls = 0;

static unsigned int InventoryFloatBits(float value)
{
    unsigned int bits = 0;
    CopyFourBytes(reinterpret_cast<BYTE*>(&bits),
        reinterpret_cast<const BYTE*>(&value));
    return bits;
}

static BYTE* InventoryVisualResolveRelative(BYTE* instruction,
    int displacementOffset, int instructionSize)
{
    if (!instruction || displacementOffset < 0 || instructionSize <= 0 ||
        !IsAccessible(instruction + displacementOffset, sizeof(LONG), false))
        return nullptr;
    const LONG displacement = *reinterpret_cast<LONG*>(
        instruction + displacementOffset);
    return instruction + instructionSize + displacement;
}

static bool ResolveInventoryVisualItemViewOffsets(
    InventoryVisualRefreshRuntime* runtime)
{
    if (!runtime)
        return false;
    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;
    SchemaClassInfoView* itemView = FindSchemaClass(scope, "C_EconItemView");
    if (!itemView || itemView->size <= 0)
        return false;

    unsigned int restore = 0;
    if (FindSchemaField(itemView, "m_bRestoreCustomMaterialAfterPrecache", &restore) &&
        restore + sizeof(BYTE) <= static_cast<unsigned int>(itemView->size))
    {
        runtime->restoreMaterialOffset = restore;
        runtime->hasRestoreMaterial = true;
    }

    unsigned int disallowSoc = 0;
    if (FindSchemaField(itemView, "m_bDisallowSOC", &disallowSoc) &&
        disallowSoc + sizeof(BYTE) <= static_cast<unsigned int>(itemView->size))
    {
        runtime->disallowSocOffset = disallowSoc;
        runtime->hasDisallowSoc = true;
    }
    return runtime->hasRestoreMaterial || runtime->hasDisallowSoc;
}

static bool ResolveInventoryVisualRefreshRuntime(HMODULE clientModule)
{
    g_inventoryVisualRefreshRuntime = {};
    g_inventoryVisualRefreshRecordCount = 0;
    if (!clientModule)
        return false;

    // Optional legacy subclass rebuild. Definition/model changes still benefit
    // from it when this older entry point remains unique on the running build.
    static const int kUpdateSubclassPattern[] = {
        0x4C, 0x8B, 0xDC, 0x53, 0x48, 0x81, 0xEC,
        -1, -1, -1, -1, 0x48, 0x8B, 0x41
    };

    // Current C_CSWeaponBase::UpdateCompositeMaterial implementation.
    static const int kUpdateCompositeMaterialPattern[] = {
        0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,
        0x48,0x89,0x74,0x24,0x20,0x57,0x41,0x56,0x41,0x57,
        0x48,0x83,0xEC,0x20,0x44,0x0F,0xB6,0xF2
    };
    // Alternate current call-site signature; target is the rel32 call.
    static const int kUpdateCompositeMaterialCallPattern[] = {
        0xE8,-1,-1,-1,-1,0x48,0x8D,0x8B,-1,-1,-1,-1,
        0x48,0x89,0xBC,0x24
    };
    static const int kUpdateCompositeMaterialSetPattern[] = {
        0x40,0x55,0x53,0x41,0x57,0x48,0x8D,0xAC,0x24,
        0x00,0xFE,-1,-1,0x48,0x81,0xEC
    };

    // The embedded CCompositeMaterialOwner is reached by an add rcx, imm32 in
    // the current material-update path. Read the immediate rather than baking
    // the observed 0x608 offset into the payload.
    static const int kCompositeOwnerOffsetPattern[] = {
        0x48,0x81,0xC1,-1,-1,-1,-1,0xB2,-1,0xE8,-1,-1,-1,-1,
        0x48,0x8D,0x8B
    };

    // Existing independently-validated C_EconItemView attribute setter call.
    static const int kViewAttributeCallPattern[] = {
        0xE8,-1,-1,-1,-1,0x66,0x41,0x0F,0x6E,0xD4
    };

    // Keep the previous UpdateSkin entry point as an optional compatibility
    // kick. It is not required for the new composite-material path.
    static const int kUpdateSkinPattern[] = {
        0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,
        0x8B,0xDA,0x48,0x8B,0xF9,0xE8,-1,-1,-1,-1,
        0xF6,0xC3,0x01,0x74,0x0A,0x33,0xD2,0x48,0x8B,0xCF,
        0xE8,-1,-1,-1,-1,0x48,0x8D,0x8F,
        0x90,0x19,0x00,0x00
    };

    BYTE* subclass = FindUniquePattern(clientModule,
        kUpdateSubclassPattern,
        sizeof(kUpdateSubclassPattern) / sizeof(kUpdateSubclassPattern[0]));
    BYTE* composite = FindUniquePattern(clientModule,
        kUpdateCompositeMaterialPattern,
        sizeof(kUpdateCompositeMaterialPattern) /
            sizeof(kUpdateCompositeMaterialPattern[0]));
    if (!composite)
    {
        BYTE* compositeCall = FindUniquePattern(clientModule,
            kUpdateCompositeMaterialCallPattern,
            sizeof(kUpdateCompositeMaterialCallPattern) /
                sizeof(kUpdateCompositeMaterialCallPattern[0]));
        if (compositeCall)
            composite = InventoryVisualResolveRelative(compositeCall, 1, 5);
    }
    BYTE* compositeSet = FindUniquePattern(clientModule,
        kUpdateCompositeMaterialSetPattern,
        sizeof(kUpdateCompositeMaterialSetPattern) /
            sizeof(kUpdateCompositeMaterialSetPattern[0]));
    BYTE* ownerOffset = FindUniquePattern(clientModule,
        kCompositeOwnerOffsetPattern,
        sizeof(kCompositeOwnerOffsetPattern) /
            sizeof(kCompositeOwnerOffsetPattern[0]));
    BYTE* viewAttributeCall = FindUniquePattern(clientModule,
        kViewAttributeCallPattern,
        sizeof(kViewAttributeCallPattern) / sizeof(kViewAttributeCallPattern[0]));
    BYTE* skin = FindUniquePattern(clientModule,
        kUpdateSkinPattern,
        sizeof(kUpdateSkinPattern) / sizeof(kUpdateSkinPattern[0]));

    if (subclass && IsExecutable(subclass))
    {
        g_inventoryVisualRefreshRuntime.updateSubclass =
            reinterpret_cast<InventoryUpdateSubclassFn>(subclass);
        g_inventoryVisualRefreshRuntime.subclassReady = true;
    }
    if (composite && IsExecutable(composite))
    {
        g_inventoryVisualRefreshRuntime.updateCompositeMaterial =
            reinterpret_cast<InventoryUpdateCompositeMaterialFn>(composite);
        g_inventoryVisualRefreshRuntime.compositeReady = true;
    }
    if (compositeSet && IsExecutable(compositeSet))
    {
        g_inventoryVisualRefreshRuntime.updateCompositeMaterialSet =
            reinterpret_cast<InventoryUpdateCompositeMaterialSetFn>(compositeSet);
        g_inventoryVisualRefreshRuntime.compositeSetReady = true;
    }
    if (ownerOffset && IsAccessible(ownerOffset + 3, sizeof(unsigned int), false))
    {
        const unsigned int offset = *reinterpret_cast<unsigned int*>(ownerOffset + 3);
        if (offset >= 0x100 && offset <= 0x2000)
        {
            g_inventoryVisualRefreshRuntime.compositeOwnerOffset = offset;
            g_inventoryVisualRefreshRuntime.compositeOwnerOffsetReady = true;
        }
    }
    if (viewAttributeCall)
    {
        BYTE* attributeSetter = InventoryVisualResolveRelative(
            viewAttributeCall, 1, 5);
        if (attributeSetter && IsExecutable(attributeSetter))
        {
            g_inventoryVisualRefreshRuntime.setViewAttributeByName =
                reinterpret_cast<InventorySetViewAttributeByNameFn>(attributeSetter);
            g_inventoryVisualRefreshRuntime.viewAttributeReady = true;
        }
    }
    if (skin && IsExecutable(skin))
    {
        g_inventoryVisualRefreshRuntime.updateSkin =
            reinterpret_cast<InventoryUpdateSkinFn>(skin);
        g_inventoryVisualRefreshRuntime.skinReady = true;
    }

    ResolveInventoryVisualItemViewOffsets(&g_inventoryVisualRefreshRuntime);
    g_inventoryVisualRefreshRuntime.resolved = true;
    return (g_inventoryVisualRefreshRuntime.compositeReady &&
            g_inventoryVisualRefreshRuntime.compositeOwnerOffsetReady) ||
        g_inventoryVisualRefreshRuntime.compositeSetReady ||
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
    int* statTrak, unsigned int* wearBits, float* wearValue)
{
    if (!entity || !definitionIndex || !paintKit || !seed ||
        !statTrak || !wearBits || !wearValue || !g_inventoryRuntime.ready)
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
    *wearValue = InventoryClampWear(*wear);
    *wearBits = InventoryFloatBits(*wearValue);
    return true;
}

static bool InventoryVisualApplyItemViewState(void* entity,
    int paintKit, int seed, float wear)
{
    if (!entity || !g_inventoryRuntime.ready)
        return false;
    BYTE* itemView = InventoryItemView(entity);
    if (!itemView)
        return false;

    bool changed = false;
    if (g_inventoryVisualRefreshRuntime.viewAttributeReady &&
        g_inventoryVisualRefreshRuntime.setViewAttributeByName)
    {
        g_inventoryVisualRefreshRuntime.setViewAttributeByName(
            itemView, "set item texture prefab", static_cast<float>(paintKit));
        g_inventoryVisualRefreshRuntime.setViewAttributeByName(
            itemView, "set item texture seed",
            static_cast<float>(InventoryClampInt(seed, 0, 1000)));
        g_inventoryVisualRefreshRuntime.setViewAttributeByName(
            itemView, "set item texture wear", InventoryClampWear(wear));
        ++g_inventoryViewAttributeRefreshCalls;
        changed = true;
    }

    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = itemView + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    if (g_inventoryVisualRefreshRuntime.hasRestoreMaterial)
    {
        BYTE* restore = itemView +
            g_inventoryVisualRefreshRuntime.restoreMaterialOffset;
        if (IsAccessible(restore, sizeof(BYTE), true))
            *restore = 1;
    }
    if (g_inventoryVisualRefreshRuntime.hasDisallowSoc)
    {
        BYTE* disallowSoc = itemView +
            g_inventoryVisualRefreshRuntime.disallowSocOffset;
        if (IsAccessible(disallowSoc, sizeof(BYTE), true))
            *disallowSoc = 0;
    }
    return changed;
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
        record->refreshBudget = 4;
        record->refreshCooldown = 0;
        record->seen = true;
        if (g_inventoryVisualRefreshRuntime.subclassReady)
        {
            g_inventoryVisualRefreshRuntime.updateSubclass(entity);
            ++g_inventorySubclassRefreshCalls;
        }
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
        if (definitionChanged && g_inventoryVisualRefreshRuntime.subclassReady)
        {
            g_inventoryVisualRefreshRuntime.updateSubclass(entity);
            ++g_inventorySubclassRefreshCalls;
        }
        record->definitionIndex = definitionIndex;
        record->paintKit = paintKit;
        record->seed = seed;
        record->statTrak = statTrak;
        record->wearBits = wearBits;
        record->refreshBudget = 4;
        record->refreshCooldown = 0;
    }
    return record;
}

static void UpdateInventoryVisualRefresh()
{
    if (!g_inventoryVisualRefreshRuntime.resolved ||
        (!g_inventoryVisualRefreshRuntime.compositeReady &&
         !g_inventoryVisualRefreshRuntime.compositeSetReady &&
         !g_inventoryVisualRefreshRuntime.skinReady) ||
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
        float wear = 0.0001f;
        if (!ReadInventoryProjectedState(entity, &definitionIndex,
            &paintKit, &seed, &statTrak, &wearBits, &wear))
            continue;

        InventoryVisualRefreshRecord* record =
            CaptureInventoryVisualRefreshRecord(entity,
                original.entityHandle, definitionIndex, paintKit,
                seed, statTrak, wearBits);
        if (!record)
            continue;

        if (record->refreshCooldown > 0)
            --record->refreshCooldown;
        if (record->refreshBudget <= 0 || record->refreshCooldown != 0)
            continue;

        InventoryVisualApplyItemViewState(entity, paintKit, seed, wear);

        if (g_inventoryVisualRefreshRuntime.compositeReady &&
            g_inventoryVisualRefreshRuntime.compositeOwnerOffsetReady)
        {
            BYTE* owner = reinterpret_cast<BYTE*>(entity) +
                g_inventoryVisualRefreshRuntime.compositeOwnerOffset;
            if (IsAccessible(owner, 0x20, false))
            {
                g_inventoryVisualRefreshRuntime.updateCompositeMaterial(
                    owner, 1);
                ++g_inventoryCompositeRefreshCalls;
            }
        }
        if (g_inventoryVisualRefreshRuntime.compositeSetReady)
        {
            g_inventoryVisualRefreshRuntime.updateCompositeMaterialSet(
                entity, 0);
            ++g_inventoryCompositeSetRefreshCalls;
        }
        if (g_inventoryVisualRefreshRuntime.skinReady)
        {
            g_inventoryVisualRefreshRuntime.updateSkin(entity, true);
            ++g_inventoryVisualRefreshCalls;
        }

        --record->refreshBudget;
        // Let asynchronous composite construction progress before another kick.
        record->refreshCooldown = 30;
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
