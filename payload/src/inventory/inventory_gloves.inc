// cas+ glove inventory domain.
//
// Gloves are not weapon C_EconEntity instances in the client. The current
// C_CSPlayerPawn owns an embedded C_EconItemView (m_EconGloves) and a
// m_bNeedToReApplyGloves lifecycle flag, so glove definition projection is kept
// in a separate adapter instead of forcing it through weapon fallback fields.
//
// Paint/wear remain stored in the VirtualInventoryItem for forward
// compatibility, but are intentionally NOT projected here: current glove paint
// is represented through econ attributes and the client-side mutation API for
// C_UtlVectorEmbeddedNetworkVar<CEconItemAttribute> has not been validated.

struct InventoryGloveCatalogEntry {
    unsigned short definitionIndex;
    const wchar_t* displayName;
};

static const InventoryGloveCatalogEntry kInventoryGloves[] = {
    { 4725, L"Broken Fang Gloves" },
    { 5027, L"Bloodhound Gloves" },
    { 5030, L"Sport Gloves" },
    { 5031, L"Driver Gloves" },
    { 5032, L"Hand Wraps" },
    { 5033, L"Moto Gloves" },
    { 5034, L"Specialist Gloves" },
    { 5035, L"Hydra Gloves" }
};

static int InventoryGloveCatalogCount()
{
    return static_cast<int>(sizeof(kInventoryGloves) /
        sizeof(kInventoryGloves[0]));
}

static const wchar_t* InventoryGloveName(unsigned short definitionIndex)
{
    for (int i = 0; i < InventoryGloveCatalogCount(); ++i)
        if (kInventoryGloves[i].definitionIndex == definitionIndex)
            return kInventoryGloves[i].displayName;
    return L"Unknown gloves";
}

static bool InventoryIsGloveDefinition(unsigned short definitionIndex)
{
    for (int i = 0; i < InventoryGloveCatalogCount(); ++i)
        if (kInventoryGloves[i].definitionIndex == definitionIndex)
            return true;
    return false;
}

