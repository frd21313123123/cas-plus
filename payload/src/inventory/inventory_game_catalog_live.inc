// Live weapon projection for records that were validated against the installed
// game catalog.  This runs immediately after CEconItem view binding so rebinding
// an item ID cannot leave the material system with a vanilla/empty attribute set
// until a later refresh retry.

static bool InventoryGameCatalogSetLiveFloat(BYTE* view,
    const char* attributeName, float value)
{
    if (!view || !attributeName || !*attributeName)
        return false;
    if (g_inventoryEconRuntime.setViewAttributeByName &&
        IsExecutable(g_inventoryEconRuntime.setViewAttributeByName))
        return InventoryEconSetViewFloat(view, attributeName, value);
    if (g_inventoryVisualRefreshRuntime.setViewAttributeByName &&
        IsExecutable(reinterpret_cast<void*>(
            g_inventoryVisualRefreshRuntime.setViewAttributeByName)))
    {
        g_inventoryVisualRefreshRuntime.setViewAttributeByName(
            view, attributeName, value);
        return true;
    }
    return false;
}

static bool InventoryGameCatalogApplyLiveView(BYTE* itemView,
    const VirtualInventoryItem& item)
{
    if (!itemView || item.paintKit <= 0)
        return itemView != nullptr;
    // This exact (definition, paint) pair came from alternate_icons2 in the
    // installed items_game. Reject an old/raw impossible pair instead of asking
    // the material system to build a skin that does not exist for this weapon.
    if (!InventoryGameCatalogFindPair(item.overrideDefinitionIndex,
        item.paintKit))
        return false;

    bool applied = true;
    applied = InventoryGameCatalogSetLiveFloat(itemView,
        "set item texture prefab", static_cast<float>(item.paintKit)) && applied;
    applied = InventoryGameCatalogSetLiveFloat(itemView,
        "set item texture seed",
        static_cast<float>(InventoryClampInt(item.seed, 0, 1000))) && applied;
    applied = InventoryGameCatalogSetLiveFloat(itemView,
        "set item texture wear", InventoryClampWear(item.wear)) && applied;

    if (item.statTrak >= 0)
    {
        InventoryGameCatalogSetLiveFloat(itemView, "kill eater",
            static_cast<float>(InventoryClampInt(item.statTrak, 0, 999999)));
        InventoryGameCatalogSetLiveFloat(itemView,
            "kill eater score type", 0.0f);
    }

    if (g_inventoryEconRuntime.viewReady)
    {
        if (IsAccessible(itemView +
            g_inventoryEconRuntime.viewInitializedOffset, 1, true))
            *(itemView + g_inventoryEconRuntime.viewInitializedOffset) = 1;
        if (IsAccessible(itemView +
            g_inventoryEconRuntime.viewDisallowSocOffset, 1, true))
            *(itemView + g_inventoryEconRuntime.viewDisallowSocOffset) = 0;
        if (g_inventoryEconRuntime.viewRestoreMaterialOffset &&
            IsAccessible(itemView +
                g_inventoryEconRuntime.viewRestoreMaterialOffset, 1, true))
            *(itemView + g_inventoryEconRuntime.viewRestoreMaterialOffset) = 1;
    }
    return applied;
}
