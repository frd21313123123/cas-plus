// Offline fixture only. These rows are never compiled into the shipped catalog.
static InventoryGameCatalogRecord g_selectorTestCatalog[20]{};

static void InventoryPrepareSelectorSnapshot(int scenario)
{
    ZeroBytes(g_selectorTestCatalog, sizeof(g_selectorTestCatalog));
    const int paints[] = { 0, 180, 302, 44, 282, 675, 600, 639, 490, 506, 656, 707,
        0, 344, 0, 309, 0, 12, 10006, 0 };
    const wchar_t* finishes[] = { L"", L"Fire Serpent", L"Vulcan", L"Case Hardened",
        L"\x041a\x0440\x0430\x0441\x043d\x0430\x044f \x043b\x0438\x043d\x0438\x044f", L"The Empress", L"Neon Revolution", L"Bloodsport",
        L"Frontside Misty", L"Point Disarray", L"Orbit Mk01", L"Neon Rider", L"", L"Dragon Lore",
        L"", L"Howl", L"", L"Crimson Web", L"Fixture glove finish", L"" };
    for (int i = 0; i < 20; ++i)
    {
        InventoryGameCatalogRecord& row = g_selectorTestCatalog[i];
        row.definitionIndex = i < 12 ? 7 : i < 14 ? 9 : i < 16 ? 16 : i < 18 ? 508 : i == 18 ? 5030 : 5105;
        row.category = i < 12 ? 2 : i < 14 ? 5 : i < 16 ? 2 : i < 18 ? 7 : i == 18 ? 8 : 9;
        row.teamMask = i < 12 ? INVENTORY_TEAM_T : i >= 14 && i < 16 ? INVENTORY_TEAM_CT : INVENTORY_TEAM_BOTH;
        row.paintKit = paints[i];
        CasSelectorCopy(row.weaponName, 48, i < 12 ? L"AK-47" : i < 14 ? L"AWP" : i < 16 ? L"M4A4" :
            i < 18 ? L"M9 Bayonet" : i == 18 ? L"Fixture gloves" : L"Fixture agent");
        CasSelectorCopy(row.finishName, 64, finishes[i]);
        CasSelectorCopy(row.displayName, 80, row.weaponName);
    }
    g_inventoryGameCatalog = scenario == 2 ? nullptr : g_selectorTestCatalog;
    g_inventoryGameCatalogCount = scenario == 2 ? 0 : 20;
    g_inventoryGameCatalogLoadAttempted = true;
    g_inventoryGameCatalogLastStatus = scenario == 2 ? INVENTORY_GAME_CATALOG_STATUS_OPEN : INVENTORY_GAME_CATALOG_STATUS_OK;
    g_inventoryStore = {};
    g_inventoryStore.enabled = true;
    g_inventoryStore.nextItemId = 0xC450000000000001ULL;
    AtomicExchange(&g_inventoryLock, 0);
    g_espConfig.selectedTab = 3;
    CasSelectorOpen(0);
    CasSelectorCopy(g_casSelectorWear, 16, L"0.0001");
    CasSelectorCopy(g_casSelectorSeed, 16, L"0");
    CasSelectorCopy(g_casSelectorCounter, 16, L"0");
    CasSelectorRebuildLists();
    if (scenario != 2)
    {
        CasSelectorChooseRow(false, 0);
        CasSelectorRebuildLists();
        CasSelectorChooseRow(true, 2);
        CasSelectorSetFocus(CAS_SEL_FINISHES);
    }
    if (scenario == 1)
    {
        CasSelectorCopy(g_casSelectorFinishSearch, 64, L"no matching finish");
        g_casSelectorListsDirty = true;
        CasSelectorRebuildLists();
    }
}