static const wchar_t* InventoryCatalogAnyName(
    unsigned short slotDefinitionIndex, unsigned short definitionIndex)
{
    if (slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        return InventoryGloveName(definitionIndex);
    return InventoryCatalogWeaponName(definitionIndex);
}

static unsigned short InventoryNextGloveDefinition(
    unsigned short currentDefinition, int direction)
{
    const int count = InventoryGloveCatalogCount();
    if (count <= 0 || direction == 0)
        return currentDefinition;
    int current = -1;
    for (int i = 0; i < count; ++i)
        if (kInventoryGloves[i].definitionIndex == currentDefinition)
        {
            current = i;
            break;
        }
    if (current < 0)
        current = 0;
    else
    {
        current += direction > 0 ? 1 : -1;
        if (current < 0) current = count - 1;
        if (current >= count) current = 0;
    }
    return kInventoryGloves[current].definitionIndex;
}

static void InventoryUiCycleGloveCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
    {
        item->overrideDefinitionIndex = InventoryNextGloveDefinition(
            item->overrideDefinitionIndex, direction);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiAddGlovePreset()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem item;
        ZeroBytes(&item, sizeof(item));
        item.itemId = g_inventoryStore.nextItemId++;
        item.slotDefinitionIndex = INVENTORY_SLOT_GLOVE;
        item.overrideDefinitionIndex = 5030; // Sport Gloves default cursor.
        item.paintKit = 0;
        item.seed = 0;
        item.statTrak = -1;
        item.wear = 0.0001f;
        item.quality = 3; // Unusual is the normal cosmetic glove quality.
        item.equippedTeams = InventoryTeamBit(g_inventoryActiveTeam);
        if (item.equippedTeams == INVENTORY_TEAM_NONE)
            item.equippedTeams = INVENTORY_TEAM_BOTH;
        const int index = g_inventoryStore.itemCount++;
        g_inventoryStore.items[index] = item;
        InventoryEquipItemLocked(index, item.equippedTeams);
        g_inventoryUiSelected = index;
        g_inventoryUiPage = index / 7;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

struct InventoryGloveRuntime {
    bool ready;
    unsigned int needReapplyOffset;
    unsigned int econGlovesOffset;
    unsigned int pawnSize;
    unsigned int econItemViewSize;
};

static InventoryGloveRuntime g_inventoryGloveRuntime{};

struct OriginalGloveState {
    bool captured;
    void* pawnAddress;
    void* pawnIdentity;
    unsigned short definitionIndex;
    int quality;
    unsigned int itemIdHigh;
    unsigned int itemIdLow;
    BYTE initialized;
};

static OriginalGloveState g_originalGloveState{};
static unsigned long long g_lastProjectedGloveItemId = 0;
static unsigned short g_lastProjectedGloveDefinition = 0;
static BYTE g_lastProjectedGloveTeam = 0;
static int g_inventoryGloveApplyCount = 0;

static bool ResolveInventoryGloveRuntime(InventoryGloveRuntime* runtime)
{
    if (!runtime || !g_inventoryRuntime.ready)
        return false;
    void* scope = nullptr;
    if (!ResolveClientSchemaScope(&scope))
        return false;
    SchemaClassInfoView* pawn = FindSchemaClass(scope, "C_CSPlayerPawn");
    SchemaClassInfoView* econItemView =
        FindSchemaClass(scope, "C_EconItemView");
    if (!pawn || !econItemView)
        return false;

    unsigned int needReapply = 0;
    unsigned int econGloves = 0;
    if (!FindSchemaField(pawn, "m_bNeedToReApplyGloves", &needReapply) ||
        !FindSchemaField(pawn, "m_EconGloves", &econGloves))
        return false;

    if (needReapply + sizeof(BYTE) > static_cast<unsigned int>(pawn->size) ||
        econGloves + static_cast<unsigned int>(econItemView->size) >
            static_cast<unsigned int>(pawn->size))
        return false;

    runtime->needReapplyOffset = needReapply;
    runtime->econGlovesOffset = econGloves;
    runtime->pawnSize = static_cast<unsigned int>(pawn->size);
    runtime->econItemViewSize = static_cast<unsigned int>(econItemView->size);
    runtime->ready = true;
    return true;
}

static BYTE* InventoryGloveItemView(void* localPawn)
{
    if (!localPawn || !g_inventoryGloveRuntime.ready)
        return nullptr;
    BYTE* view = reinterpret_cast<BYTE*>(localPawn) +
        g_inventoryGloveRuntime.econGlovesOffset;
    return IsAccessible(view, g_inventoryGloveRuntime.econItemViewSize, false) ?
        view : nullptr;
}

static bool CaptureOriginalGloveState(void* localPawn)
{
    if (!localPawn || !g_inventoryGloveRuntime.ready ||
        !g_inventoryRuntime.ready || !IsAccessible(localPawn, 0x18, false))
        return false;
    void* identity = *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(localPawn) + 0x10);
    if (g_originalGloveState.captured &&
        g_originalGloveState.pawnAddress == localPawn &&
        g_originalGloveState.pawnIdentity == identity)
        return true;

    BYTE* view = InventoryGloveItemView(localPawn);
    if (!view)
        return false;
    auto* definition = reinterpret_cast<unsigned short*>(
        view + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        view + g_inventoryRuntime.itemIdHighOffset);
    if (!IsAccessible(definition, sizeof(unsigned short), false) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), false))
        return false;

    ZeroBytes(&g_originalGloveState, sizeof(g_originalGloveState));
    g_originalGloveState.captured = true;
    g_originalGloveState.pawnAddress = localPawn;
    g_originalGloveState.pawnIdentity = identity;
    g_originalGloveState.definitionIndex = *definition;
    g_originalGloveState.itemIdHigh = *itemIdHigh;
    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            view + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), false))
            g_originalGloveState.quality = *quality;
    }
    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            view + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), false))
            g_originalGloveState.itemIdLow = *itemIdLow;
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = view + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), false))
            g_originalGloveState.initialized = *initialized;
    }
    return true;
}

static bool InventoryGloveStateMatchesPawn(void* localPawn)
{
    if (!g_originalGloveState.captured || !localPawn ||
        localPawn != g_originalGloveState.pawnAddress ||
        !IsAccessible(localPawn, 0x18, false))
        return false;
    return *reinterpret_cast<void**>(
        reinterpret_cast<BYTE*>(localPawn) + 0x10) ==
        g_originalGloveState.pawnIdentity;
}

static void MarkGlovesForReapply(void* localPawn)
{
    if (!localPawn || !g_inventoryGloveRuntime.ready)
        return;
    BYTE* flag = reinterpret_cast<BYTE*>(localPawn) +
        g_inventoryGloveRuntime.needReapplyOffset;
    if (IsAccessible(flag, sizeof(BYTE), true))
        *flag = 1;
}

