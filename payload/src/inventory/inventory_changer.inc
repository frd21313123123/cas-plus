// cas+ inventory changer runtime.
// This module is injected into the generated payload source by
// tools/apply-inventory-changer.ps1 after the visual pipeline patcher runs.

struct InventoryChangerConfig {
    bool enabled;
    int paintKit;
    int seed;
    int statTrak;
    float wear;
};

static InventoryChangerConfig g_inventoryChanger = {
    false,
    711,   // AK-47 | The Empress (example paint kit)
    0,
    -1,
    0.0001f
};

struct InventoryRuntime {
    bool ready;
    unsigned int attributeManagerOffset;
    unsigned int itemOffset;
    unsigned int itemIdHighOffset;
    unsigned int fallbackPaintKitOffset;
    unsigned int fallbackSeedOffset;
    unsigned int fallbackWearOffset;
    unsigned int fallbackStatTrakOffset;
};

static InventoryRuntime g_inventoryRuntime{};

struct OriginalWeaponCosmetics {
    unsigned int entityHandle;
    int paintKit;
    int seed;
    int statTrak;
    float wear;
    unsigned int itemIdHigh;
};

static OriginalWeaponCosmetics g_originalWeaponCosmetics[64]{};
static int g_originalWeaponCosmeticsCount = 0;
static void* g_inventoryLastEntitySystem = nullptr;

static bool ResolveInventoryRuntime(InventoryRuntime* runtime)
{
    if (!runtime)
        return false;

    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;

    SchemaClassInfoView* econEntity = FindSchemaClass(scope, "C_EconEntity");
    SchemaClassInfoView* attributeContainer = FindSchemaClass(scope, "C_AttributeContainer");
    SchemaClassInfoView* econItemView = FindSchemaClass(scope, "C_EconItemView");
    if (!econEntity || !attributeContainer || !econItemView)
        return false;

    unsigned int attributeManager = 0;
    unsigned int item = 0;
    unsigned int itemIdHigh = 0;
    unsigned int paintKit = 0;
    unsigned int seed = 0;
    unsigned int wear = 0;
    unsigned int statTrak = 0;

    if (!FindSchemaField(econEntity, "m_AttributeManager", &attributeManager) ||
        !FindSchemaField(attributeContainer, "m_Item", &item) ||
        !FindSchemaField(econItemView, "m_iItemIDHigh", &itemIdHigh) ||
        !FindSchemaField(econEntity, "m_nFallbackPaintKit", &paintKit) ||
        !FindSchemaField(econEntity, "m_nFallbackSeed", &seed) ||
        !FindSchemaField(econEntity, "m_flFallbackWear", &wear) ||
        !FindSchemaField(econEntity, "m_nFallbackStatTrak", &statTrak))
        return false;

    if (attributeManager >= static_cast<unsigned int>(econEntity->size) ||
        item >= static_cast<unsigned int>(attributeContainer->size) ||
        itemIdHigh + sizeof(unsigned int) > static_cast<unsigned int>(econItemView->size) ||
        paintKit + sizeof(int) > static_cast<unsigned int>(econEntity->size) ||
        seed + sizeof(int) > static_cast<unsigned int>(econEntity->size) ||
        wear + sizeof(float) > static_cast<unsigned int>(econEntity->size) ||
        statTrak + sizeof(int) > static_cast<unsigned int>(econEntity->size))
        return false;

    runtime->attributeManagerOffset = attributeManager;
    runtime->itemOffset = item;
    runtime->itemIdHighOffset = itemIdHigh;
    runtime->fallbackPaintKitOffset = paintKit;
    runtime->fallbackSeedOffset = seed;
    runtime->fallbackWearOffset = wear;
    runtime->fallbackStatTrakOffset = statTrak;
    runtime->ready = true;
    return true;
}

static OriginalWeaponCosmetics* FindOriginalWeaponCosmetics(unsigned int entityHandle)
{
    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
        if (g_originalWeaponCosmetics[i].entityHandle == entityHandle)
            return &g_originalWeaponCosmetics[i];
    return nullptr;
}

static bool CaptureOriginalWeaponCosmetics(void* weapon, unsigned int handle)
{
    if (!weapon || handle == 0xFFFFFFFFu ||
        g_originalWeaponCosmeticsCount >= static_cast<int>(sizeof(g_originalWeaponCosmetics) / sizeof(g_originalWeaponCosmetics[0])))
        return false;
    if (FindOriginalWeaponCosmetics(handle))
        return true;

    BYTE* base = reinterpret_cast<BYTE*>(weapon);
    BYTE* itemView = base + g_inventoryRuntime.attributeManagerOffset + g_inventoryRuntime.itemOffset;
    int* paintKit = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackPaintKitOffset);
    int* seed = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackSeedOffset);
    float* wear = reinterpret_cast<float*>(base + g_inventoryRuntime.fallbackWearOffset);
    int* statTrak = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackStatTrakOffset);
    unsigned int* itemIdHigh = reinterpret_cast<unsigned int*>(itemView + g_inventoryRuntime.itemIdHighOffset);

    if (!IsAccessible(paintKit, sizeof(int), false) ||
        !IsAccessible(seed, sizeof(int), false) ||
        !IsAccessible(wear, sizeof(float), false) ||
        !IsAccessible(statTrak, sizeof(int), false) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), false))
        return false;

    OriginalWeaponCosmetics& original = g_originalWeaponCosmetics[g_originalWeaponCosmeticsCount++];
    original.entityHandle = handle;
    original.paintKit = *paintKit;
    original.seed = *seed;
    original.statTrak = *statTrak;
    original.wear = *wear;
    original.itemIdHigh = *itemIdHigh;
    return true;
}

