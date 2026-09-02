// Final-stage inventory runtime policies. No new client offsets/signatures.
// Injected after the validated econ and attachment adapters are available.

static void InventoryRuntimeHashBytes(unsigned int* hash, const void* data,
    SIZE_T size)
{
    const BYTE* bytes = reinterpret_cast<const BYTE*>(data);
    for (SIZE_T i = 0; i < size; ++i)
    {
        *hash ^= bytes[i];
        *hash *= 16777619u;
    }
}

struct InventoryRuntimeRetry {
    unsigned long long itemId;
    unsigned int signature;
    unsigned int nextFrame;
};
static InventoryRuntimeRetry g_inventoryRuntimeRetries[INVENTORY_MAX_ITEMS]{};
static unsigned int g_inventoryRuntimeFrame = 0;

static InventoryRuntimeRetry* InventoryRuntimeRetryFor(unsigned long long itemId)
{
    for (int i = 0; i < INVENTORY_MAX_ITEMS; ++i)
        if (g_inventoryRuntimeRetries[i].itemId == itemId)
            return &g_inventoryRuntimeRetries[i];
    for (int i = 0; i < INVENTORY_MAX_ITEMS; ++i)
        if (!g_inventoryRuntimeRetries[i].itemId ||
            static_cast<int>(g_inventoryRuntimeFrame - g_inventoryRuntimeRetries[i].nextFrame) >= 0)
        {
            g_inventoryRuntimeRetries[i] = {};
            g_inventoryRuntimeRetries[i].itemId = itemId;
            return &g_inventoryRuntimeRetries[i];
        }
    return nullptr;
}

static bool InventoryRuntimeRetryDue(unsigned int nextFrame)
{
    return nextFrame == 0 || static_cast<int>(g_inventoryRuntimeFrame - nextFrame) >= 0;
}

static unsigned int InventoryRuntimeAppearanceSignature(
    const VirtualInventoryItem& item)
{
    // Hash semantic appearance fields only: equip state is handled separately,
    // and padding/reserved bytes from old persistence files are not identity.
    unsigned int hash = 2166136261u;
    InventoryRuntimeHashBytes(&hash, &item.slotDefinitionIndex,
        sizeof(item.slotDefinitionIndex));
    InventoryRuntimeHashBytes(&hash, &item.overrideDefinitionIndex,
        sizeof(item.overrideDefinitionIndex));
    InventoryRuntimeHashBytes(&hash, &item.paintKit, sizeof(item.paintKit));
    InventoryRuntimeHashBytes(&hash, &item.seed, sizeof(item.seed));
    InventoryRuntimeHashBytes(&hash, &item.statTrak, sizeof(item.statTrak));
    InventoryRuntimeHashBytes(&hash, &item.wear, sizeof(item.wear));
    InventoryRuntimeHashBytes(&hash, &item.quality, sizeof(item.quality));
    const int length = InventoryClampInt(item.customNameLength, 0,
        INVENTORY_CUSTOM_NAME_CAPACITY - 1);
    InventoryRuntimeHashBytes(&hash, &length, sizeof(length));
    InventoryRuntimeHashBytes(&hash, item.customName, length);
    return hash;
}

static bool InventoryRuntimeTryItemSignature(const VirtualInventoryItem& item,
    unsigned int* output)
{
    if (!output || !InventoryStickerTryLock())
        return false;
    if (!InventoryKeychainTryLock())
    {
        InventoryStickerUnlock();
        return false;
    }
    unsigned int hash = InventoryRuntimeAppearanceSignature(item);
    InventoryStickerRecord stickers{};
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        stickers.slots[i].scale = 1.0f;
        stickers.slots[i].schema = static_cast<BYTE>(i);
    }
    InventoryStickerRecord* source = InventoryStickerFindLocked(item.itemId, false);
    if (source)
        stickers = *source;
    InventoryKeychainState keychain{};
    InventoryKeychainRecord* chain = InventoryKeychainFindLocked(item.itemId, false);
    if (chain)
        keychain = chain->state;
    InventoryKeychainUnlock();
    InventoryStickerUnlock();
    // A busy sidecar is a retry, never a fictitious empty attachment set.
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        const InventoryStickerSlot& slot = stickers.slots[i];
        InventoryRuntimeHashBytes(&hash, &slot.id, sizeof(slot.id));
        InventoryRuntimeHashBytes(&hash, &slot.wear, sizeof(slot.wear));
        InventoryRuntimeHashBytes(&hash, &slot.scale, sizeof(slot.scale));
        InventoryRuntimeHashBytes(&hash, &slot.rotation, sizeof(slot.rotation));
        InventoryRuntimeHashBytes(&hash, &slot.offsetX, sizeof(slot.offsetX));
        InventoryRuntimeHashBytes(&hash, &slot.offsetY, sizeof(slot.offsetY));
        InventoryRuntimeHashBytes(&hash, &slot.schema, sizeof(slot.schema));
    }
    InventoryRuntimeHashBytes(&hash, &keychain, sizeof(keychain));
    *output = hash;
    return true;
}