static void RestoreInventoryGloves(void* localPawn)
{
    if (!InventoryGloveStateMatchesPawn(localPawn) ||
        !g_inventoryRuntime.ready)
    {
        ZeroBytes(&g_originalGloveState, sizeof(g_originalGloveState));
        g_lastProjectedGloveItemId = 0;
        g_lastProjectedGloveDefinition = 0;
        return;
    }

    BYTE* view = InventoryGloveItemView(localPawn);
    if (!view)
        return;
    auto* definition = reinterpret_cast<unsigned short*>(
        view + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        view + g_inventoryRuntime.itemIdHighOffset);
    if (IsAccessible(definition, sizeof(unsigned short), true))
        *definition = g_originalGloveState.definitionIndex;
    if (IsAccessible(itemIdHigh, sizeof(unsigned int), true))
        *itemIdHigh = g_originalGloveState.itemIdHigh;
    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            view + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), true))
            *quality = g_originalGloveState.quality;
    }
    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            view + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), true))
            *itemIdLow = g_originalGloveState.itemIdLow;
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = view + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = g_originalGloveState.initialized;
    }
    MarkGlovesForReapply(localPawn);
    ZeroBytes(&g_originalGloveState, sizeof(g_originalGloveState));
    g_lastProjectedGloveItemId = 0;
    g_lastProjectedGloveDefinition = 0;
}

static bool FindEquippedVirtualGloves(BYTE teamBit,
    VirtualInventoryItem* output)
{
    if (!output || teamBit == INVENTORY_TEAM_NONE || !InventoryTryLock())
        return false;
    bool found = false;
    for (int i = 0; i < g_inventoryStore.itemCount; ++i)
    {
        const VirtualInventoryItem& item = g_inventoryStore.items[i];
        if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE &&
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

static void UpdateInventoryGloves()
{
    if (!g_inventoryRuntime.ready || !g_preResolvedEntityRuntimeReady ||
        !g_botHighlightRuntimeReady)
        return;
    if (!g_inventoryGloveRuntime.ready &&
        !ResolveInventoryGloveRuntime(&g_inventoryGloveRuntime))
        return;

    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
    {
        ZeroBytes(&g_originalGloveState, sizeof(g_originalGloveState));
        g_lastProjectedGloveItemId = 0;
        return;
    }

    void* localController = nullptr;
    void* localPawn = nullptr;
    unsigned int localPawnHandle = 0xFFFFFFFFu;
    BYTE localTeam = 0;
    if (!InventoryResolveLocal(entitySystem, &localController, &localPawn,
        &localPawnHandle, &localTeam))
        return;

    if (g_originalGloveState.captured &&
        !InventoryGloveStateMatchesPawn(localPawn))
    {
        ZeroBytes(&g_originalGloveState, sizeof(g_originalGloveState));
        g_lastProjectedGloveItemId = 0;
        g_lastProjectedGloveDefinition = 0;
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
        FindEquippedVirtualGloves(InventoryTeamBit(localTeam), &selected);
    if (!hasSelected)
    {
        if (g_originalGloveState.captured)
            RestoreInventoryGloves(localPawn);
        return;
    }

    if (!InventoryIsGloveDefinition(selected.overrideDefinitionIndex))
        return;
    if (!CaptureOriginalGloveState(localPawn))
        return;

    // Avoid marking the pawn dirty each frame. Reapply only when the selected
    // virtual glove instance/definition or team actually changes.
    if (g_lastProjectedGloveItemId == selected.itemId &&
        g_lastProjectedGloveDefinition == selected.overrideDefinitionIndex &&
        g_lastProjectedGloveTeam == localTeam)
        return;

    BYTE* view = InventoryGloveItemView(localPawn);
    if (!view)
        return;
    auto* definition = reinterpret_cast<unsigned short*>(
        view + g_inventoryRuntime.itemDefinitionIndexOffset);
    auto* itemIdHigh = reinterpret_cast<unsigned int*>(
        view + g_inventoryRuntime.itemIdHighOffset);
    if (!IsAccessible(definition, sizeof(unsigned short), true) ||
        !IsAccessible(itemIdHigh, sizeof(unsigned int), true))
        return;

    *definition = selected.overrideDefinitionIndex;
    *itemIdHigh = 0xFFFFFFFFu;
    if (g_inventoryRuntime.hasQuality)
    {
        int* quality = reinterpret_cast<int*>(
            view + g_inventoryRuntime.entityQualityOffset);
        if (IsAccessible(quality, sizeof(int), true))
            *quality = selected.quality ? selected.quality : 3;
    }
    if (g_inventoryRuntime.hasItemIdLow)
    {
        unsigned int* itemIdLow = reinterpret_cast<unsigned int*>(
            view + g_inventoryRuntime.itemIdLowOffset);
        if (IsAccessible(itemIdLow, sizeof(unsigned int), true))
            *itemIdLow = static_cast<unsigned int>(selected.itemId);
    }
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = view + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    MarkGlovesForReapply(localPawn);
    g_lastProjectedGloveItemId = selected.itemId;
    g_lastProjectedGloveDefinition = selected.overrideDefinitionIndex;
    g_lastProjectedGloveTeam = localTeam;
    ++g_inventoryGloveApplyCount;
}
