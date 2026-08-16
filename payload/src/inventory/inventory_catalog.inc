// cas+ inventory catalog helpers.
// Injected after inventory_changer.inc, so it can reuse the virtual store,
// locking helpers and InventoryUi* mutation primitives without adding another
// runtime hook.

enum InventoryWeaponCategory : BYTE {
    INVENTORY_CATEGORY_PISTOL = 1,
    INVENTORY_CATEGORY_SMG = 2,
    INVENTORY_CATEGORY_RIFLE = 3,
    INVENTORY_CATEGORY_SNIPER = 4,
    INVENTORY_CATEGORY_SHOTGUN = 5,
    INVENTORY_CATEGORY_HEAVY = 6,
    INVENTORY_CATEGORY_KNIFE = 7,
    INVENTORY_CATEGORY_SPECIAL = 8
};

struct InventoryWeaponCatalogEntry {
    unsigned short definitionIndex;
    BYTE category;
    BYTE teamMask;
    const wchar_t* displayName;
    const char* internalName;
};

// Definition indexes are intentionally kept in one read-only table. The game
// adapter does not depend on this table for memory layout or ownership checks;
// catalog mistakes therefore cannot turn into arbitrary writes.
static const InventoryWeaponCatalogEntry kInventoryWeapons[] = {
    { 1,  INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_BOTH, L"Desert Eagle",  "weapon_deagle" },
    { 2,  INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_BOTH, L"Dual Berettas", "weapon_elite" },
    { 3,  INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_CT,   L"Five-SeveN",    "weapon_fiveseven" },
    { 4,  INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_T,    L"Glock-18",      "weapon_glock" },
    { 7,  INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_T,    L"AK-47",         "weapon_ak47" },
    { 8,  INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_CT,   L"AUG",           "weapon_aug" },
    { 9,  INVENTORY_CATEGORY_SNIPER,  INVENTORY_TEAM_BOTH, L"AWP",           "weapon_awp" },
    { 10, INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_CT,   L"FAMAS",         "weapon_famas" },
    { 11, INVENTORY_CATEGORY_SNIPER,  INVENTORY_TEAM_T,    L"G3SG1",         "weapon_g3sg1" },
    { 13, INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_T,    L"Galil AR",      "weapon_galilar" },
    { 14, INVENTORY_CATEGORY_HEAVY,   INVENTORY_TEAM_BOTH, L"M249",          "weapon_m249" },
    { 16, INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_CT,   L"M4A4",          "weapon_m4a1" },
    { 17, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_T,    L"MAC-10",        "weapon_mac10" },
    { 19, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_BOTH, L"P90",           "weapon_p90" },
    { 23, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_BOTH, L"MP5-SD",        "weapon_mp5sd" },
    { 24, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_BOTH, L"UMP-45",        "weapon_ump45" },
    { 25, INVENTORY_CATEGORY_SHOTGUN, INVENTORY_TEAM_BOTH, L"XM1014",        "weapon_xm1014" },
    { 26, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_BOTH, L"PP-Bizon",      "weapon_bizon" },
    { 27, INVENTORY_CATEGORY_SHOTGUN, INVENTORY_TEAM_CT,   L"MAG-7",         "weapon_mag7" },
    { 28, INVENTORY_CATEGORY_HEAVY,   INVENTORY_TEAM_BOTH, L"Negev",         "weapon_negev" },
    { 29, INVENTORY_CATEGORY_SHOTGUN, INVENTORY_TEAM_T,    L"Sawed-Off",     "weapon_sawedoff" },
    { 30, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_T,    L"Tec-9",         "weapon_tec9" },
    { 31, INVENTORY_CATEGORY_SPECIAL, INVENTORY_TEAM_BOTH, L"Zeus x27",      "weapon_taser" },
    { 32, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_CT,   L"P2000",         "weapon_hkp2000" },
    { 33, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_BOTH, L"MP7",           "weapon_mp7" },
    { 34, INVENTORY_CATEGORY_SMG,     INVENTORY_TEAM_CT,   L"MP9",           "weapon_mp9" },
    { 35, INVENTORY_CATEGORY_SHOTGUN, INVENTORY_TEAM_BOTH, L"Nova",          "weapon_nova" },
    { 36, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_BOTH, L"P250",          "weapon_p250" },
    { 38, INVENTORY_CATEGORY_SNIPER,  INVENTORY_TEAM_CT,   L"SCAR-20",       "weapon_scar20" },
    { 39, INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_T,    L"SG 553",        "weapon_sg556" },
    { 40, INVENTORY_CATEGORY_SNIPER,  INVENTORY_TEAM_BOTH, L"SSG 08",        "weapon_ssg08" },
    { 42, INVENTORY_CATEGORY_KNIFE,   INVENTORY_TEAM_T,    L"Default T Knife","weapon_knife_t" },
    { 59, INVENTORY_CATEGORY_KNIFE,   INVENTORY_TEAM_CT,   L"Default CT Knife","weapon_knife" },
    { 60, INVENTORY_CATEGORY_RIFLE,   INVENTORY_TEAM_CT,   L"M4A1-S",        "weapon_m4a1_silencer" },
    { 61, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_CT,   L"USP-S",         "weapon_usp_silencer" },
    { 63, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_BOTH, L"CZ75-Auto",     "weapon_cz75a" },
    { 64, INVENTORY_CATEGORY_PISTOL,  INVENTORY_TEAM_BOTH, L"R8 Revolver",   "weapon_revolver" },

    { 500, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Bayonet",          "weapon_bayonet" },
    { 503, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Classic Knife",    "weapon_knife_css" },
    { 505, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Flip Knife",       "weapon_knife_flip" },
    { 506, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Gut Knife",        "weapon_knife_gut" },
    { 507, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Karambit",         "weapon_knife_karambit" },
    { 508, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"M9 Bayonet",       "weapon_knife_m9_bayonet" },
    { 509, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Huntsman Knife",   "weapon_knife_tactical" },
    { 512, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Falchion Knife",   "weapon_knife_falchion" },
    { 514, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Bowie Knife",      "weapon_knife_survival_bowie" },
    { 515, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Butterfly Knife",  "weapon_knife_butterfly" },
    { 516, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Shadow Daggers",   "weapon_knife_push" },
    { 517, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Paracord Knife",   "weapon_knife_cord" },
    { 518, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Survival Knife",   "weapon_knife_canis" },
    { 519, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Ursus Knife",      "weapon_knife_ursus" },
    { 520, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Navaja Knife",     "weapon_knife_gypsy_jackknife" },
    { 521, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Nomad Knife",      "weapon_knife_outdoor" },
    { 522, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Stiletto Knife",   "weapon_knife_stiletto" },
    { 523, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Talon Knife",      "weapon_knife_widowmaker" },
    { 525, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Skeleton Knife",   "weapon_knife_skeleton" },
    { 526, INVENTORY_CATEGORY_KNIFE, INVENTORY_TEAM_BOTH, L"Kukri Knife",      "weapon_knife_kukri" }
};

struct InventoryPaintCatalogEntry {
    int paintKit;
    unsigned short slotDefinitionIndex;
    const wchar_t* displayName;
};

// Baseline catalog intentionally contains only finishes whose paint-kit IDs are
// broadly reused by knife families. A refresh generator can replace/extend this
// table from the current items_game.txt without touching the runtime adapter.
static const InventoryPaintCatalogEntry kInventoryPaints[] = {
    { 0,   INVENTORY_SLOT_KNIFE, L"Vanilla" },
    { 12,  INVENTORY_SLOT_KNIFE, L"Crimson Web" },
    { 38,  INVENTORY_SLOT_KNIFE, L"Fade" },
    { 40,  INVENTORY_SLOT_KNIFE, L"Night" },
    { 42,  INVENTORY_SLOT_KNIFE, L"Blue Steel" },
    { 43,  INVENTORY_SLOT_KNIFE, L"Stained" },
    { 44,  INVENTORY_SLOT_KNIFE, L"Case Hardened" },
    { 59,  INVENTORY_SLOT_KNIFE, L"Slaughter" },
    { 72,  INVENTORY_SLOT_KNIFE, L"Safari Mesh" },
    { 77,  INVENTORY_SLOT_KNIFE, L"Forest DDPAT" },
    { 98,  INVENTORY_SLOT_KNIFE, L"Ultraviolet" },
    { 143, INVENTORY_SLOT_KNIFE, L"Urban Masked" },
    { 175, INVENTORY_SLOT_KNIFE, L"Scorched" },
    { 409, INVENTORY_SLOT_KNIFE, L"Tiger Tooth" },
    { 413, INVENTORY_SLOT_KNIFE, L"Marble Fade" },
    { 414, INVENTORY_SLOT_KNIFE, L"Rust Coat" },
    { 415, INVENTORY_SLOT_KNIFE, L"Doppler" },
    { 568, INVENTORY_SLOT_KNIFE, L"Gamma Doppler" }
};

static int InventoryCatalogWeaponCount()
{
    return static_cast<int>(sizeof(kInventoryWeapons) /
        sizeof(kInventoryWeapons[0]));
}

static int InventoryCatalogPaintCount()
{
    return static_cast<int>(sizeof(kInventoryPaints) /
        sizeof(kInventoryPaints[0]));
}

static const InventoryWeaponCatalogEntry* InventoryCatalogFindWeapon(
    unsigned short definitionIndex)
{
    for (int i = 0; i < InventoryCatalogWeaponCount(); ++i)
        if (kInventoryWeapons[i].definitionIndex == definitionIndex)
            return &kInventoryWeapons[i];
    return nullptr;
}

static const wchar_t* InventoryCatalogWeaponName(
    unsigned short definitionIndex)
{
    const InventoryWeaponCatalogEntry* entry =
        InventoryCatalogFindWeapon(definitionIndex);
    return entry ? entry->displayName : L"Unknown item";
}

static const InventoryPaintCatalogEntry* InventoryCatalogFindPaint(
    int paintKit, unsigned short slotDefinitionIndex)
{
    for (int i = 0; i < InventoryCatalogPaintCount(); ++i)
    {
        const InventoryPaintCatalogEntry& entry = kInventoryPaints[i];
        if (entry.paintKit == paintKit &&
            entry.slotDefinitionIndex == slotDefinitionIndex)
            return &entry;
    }
    return nullptr;
}

static const wchar_t* InventoryCatalogPaintName(
    int paintKit, unsigned short slotDefinitionIndex)
{
    const InventoryPaintCatalogEntry* entry =
        InventoryCatalogFindPaint(paintKit, slotDefinitionIndex);
    return entry ? entry->displayName : L"Raw paint ID";
}

static int InventoryCatalogNextPaint(
    unsigned short slotDefinitionIndex, int currentPaint, int direction)
{
    if (direction == 0)
        return currentPaint;
    const int count = InventoryCatalogPaintCount();
    int current = -1;
    for (int i = 0; i < count; ++i)
    {
        if (kInventoryPaints[i].slotDefinitionIndex == slotDefinitionIndex &&
            kInventoryPaints[i].paintKit == currentPaint)
        {
            current = i;
            break;
        }
    }

    for (int step = 1; step <= count; ++step)
    {
        int index = current;
        if (current < 0)
            index = direction > 0 ? step - 1 : count - step;
        else
        {
            index = current + (direction > 0 ? step : -step);
            while (index < 0) index += count;
            while (index >= count) index -= count;
        }
        if (kInventoryPaints[index].slotDefinitionIndex == slotDefinitionIndex)
            return kInventoryPaints[index].paintKit;
    }
    return currentPaint;
}

static unsigned short InventoryCatalogNextKnife(
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
        int index = current;
        if (current < 0)
            index = direction > 0 ? step - 1 : count - step;
        else
        {
            index = current + (direction > 0 ? step : -step);
            while (index < 0) index += count;
            while (index >= count) index -= count;
        }
        if (kInventoryWeapons[index].category == INVENTORY_CATEGORY_KNIFE &&
            kInventoryWeapons[index].definitionIndex >= 500)
            return kInventoryWeapons[index].definitionIndex;
    }
    return currentDefinition;
}

static void InventoryUiCycleKnifeCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
    {
        item->overrideDefinitionIndex = InventoryCatalogNextKnife(
            item->overrideDefinitionIndex, direction);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}

static void InventoryUiCyclePaintCatalog(int direction)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        const int next = InventoryCatalogNextPaint(
            item->slotDefinitionIndex, item->paintKit, direction);
        if (next != item->paintKit)
        {
            item->paintKit = next;
            MarkInventoryDirty();
        }
    }
    InventoryUnlock();
}

static void InventoryUiDuplicateSelected()
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* selected = InventorySelectedLocked();
    if (selected && g_inventoryStore.itemCount < INVENTORY_MAX_ITEMS)
    {
        VirtualInventoryItem copy = *selected;
        copy.itemId = g_inventoryStore.nextItemId++;
        // Duplicates start unequipped so they never silently evict the source
        // from its current T/CT loadout slot.
        copy.equippedTeams = INVENTORY_TEAM_NONE;
        const int newIndex = g_inventoryStore.itemCount++;
        g_inventoryStore.items[newIndex] = copy;
        g_inventoryUiSelected = newIndex;
        g_inventoryUiPage = newIndex / 7;
        MarkInventoryDirty();
    }
    InventoryUnlock();
}