static bool InventoryRuntimeValidateItem(const VirtualInventoryItem& item)
{
    if (!item.itemId)
        return false;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
    {
        for (int i = 0; i < InventoryMusicCatalogCount(); ++i)
            if (kInventoryMusicKits[i].musicKitId == item.overrideDefinitionIndex)
                return item.paintKit == 0;
        return false;
    }
    // Sandbox containers are persistent local records, not verified client
    // inventory definitions. They must never be fabricated in the SOCache.
    if (item.slotDefinitionIndex == INVENTORY_SLOT_CONTAINER)
        return false;
    return InventoryGameCatalogValidateItem(item);
}

static bool InventoryRuntimeReadEquipped(unsigned short slot, BYTE team,
    VirtualInventoryItem* output, bool* found)
{
    if (!output || !found || !InventoryTryLock())
        return false;
    *found = false;
    if (g_inventoryStore.enabled)
    {
        const int count = InventoryClampInt(g_inventoryStore.itemCount,
            0, INVENTORY_MAX_ITEMS);
        for (int i = 0; i < count; ++i)
        {
            const VirtualInventoryItem& item = g_inventoryStore.items[i];
            if (item.slotDefinitionIndex == slot && (item.equippedTeams & team))
            {
                *output = item;
                *found = true;
                break;
            }
        }
    }
    InventoryUnlock();
    if (*found)
    {
        output->equippedTeams = InventoryRuntimeEffectiveTeams(*output);
        if (!InventoryRuntimeValidateItem(*output) || !(output->equippedTeams & team))
            *found = false;
    }
    return true;
}

static void InventoryRuntimeCaptureView(BYTE* view, InventoryOriginalViewState* state)
{
    ZeroBytes(state, sizeof(*state));
    if (!view)
        return;
    if (g_inventoryRuntime.hasCustomName && IsAccessible(
        view + g_inventoryRuntime.customNameOffset, INVENTORY_CUSTOM_NAME_CAPACITY, false))
    {
        for (int i = 0; i < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
            state->customName[i] = *(view + g_inventoryRuntime.customNameOffset + i);
        state->hasName = true;
    }
    if (g_inventoryEconRuntime.viewReady && IsAccessible(
        view + g_inventoryEconRuntime.viewItemIdOffset, sizeof(unsigned long long), false) &&
        IsAccessible(view + g_inventoryEconRuntime.viewAccountIdOffset, sizeof(unsigned int), false) &&
        IsAccessible(view + g_inventoryEconRuntime.viewDisallowSocOffset, 1, false))
    {
        state->itemId = *reinterpret_cast<unsigned long long*>(
            view + g_inventoryEconRuntime.viewItemIdOffset);
        state->accountId = *reinterpret_cast<unsigned int*>(
            view + g_inventoryEconRuntime.viewAccountIdOffset);
        state->disallowSoc = *(view + g_inventoryEconRuntime.viewDisallowSocOffset);
        state->hasIdentity = true;
    }
    if (g_inventoryEconRuntime.viewReady && g_inventoryEconRuntime.viewRestoreMaterialOffset &&
        IsAccessible(view + g_inventoryEconRuntime.viewRestoreMaterialOffset, 1, false))
    {
        state->restoreMaterial = *(view + g_inventoryEconRuntime.viewRestoreMaterialOffset);
        state->hasRestoreMaterial = true;
    }
}

static void InventoryRuntimeRestoreView(BYTE* view, const InventoryOriginalViewState& state)
{
    if (!view)
        return;
    if (state.hasName && g_inventoryRuntime.hasCustomName && IsAccessible(
        view + g_inventoryRuntime.customNameOffset, INVENTORY_CUSTOM_NAME_CAPACITY, true))
        for (int i = 0; i < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
            *(view + g_inventoryRuntime.customNameOffset + i) = state.customName[i];
    if (state.hasIdentity && g_inventoryEconRuntime.viewReady)
    {
        if (IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset, sizeof(unsigned long long), true))
            *reinterpret_cast<unsigned long long*>(view + g_inventoryEconRuntime.viewItemIdOffset) = state.itemId;
        if (IsAccessible(view + g_inventoryEconRuntime.viewAccountIdOffset, sizeof(unsigned int), true))
            *reinterpret_cast<unsigned int*>(view + g_inventoryEconRuntime.viewAccountIdOffset) = state.accountId;
        if (IsAccessible(view + g_inventoryEconRuntime.viewDisallowSocOffset, 1, true))
            *(view + g_inventoryEconRuntime.viewDisallowSocOffset) = state.disallowSoc;
    }
    if (state.hasRestoreMaterial && g_inventoryEconRuntime.viewReady &&
        IsAccessible(view + g_inventoryEconRuntime.viewRestoreMaterialOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewRestoreMaterialOffset) = state.restoreMaterial;
}

static bool InventoryRuntimeViewNeedsBind(BYTE* view, unsigned long long itemId)
{
    InventoryEconMirror* mirror = InventoryEconFindMirror(itemId);
    if (!view || !mirror || !g_inventoryEconRuntime.viewReady ||
        !IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset,
            sizeof(unsigned long long), false))
        return false;
    return *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset) != mirror->econItemId;
}