static int InventoryRunSelectorTests()
{
    int failures = 0;
    int assertions = 0;
#define INV_SELECTOR_EXPECT(condition) do { ++assertions; if (!(condition)) { \
    printf("Selector test failed at line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)
    InventoryGameCatalogRecord* savedCatalog = g_inventoryGameCatalog;
    const unsigned int savedCatalogCount = g_inventoryGameCatalogCount;
    const InventoryStore savedStore = g_inventoryStore;
    InventoryPrepareSelectorSnapshot(0);
    INV_SELECTOR_EXPECT(g_casSelectorModelCount == 3);
    INV_SELECTOR_EXPECT(g_casSelectorFinishCount == 12);
    INV_SELECTOR_EXPECT(g_casSelectorDefinition == 7);
    INV_SELECTOR_EXPECT(g_casSelectorPaint == 302);
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 0);
    INV_SELECTOR_EXPECT(CasSelectorValidation() == nullptr);
    g_casSelectorEditId = 42;
    g_casSelectorCategory = 0;
    INV_SELECTOR_EXPECT(CasSelectorClick(280, 82));
    INV_SELECTOR_EXPECT(g_casSelectorEditId == 42 && g_casSelectorCategory == 0);
    CasSelectorSetFocus(CAS_SEL_CATEGORY);
    INV_SELECTOR_EXPECT(CasSelectorHandleInput(0x0100, 0x27, 0));
    INV_SELECTOR_EXPECT(g_casSelectorEditId == 42 && g_casSelectorCategory == 0);
    g_casSelectorEditId = 0;
    CasSelectorSetFocus(CAS_SEL_FINISHES);
    INV_SELECTOR_EXPECT(CasSelectorEnabledState() == 1);
    INV_SELECTOR_EXPECT(CasSelectorToggleEnabled());
    INV_SELECTOR_EXPECT(CasSelectorEnabledState() == 0);
    AtomicExchange(&g_inventoryLock, 1);
    INV_SELECTOR_EXPECT(CasSelectorEnabledState() == -1);
    INV_SELECTOR_EXPECT(!CasSelectorToggleEnabled());
    AtomicExchange(&g_inventoryLock, 0);
    INV_SELECTOR_EXPECT(CasSelectorEnabledState() == 0);
    INV_SELECTOR_EXPECT(CasSelectorToggleEnabled() && CasSelectorEnabledState() == 1);
    INV_SELECTOR_EXPECT(InventoryGameCatalogAt(g_casSelectorFinishRows[0])->paintKit == 0);
    INV_SELECTOR_EXPECT(CasSelectorContains(L"VULCAN", L"vulcan"));
    INV_SELECTOR_EXPECT(CasSelectorContains(L"\x041a\x0420\x0410\x0421\x041d\x0410\x042f", L"\x043a\x0440\x0430\x0441\x043d\x0430\x044f"));
    int value = -1;
    float wear = -1.0f;
    INV_SELECTOR_EXPECT(CasSelectorParseNumber(L"1000", 1000, &value) && value == 1000);
    INV_SELECTOR_EXPECT(!CasSelectorParseNumber(L"1001", 1000, &value));
    INV_SELECTOR_EXPECT(!CasSelectorParseNumber(L"999999999999999", 1000, &value));
    INV_SELECTOR_EXPECT(!CasSelectorParseNumber(L"-1", 1000, &value));
    INV_SELECTOR_EXPECT(!CasSelectorParseNumber(L"", 1000, &value));
    INV_SELECTOR_EXPECT(CasSelectorParseWear(L"0,25", &wear) && wear == 0.25f);
    INV_SELECTOR_EXPECT(CasSelectorParseWear(L"1", &wear) && wear == 1.0f);
    INV_SELECTOR_EXPECT(!CasSelectorParseWear(L"1.01", &wear));
    INV_SELECTOR_EXPECT(!CasSelectorParseWear(L"nan", &wear));
    INV_SELECTOR_EXPECT(!CasSelectorParseWear(L"0.2.3", &wear));
    INV_SELECTOR_EXPECT(!CasSelectorParseWear(L".", &wear));
    // Disabled fields cannot invalidate a default finish or an agent draft.
    g_casSelectorPaint = 0;
    CasSelectorCopy(g_casSelectorWear, 16, L"invalid");
    CasSelectorCopy(g_casSelectorSeed, 16, L"invalid");
    CasSelectorRebuildLists();
    INV_SELECTOR_EXPECT(CasSelectorValidation() == nullptr);
    g_casSelectorPaint = 302;
    CasSelectorCopy(g_casSelectorWear, 16, L"0.0001");
    CasSelectorCopy(g_casSelectorSeed, 16, L"0");
    AtomicExchange(&g_inventoryLock, 1);
    INV_SELECTOR_EXPECT(!CasSelectorSubmit());
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 0 && g_casSelectorPaint == 302);
    AtomicExchange(&g_inventoryLock, 0);
    INV_SELECTOR_EXPECT(CasSelectorSubmit());
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 1);
    INV_SELECTOR_EXPECT(g_inventoryStore.items[0].paintKit == 302);
    INV_SELECTOR_EXPECT(g_inventoryStore.items[0].equippedTeams == INVENTORY_TEAM_NONE);
    INV_SELECTOR_EXPECT(g_casUiSkinchangerSubtab == 3 && g_casUiInventoryView == 3);
    INV_SELECTOR_EXPECT(g_inventoryUiSelected == 0);
    InventoryUiSetEquipMask(INVENTORY_TEAM_T);
    INV_SELECTOR_EXPECT(g_inventoryStore.items[0].equippedTeams == INVENTORY_TEAM_T);
    InventoryUiSetEquipMask(INVENTORY_TEAM_NONE);
    INV_SELECTOR_EXPECT(g_inventoryStore.items[0].equippedTeams == INVENTORY_TEAM_NONE);
    INV_SELECTOR_EXPECT(!CasSelectorSubmit() && g_inventoryStore.itemCount == 1);
    InventoryPrepareSelectorSnapshot(1);
    INV_SELECTOR_EXPECT(g_casSelectorFinishCount == 0);
    INV_SELECTOR_EXPECT(g_casSelectorPaint == 302);
    INV_SELECTOR_EXPECT(!CasSelectorSubmit() && g_inventoryStore.itemCount == 0);
    g_casSelectorFinishSearch[0] = 0; g_casSelectorListsDirty = true; CasSelectorRebuildLists();
    INV_SELECTOR_EXPECT(CasSelectorSelectedRow(true) == 2);
    CasSelectorChooseRow(false, 1); CasSelectorRebuildLists();
    INV_SELECTOR_EXPECT(g_casSelectorDefinition == 9 && g_casSelectorPaint == -1);
    INV_SELECTOR_EXPECT(g_casSelectorFinishCount == 2);
    CasSelectorChooseRow(true, 1);
    INV_SELECTOR_EXPECT(g_casSelectorPaint == 344 && g_inventoryStore.itemCount == 0);
    g_casSelectorPaint = 302;
    INV_SELECTOR_EXPECT(!CasSelectorSubmit());
    CasSelectorOpen(2); CasSelectorRebuildLists();
    CasSelectorChooseRow(false, 0); CasSelectorRebuildLists();
    INV_SELECTOR_EXPECT(g_casSelectorFinishCount == 1);
    INV_SELECTOR_EXPECT(InventoryGameCatalogAt(g_casSelectorFinishRows[0])->paintKit != 0);
    CasSelectorChooseRow(true, 0);
    INV_SELECTOR_EXPECT(CasSelectorSubmit());
    INV_SELECTOR_EXPECT(g_inventoryStore.items[0].slotDefinitionIndex == INVENTORY_SLOT_GLOVE);
    InventoryPrepareSelectorSnapshot(0);
    g_inventoryStore.itemCount = INVENTORY_MAX_ITEMS;
    INV_SELECTOR_EXPECT(!CasSelectorSubmit() && g_inventoryStore.itemCount == INVENTORY_MAX_ITEMS);
    InventoryPrepareSelectorSnapshot(0);
    CasSelectorSetFocus(CAS_SEL_SUBMIT);
    CasSelectorHandleInput(0x0100, 13, static_cast<LPARAM>(1) << 30);
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 0);
    CasSelectorHandleInput(0x0100, 13, 0);
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 1);
    InventoryPrepareSelectorSnapshot(2);
    INV_SELECTOR_EXPECT(g_casSelectorModelCount == 0 && g_casSelectorFinishCount == 0);
    INV_SELECTOR_EXPECT(!CasSelectorSubmit());
    INV_SELECTOR_EXPECT(g_inventoryStore.itemCount == 0);
    g_inventoryGameCatalog = savedCatalog;
    g_inventoryGameCatalogCount = savedCatalogCount;
    g_inventoryStore = savedStore;
    g_casSelectorListsDirty = true;
#undef INV_SELECTOR_EXPECT
    printf("Selector: %d assertions, %d failures\n", assertions, failures);
    return failures;
}