static bool ApplyWeaponCosmetics(void* weapon)
{
    if (!weapon || !g_inventoryRuntime.ready)
        return false;
    const unsigned int handle = EntityHandleFor(weapon);
    if (handle == 0xFFFFFFFFu || !CaptureOriginalWeaponCosmetics(weapon, handle))
        return false;

    BYTE* base = reinterpret_cast<BYTE*>(weapon);
    BYTE* itemView = base + g_inventoryRuntime.attributeManagerOffset + g_inventoryRuntime.itemOffset;
    int* paintKit = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackPaintKitOffset);
    int* seed = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackSeedOffset);
    float* wear = reinterpret_cast<float*>(base + g_inventoryRuntime.fallbackWearOffset);
    int* statTrak = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackStatTrakOffset);
    unsigned int* itemIdHigh = reinterpret_cast<unsigned int*>(itemView + g_inventoryRuntime.itemIdHighOffset);

    if (!IsAccessible(paintKit, sizeof(int), true) ||
        !IsAccessible(seed, sizeof(int), true) ||
        !IsAccessible(wear, sizeof(float), true) ||
        !IsAccessible(statTrak, sizeof(int), true) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), true))
        return false;

    int clampedPaint = g_inventoryChanger.paintKit < 0 ? 0 : g_inventoryChanger.paintKit;
    int clampedSeed = g_inventoryChanger.seed < 0 ? 0 : (g_inventoryChanger.seed > 1000 ? 1000 : g_inventoryChanger.seed);
    float clampedWear = g_inventoryChanger.wear;
    if (clampedWear < 0.0001f) clampedWear = 0.0001f;
    if (clampedWear > 1.0f) clampedWear = 1.0f;

    *itemIdHigh = 0xFFFFFFFFu;
    *paintKit = clampedPaint;
    *seed = clampedSeed;
    *wear = clampedWear;
    *statTrak = g_inventoryChanger.statTrak;
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
        const OriginalWeaponCosmetics& original = g_originalWeaponCosmetics[i];
        void* weapon = EntityFromHandle(g_preResolvedEntityRuntime, entitySystem, original.entityHandle);
        if (!weapon)
            continue;

        BYTE* base = reinterpret_cast<BYTE*>(weapon);
        BYTE* itemView = base + g_inventoryRuntime.attributeManagerOffset + g_inventoryRuntime.itemOffset;
        int* paintKit = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackPaintKitOffset);
        int* seed = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackSeedOffset);
        float* wear = reinterpret_cast<float*>(base + g_inventoryRuntime.fallbackWearOffset);
        int* statTrak = reinterpret_cast<int*>(base + g_inventoryRuntime.fallbackStatTrakOffset);
        unsigned int* itemIdHigh = reinterpret_cast<unsigned int*>(itemView + g_inventoryRuntime.itemIdHighOffset);

        if (IsAccessible(paintKit, sizeof(int), true)) *paintKit = original.paintKit;
        if (IsAccessible(seed, sizeof(int), true)) *seed = original.seed;
        if (IsAccessible(wear, sizeof(float), true)) *wear = original.wear;
        if (IsAccessible(statTrak, sizeof(int), true)) *statTrak = original.statTrak;
        if (IsAccessible(itemIdHigh, sizeof(unsigned int), true)) *itemIdHigh = original.itemIdHigh;
    }
    g_originalWeaponCosmeticsCount = 0;
}

static void UpdateInventoryChanger()
{
    if (!g_preResolvedEntityRuntimeReady || !g_botHighlightRuntimeReady)
        return;

    if (!g_inventoryRuntime.ready && !ResolveInventoryRuntime(&g_inventoryRuntime))
        return;

    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        g_originalWeaponCosmeticsCount = 0;
        g_inventoryLastEntitySystem = nullptr;
        return;
    }

    if (g_inventoryLastEntitySystem && g_inventoryLastEntitySystem != entitySystem)
        g_originalWeaponCosmeticsCount = 0;
    g_inventoryLastEntitySystem = entitySystem;

    if (!g_inventoryChanger.enabled)
    {
        if (g_originalWeaponCosmeticsCount > 0)
            RestoreInventoryChanger(entitySystem);
        return;
    }

    void* localController = nullptr;
    for (int index = 1; index <= 64; ++index)
    {
        void* controller = EntityAtIndex(g_preResolvedEntityRuntime, entitySystem, index);
        if (!HasDesignerName(controller, "cs_player_controller"))
            continue;
        BYTE* localFlag = reinterpret_cast<BYTE*>(controller) + g_botHighlightRuntime.localControllerOffset;
        if (IsAccessible(localFlag, 1, false) && *localFlag != 0)
        {
            localController = controller;
            break;
        }
    }
    if (!localController)
        return;

    unsigned int* pawnHandle = reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(localController) + g_botHighlightRuntime.playerPawnHandleOffset);
    if (!IsAccessible(pawnHandle, sizeof(unsigned int), false))
        return;
    void* pawn = EntityFromHandle(g_preResolvedEntityRuntime, entitySystem, *pawnHandle);
    if (!pawn)
        return;

    void** weaponServices = reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(pawn) + g_botHighlightRuntime.weaponServicesOffset);
    if (!IsAccessible(weaponServices, sizeof(void*), false) || !*weaponServices)
        return;
    unsigned int* activeWeaponHandle = reinterpret_cast<unsigned int*>(
        reinterpret_cast<BYTE*>(*weaponServices) + g_botHighlightRuntime.activeWeaponOffset);
    if (!IsAccessible(activeWeaponHandle, sizeof(unsigned int), false))
        return;

    void* activeWeapon = EntityFromHandle(g_preResolvedEntityRuntime, entitySystem, *activeWeaponHandle);
    if (activeWeapon)
        ApplyWeaponCosmetics(activeWeapon);
}
