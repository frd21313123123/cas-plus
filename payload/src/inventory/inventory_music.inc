// cas+ music-kit virtual inventory domain.
//
// Current client schema exposes CCSPlayerController::m_iMusicKitID and
// m_iMusicKitMVPs as plain int32 fields, so this adapter does not need econ
// attribute vectors. Music kits remain persistent VirtualInventoryItem entries;
// their StatTrak value maps to the local music-kit MVP counter when available.

struct InventoryMusicCatalogEntry {
    unsigned short musicKitId;
    const wchar_t* displayName;
};

static const InventoryMusicCatalogEntry kInventoryMusicKits[] = {
    { 1,  L"Valve - CS2" },
    { 32, L"Beartooth 02" },
    { 43, L"Austin Wintory 02" },
    { 54, L"Austin Wintory 03" },
    { 62, L"3kliksphilip 01" },
    { 71, L"Dryden 01" },
    { 78, L"Austin Wintory 04" },
    { 79, L"Ben Bromfield 01" },
    { 80, L"Daniel Sadowski 05" },
    { 81, L"Dren 03" }
};

static int InventoryMusicCatalogCount()
{
    return static_cast<int>(sizeof(kInventoryMusicKits) /
        sizeof(kInventoryMusicKits[0]));
}

static const wchar_t* InventoryMusicName(unsigned short musicKitId)
{
    for (int i = 0; i < InventoryMusicCatalogCount(); ++i)
        if (kInventoryMusicKits[i].musicKitId == musicKitId)
            return kInventoryMusicKits[i].displayName;
    return L"Raw music kit ID";
}

static unsigned short InventoryNextMusicKit(
    unsigned short currentId, int direction)
{
    if (direction == 0 || InventoryMusicCatalogCount() <= 0)
        return currentId;
    int current = -1;
    for (int i = 0; i < InventoryMusicCatalogCount(); ++i)
        if (kInventoryMusicKits[i].musicKitId == currentId)
        {
            current = i;
            break;
        }
    if (current < 0)
        current = direction > 0 ? 0 : InventoryMusicCatalogCount() - 1;
    else
    {
        current += direction > 0 ? 1 : -1;
        if (current < 0) current = InventoryMusicCatalogCount() - 1;
        if (current >= InventoryMusicCatalogCount()) current = 0;
    }
    return kInventoryMusicKits[current].musicKitId;
}