static bool InventoryRuntimeHasNativeView(BYTE* view)
{
    if (!view || !g_inventoryEconRuntime.attributeReady || !g_inventoryEconRuntime.viewReady ||
        !IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset,
            sizeof(unsigned long long), false))
        return false;
    const unsigned long long id = *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset);
    for (int i = 0; i < g_inventoryEconMirrorCount; ++i)
        if (g_inventoryEconMirrors[i].active && g_inventoryEconMirrors[i].econItemId == id &&
            g_inventoryEconMirrors[i].inventory == g_inventoryEconLastInventory)
            return true;
    return false;
}

static void InventoryRuntimeRefreshRestoredWeapon(void* entity,
    const OriginalWeaponCosmetics& original, bool definitionChanged)
{
    // Restore the texture attributes written by our live-view fallback as well
    // as the entity's fallback fields. Otherwise a disabled item can keep the
    // previous custom material even after its ID has been restored.
    if (original.wroteViewAttributes)
        InventoryVisualApplyItemViewState(entity, original.paintKit,
            original.seed, original.wear);
    if (definitionChanged && g_inventoryVisualRefreshRuntime.subclassReady)
        g_inventoryVisualRefreshRuntime.updateSubclass(entity);
    if (g_inventoryVisualRefreshRuntime.compositeReady &&
        g_inventoryVisualRefreshRuntime.compositeOwnerOffsetReady)
    {
        BYTE* owner = reinterpret_cast<BYTE*>(entity) +
            g_inventoryVisualRefreshRuntime.compositeOwnerOffset;
        if (IsAccessible(owner, 0x20, false))
            g_inventoryVisualRefreshRuntime.updateCompositeMaterial(owner, 1);
    }
    if (g_inventoryVisualRefreshRuntime.compositeSetReady)
        g_inventoryVisualRefreshRuntime.updateCompositeMaterialSet(entity, 0);
    if (g_inventoryVisualRefreshRuntime.skinReady)
        g_inventoryVisualRefreshRuntime.updateSkin(entity, true);
}

struct InventoryOriginalLoadoutSlot {
    bool captured;
    bool owned;
    unsigned long long originalId;
    unsigned long long projectedId;
};

static InventoryOriginalLoadoutSlot g_inventoryOriginalLoadout[2][57]{};

