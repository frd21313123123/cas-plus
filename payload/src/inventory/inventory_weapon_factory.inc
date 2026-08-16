// cas+ catalog-driven virtual weapon creation.
// This layer only mutates InventoryStore. The existing schema-backed weapon
// adapter remains the only code that projects weapon state into CS2 entities.

static bool InventoryCatalogIsSelectableWeapon(
    const InventoryWeaponCatalogEntry& entry)
{
    return entry.category != INVENTORY_CATEGORY_KNIFE &&
        entry.category != INVENTORY_CATEGORY_SPECIAL &&
        entry.definitionIndex > 0 && entry.definitionIndex <= 4095;
}

static unsigned short InventoryCatalogNextWeapon(
    unsigned short currentDefinition, int direction)
{
    if (direction == 0)
        return currentDefinition;
    const int count = InventoryCatalogWeaponCount();
    int current = -1;
    for (int i = 0; i < count; ++i)
        if (kInventoryWeapons[i].definitionIndex == currentDefinition)
        {
            current = i;
            break;
        }

    for (int step = 1; step <= count; ++step)
    {
        int index;
        if (current < 0)
            index = direction > 0 ? step - 1 : count - step;
        else
        {
            index = current + (direction > 0 ? step : -step);
            while (index < 0) index += count;
            while (index >= count) index -= count;
        }
        if (InventoryCatalogIsSelectableWeapon(kInventoryWeapons[index]))
            return kInventoryWeapons[index].definitionIndex;
    }
    return currentDefinition;
}

static BYTE InventoryDefaultEquipForCatalogWeapon(
    const InventoryWeaponCatalogEntry& entry)
{
    const BYTE currentTeam = InventoryTeamBit(g_inventoryActiveTeam);
    if (currentTeam != INVENTORY_TEAM_NONE &&
        (entry.teamMask & currentTeam) != 0)
        return currentTeam;
    if (entry.teamMask == INVENTORY_TEAM_T ||
        entry.teamMask == INVENTORY_TEAM_CT)
        return entry.teamMask;
    return currentTeam != INVENTORY_TEAM_NONE ?
        currentTeam : INVENTORY_TEAM_BOTH;
}

static void InventoryUiAddWeaponPreset()
{
    if (!InventoryTryLock())
        return;
    if (g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        unsigned short definition = g_inventoryActiveTeam == 3 ? 16 : 7;
        const InventoryWeaponCatalogEntry* entry =
            InventoryCatalogFindWeapon(definition);
        if (!entry || !InventoryCatalogIsSelectableWeapon(*entry))
        {
            for (int i = 0; i < InventoryCatalogWeaponCount(); ++i)
                if (InventoryCatalogIsSelectableWeapon(kInventoryWeapons[i]))
                {
                    entry = &kInventoryWeapons[i];
                    definition = entry->definitionIndex;
                    break;
                }
        }

        if (entry)
        {
            VirtualInventoryItem item;
            ZeroBytes(&item, sizeof(item));
            item.itemId = g_inventoryStore.nextItemId++;
            item.slotDefinitionIndex = definition;
            item.overrideDefinitionIndex = definition;
            item.paintKit = 0;
            item.seed = 0;
            item.statTrak = -1;
            item.wear = 0.0001f;
            item.quality = 0;
            item.equippedTeams = InventoryDefaultEquipForCatalogWeapon(*entry);

            const int index = g_inventoryStore.itemCount++;
            g_inventoryStore.items[index] = item;
            InventoryEquipItemLocked(index, item.equippedTeams);
            g_inventoryUiSelected = index;
            g_inventoryUiPage = index / 7;
            MarkInventoryDirty();
        }
    }
    InventoryUnlock();
}

static void InventoryUiCycleWeaponCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex <= 4095)
    {
        const InventoryWeaponCatalogEntry* current =
            InventoryCatalogFindWeapon(item->overrideDefinitionIndex);
        if (current && InventoryCatalogIsSelectableWeapon(*current))
        {
            const unsigned short nextDefinition =
                InventoryCatalogNextWeapon(item->overrideDefinitionIndex,
                    direction);
            const InventoryWeaponCatalogEntry* next =
                InventoryCatalogFindWeapon(nextDefinition);
            if (next && nextDefinition != item->overrideDefinitionIndex)
            {
                const int index = static_cast<int>(
                    item - g_inventoryStore.items);
                item->slotDefinitionIndex = nextDefinition;
                item->overrideDefinitionIndex = nextDefinition;
                // Paint compatibility changes with the weapon definition.
                // Reset to vanilla instead of carrying a potentially invalid
                // finish onto the new slot.
                item->paintKit = 0;
                item->seed = 0;
                item->equippedTeams &= next->teamMask;
                if (item->equippedTeams == INVENTORY_TEAM_NONE)
                    item->equippedTeams =
                        InventoryDefaultEquipForCatalogWeapon(*next);
                InventoryEquipItemLocked(index, item->equippedTeams);
                MarkInventoryDirty();
            }
        }
    }
    InventoryUnlock();
}