static const wchar_t* InventoryCatalogDomainName(
    unsigned short slotDefinitionIndex, unsigned short definitionIndex)
{
    if (slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return InventoryMusicName(definitionIndex);
    return InventoryCatalogAnyName(slotDefinitionIndex, definitionIndex);
}

static void InventoryUiAddMusicPreset()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        ZeroBytes(&item, sizeof(item));
        item.itemId = g_inventoryStore.nextItemId++;
        item.slotDefinitionIndex = INVENTORY_SLOT_MUSIC;
        item.overrideDefinitionIndex = 1;
        item.paintKit = 0;
        item.seed = 0;
        item.statTrak = -1;
        item.wear = 0.0001f;
        item.quality = 0;
        item.equippedTeams = INVENTORY_TEAM_BOTH;
        const int index = g_inventoryStore.itemCount++;
        g_inventoryStore.items[index] = item;
        InventoryEquipItemLocked(index, INVENTORY_TEAM_BOTH);
        g_inventoryUiSelected = index;
        g_inventoryUiPage = index / 7;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiCycleMusicCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
    {
        item->overrideDefinitionIndex = InventoryNextMusicKit(
            item->overrideDefinitionIndex, direction);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

struct InventoryMusicRuntime {
    bool ready;
    unsigned int musicKitIdOffset;
    unsigned int musicKitMvpsOffset;
    bool hasMusicKitMvps;
    unsigned int controllerSize;
};

static InventoryMusicRuntime g_inventoryMusicRuntime{};

struct OriginalMusicKitState {
    bool captured;
    void* controllerAddress;
    void* controllerIdentity;
    int musicKitId;
    int musicKitMvps;
};

static OriginalMusicKitState g_originalMusicKitState{};
static unsigned long long g_lastProjectedMusicItemId = 0;
static unsigned short g_lastProjectedMusicKitId = 0;
static int g_lastProjectedMusicMvps = -2147483647;
static BYTE g_lastProjectedMusicTeam = 0;
static int g_inventoryMusicApplyCount = 0;

static void ResetProjectedMusicCache()
{
    g_lastProjectedMusicItemId = 0;
    g_lastProjectedMusicKitId = 0;
    g_lastProjectedMusicMvps = -2147483647;
    g_lastProjectedMusicTeam = 0;
}

static bool ResolveInventoryMusicRuntime(InventoryMusicRuntime* runtime)
{
    if (!runtime)
        return false;
    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;
    SchemaClassInfoView* controller =
        FindSchemaClass(scope, "CCSPlayerController");
    if (!controller)
        return false;

    unsigned int musicKitId = 0;
    unsigned int musicKitMvps = 0;
    if (!FindSchemaField(controller, "m_iMusicKitID", &musicKitId) ||
        musicKitId + sizeof(int) >
            static_cast<unsigned int>(controller->size))
        return false;

    const bool hasMvps = FindSchemaField(controller,
        "m_iMusicKitMVPs", &musicKitMvps) &&
        musicKitMvps + sizeof(int) <=
            static_cast<unsigned int>(controller->size);

    runtime->musicKitIdOffset = musicKitId;
    runtime->musicKitMvpsOffset = musicKitMvps;
    runtime->hasMusicKitMvps = hasMvps;
    runtime->controllerSize = static_cast<unsigned int>(controller->size);
    runtime->ready = true;
    return true;
}

static bool InventoryMusicStateMatchesController(void* controller)
{
    if (!g_originalMusicKitState.captured || !controller ||
        controller != g_originalMusicKitState.controllerAddress ||
        !IsAccessible(controller, 0x18, false))
        return false;
    return *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(controller) + 0x10) ==
        g_originalMusicKitState.controllerIdentity;
}

static bool CaptureOriginalMusicKitState(void* controller)
{
    if (!controller || !g_inventoryMusicRuntime.ready ||
        !IsAccessible(controller, 0x18, false))
        return false;
    void* identity = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(controller) + 0x10);
    if (g_originalMusicKitState.captured &&
        g_originalMusicKitState.controllerAddress == controller &&
        g_originalMusicKitState.controllerIdentity == identity)
        return true;

    int* musicKit = reinterpret_cast<int*>(
        reinterpret_cast<BYTE*>(controller) +
        g_inventoryMusicRuntime.musicKitIdOffset);
    if (!IsAccessible(musicKit, sizeof(int), false))
        return false;

    ZeroBytes(&g_originalMusicKitState, sizeof(g_originalMusicKitState));
    g_originalMusicKitState.captured = true;
    g_originalMusicKitState.controllerAddress = controller;
    g_originalMusicKitState.controllerIdentity = identity;
    g_originalMusicKitState.musicKitId = *musicKit;
    if (g_inventoryMusicRuntime.hasMusicKitMvps)
    {
        int* mvps = reinterpret_cast<int*>(
            reinterpret_cast<BYTE*>(controller) +
            g_inventoryMusicRuntime.musicKitMvpsOffset);
        if (IsAccessible(mvps, sizeof(int), false))
            g_originalMusicKitState.musicKitMvps = *mvps;
    }
    return true;
}