static bool InventoryRuntimeReadLoadoutId(void* inventory, int team, int slot,
    unsigned long long* id)
{
    if (!inventory || !id || team < 2 || team > 3 || slot < 0 || slot >= 57 ||
        !g_inventoryEconRuntime.viewReady || !g_inventoryEconRuntime.equipReady ||
        !IsExecutable(g_inventoryEconRuntime.getItemInLoadout))
        return false;
    using Fn = void* (*)(void*, int, int);
    BYTE* view = reinterpret_cast<BYTE*>(reinterpret_cast<Fn>(
        g_inventoryEconRuntime.getItemInLoadout)(inventory, team, slot));
    if (!view || !IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset,
        sizeof(unsigned long long), false))
        return false;
    *id = *reinterpret_cast<unsigned long long*>(view + g_inventoryEconRuntime.viewItemIdOffset);
    return true;
}

static bool InventoryRuntimeRestoreLoadout(unsigned long long projectedId, void* inventory)
{
    bool restored = true;
    using EquipFn = bool (*)(void*, int, int, unsigned long long);
    for (int team = 2; team <= 3; ++team)
    {
        for (int slot = 0; slot < 57; ++slot)
        {
            InventoryOriginalLoadoutSlot& original = g_inventoryOriginalLoadout[team - 2][slot];
            if (!original.owned || original.projectedId != projectedId)
                continue;
            unsigned long long current = 0;
            if (!InventoryRuntimeReadLoadoutId(inventory, team, slot, &current))
            {
                restored = false;
                continue;
            }
            // Do not undo a user/game loadout change that no longer points at us.
            if (current == projectedId && (!IsExecutable(g_inventoryEconRuntime.equipItemInLoadout) ||
                !reinterpret_cast<EquipFn>(g_inventoryEconRuntime.equipItemInLoadout)(
                    g_inventoryEconRuntime.manager, team, slot, original.originalId)))
            {
                restored = false;
                continue;
            }
            ZeroBytes(&original, sizeof(original));
        }
    }
    return restored;
}

static void InventoryRuntimeEquipMirror(InventoryEconMirror* mirror,
    const VirtualInventoryItem& item)
{
    if (!mirror || !g_inventoryEconRuntime.equipReady || !g_inventoryEconRuntime.manager ||
        !mirror->inventory || !IsExecutable(g_inventoryEconRuntime.equipItemInLoadout))
        return;
    using EquipFn = bool (*)(void*, int, int, unsigned long long);
    EquipFn equip = reinterpret_cast<EquipFn>(g_inventoryEconRuntime.equipItemInLoadout);
    const BYTE desiredTeams = InventoryRuntimeEffectiveTeams(item);
    BYTE appliedTeams = mirror->teams;
    for (int team = 2; team <= 3; ++team)
    {
        const BYTE bit = team == 2 ? INVENTORY_TEAM_T : INVENTORY_TEAM_CT;
        if ((desiredTeams & bit) == (appliedTeams & bit))
            continue;
        if (!(desiredTeams & bit))
        {
            bool restored = true;
            for (int slot = 0; slot < 57; ++slot)
            {
                InventoryOriginalLoadoutSlot& original = g_inventoryOriginalLoadout[team - 2][slot];
                if (!original.owned || original.projectedId != mirror->econItemId)
                    continue;
                unsigned long long current = 0;
                if (!InventoryRuntimeReadLoadoutId(mirror->inventory, team, slot, &current) ||
                    (current == mirror->econItemId && !equip(g_inventoryEconRuntime.manager,
                        team, slot, original.originalId)))
                {
                    restored = false;
                    continue;
                }
                ZeroBytes(&original, sizeof(original));
            }
            if (restored)
                appliedTeams &= static_cast<BYTE>(~bit);
            continue;
        }
        const int slot = InventoryEconSlotForItem(mirror->inventory, item, team);
        unsigned long long current = 0;
        if (slot < 0 || slot >= 57 || !InventoryRuntimeReadLoadoutId(mirror->inventory,
            team, slot, &current))
            continue;
        InventoryOriginalLoadoutSlot& original = g_inventoryOriginalLoadout[team - 2][slot];
        if (!original.captured || (!original.owned || current != original.projectedId))
        {
            original.originalId = current;
            original.captured = true;
        }
        if (equip(g_inventoryEconRuntime.manager, team, slot, mirror->econItemId))
        {
            original.owned = true;
            original.projectedId = mirror->econItemId;
            appliedTeams |= bit;
        }
    }
    // Failed equip operations stay pending and retry on later frames.
    mirror->teams = appliedTeams;
}