static void RestoreInventoryMusic(void* controller)
{
    if (!InventoryMusicStateMatchesController(controller))
    {
        ZeroBytes(&g_originalMusicKitState, sizeof(g_originalMusicKitState));
        ResetProjectedMusicCache();
        return;
    }
    int* musicKit = reinterpret_cast<int*>(
        reinterpret_cast<BYTE*>(controller) +
        g_inventoryMusicRuntime.musicKitIdOffset);
    if (IsAccessible(musicKit, sizeof(int), true))
        *musicKit = g_originalMusicKitState.musicKitId;
    if (g_inventoryMusicRuntime.hasMusicKitMvps)
    {
        int* mvps = reinterpret_cast<int*>(
            reinterpret_cast<BYTE*>(controller) +
            g_inventoryMusicRuntime.musicKitMvpsOffset);
        if (IsAccessible(mvps, sizeof(int), true))
            *mvps = g_originalMusicKitState.musicKitMvps;
    }
    ZeroBytes(&g_originalMusicKitState, sizeof(g_originalMusicKitState));
    ResetProjectedMusicCache();
}

static bool FindEquippedVirtualMusic(BYTE teamBit,
    VirtualInventoryItem* output)
{
    if (!output || teamBit == INVENTORY_TEAM_NONE || !InventoryTryLock())
        return false;
    bool found = false;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
    {
        const VirtualInventoryItem& item = g_inventoryStore.items[i];
        if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC &&
            (item.equippedTeams & teamBit) != 0)
        {
            *output = item;
            found = true;
            break;
        }
    }
    InventoryUnlock();
    return found;
}

static void UpdateInventoryMusic()
{
    if (!g_preResolvedEntityRuntimeReady || !g_botHighlightRuntimeReady)
        return;
    if (!g_inventoryMusicRuntime.ready &&
        !ResolveInventoryMusicRuntime(&g_inventoryMusicRuntime))
        return;

    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        ZeroBytes(&g_originalMusicKitState, sizeof(g_originalMusicKitState));
        ResetProjectedMusicCache();
        return;
    }

    void* localController = nullptr;
    void* localPawn = nullptr;
    unsigned int pawnHandle = 0xFFFFFFFFu;
    BYTE localTeam = 0;
    if (!InventoryResolveLocal(entitySystem, &localController, &localPawn,
        &pawnHandle, &localTeam) || !localController)
        return;

    if (g_originalMusicKitState.captured &&
        !InventoryMusicStateMatchesController(localController))
    {
        ZeroBytes(&g_originalMusicKitState, sizeof(g_originalMusicKitState));
        ResetProjectedMusicCache();
    }

    bool storeEnabled = false;
    if (InventoryTryLock())
    {
        storeEnabled = g_inventoryStore.enabled;
        InventoryUnlock();
    }
    else
        return;

    VirtualInventoryItem selected;
    const bool hasSelected = storeEnabled &&
        FindEquippedVirtualMusic(InventoryTeamBit(localTeam), &selected);
    if (!hasSelected)
    {
        if (g_originalMusicKitState.captured)
            RestoreInventoryMusic(localController);
        return;
    }

    if (!CaptureOriginalMusicKitState(localController))
        return;
    if (g_lastProjectedMusicItemId == selected.itemId &&
        g_lastProjectedMusicKitId == selected.overrideDefinitionIndex &&
        g_lastProjectedMusicMvps == selected.statTrak &&
        g_lastProjectedMusicTeam == localTeam)
        return;

    int* musicKit = reinterpret_cast<int*>(
        reinterpret_cast<BYTE*>(localController) +
        g_inventoryMusicRuntime.musicKitIdOffset);
    if (!IsAccessible(musicKit, sizeof(int), true))
        return;
    *musicKit = static_cast<int>(selected.overrideDefinitionIndex);

    if (g_inventoryMusicRuntime.hasMusicKitMvps)
    {
        int* mvps = reinterpret_cast<int*>(
            reinterpret_cast<BYTE*>(localController) +
            g_inventoryMusicRuntime.musicKitMvpsOffset);
        if (IsAccessible(mvps, sizeof(int), true))
            *mvps = selected.statTrak >= 0 ? selected.statTrak :
                g_originalMusicKitState.musicKitMvps;
    }

    g_lastProjectedMusicItemId = selected.itemId;
    g_lastProjectedMusicKitId = selected.overrideDefinitionIndex;
    g_lastProjectedMusicMvps = selected.statTrak;
    g_lastProjectedMusicTeam = localTeam;
    ++g_inventoryMusicApplyCount;
}
