// Included after the actual generated payload by the offline CRT test harness.
// All writes target test-owned memory; no client modules/game calls are used.
static int InventoryRunRuntimePolicyTests()
{
    int failures = 0;
#define INV_RUNTIME_EXPECT(condition) do { if (!(condition)) { \
    printf("Runtime test failed at line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

    VirtualInventoryItem first{};
    first.itemId = 0xC450000000000091ULL;
    first.slotDefinitionIndex = 7;
    first.overrideDefinitionIndex = 7;
    first.paintKit = 180;
    first.wear = 0.12f;
    first.seed = 17;
    first.statTrak = -1;
    first.customNameLength = 3;
    first.customName[0] = 't';
    first.customName[1] = 'e';
    first.customName[2] = 's';
    const unsigned int appearance = InventoryRuntimeAppearanceSignature(first);
    VirtualInventoryItem copy = first;
    copy.equippedTeams = INVENTORY_TEAM_BOTH;
    copy.itemId += 4;
    copy.reserved = 255;
    copy.customName[15] = 'x';
    INV_RUNTIME_EXPECT(appearance == InventoryRuntimeAppearanceSignature(copy));
    copy.paintKit += 1;
    INV_RUNTIME_EXPECT(appearance != InventoryRuntimeAppearanceSignature(copy));
    copy = first;
    copy.customName[0] = 'n';
    INV_RUNTIME_EXPECT(appearance != InventoryRuntimeAppearanceSignature(copy));

    const int stickerCount = g_inventoryStickerRecordCount;
    const int keychainCount = g_inventoryKeychainRecordCount;
    g_inventoryStickerRecordCount = 0;
    g_inventoryKeychainRecordCount = 0;
    unsigned int full = 0;
    INV_RUNTIME_EXPECT(InventoryRuntimeTryItemSignature(first, &full));
    AtomicExchange(&g_inventoryStickerLock, 1);
    unsigned int untouched = 0x12345678u;
    INV_RUNTIME_EXPECT(!InventoryRuntimeTryItemSignature(first, &untouched));
    INV_RUNTIME_EXPECT(untouched == 0x12345678u);
    AtomicExchange(&g_inventoryStickerLock, 0);
    AtomicExchange(&g_inventoryKeychainLock, 1);
    INV_RUNTIME_EXPECT(!InventoryRuntimeTryItemSignature(first, &untouched));
    INV_RUNTIME_EXPECT(g_inventoryStickerLock == 0);
    AtomicExchange(&g_inventoryKeychainLock, 0);
    const InventoryStickerRecord savedSticker = g_inventoryStickerRecords[0];
    ZeroBytes(&g_inventoryStickerRecords[0], sizeof(InventoryStickerRecord));
    g_inventoryStickerRecordCount = 1;
    g_inventoryStickerRecords[0].itemId = first.itemId;
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        g_inventoryStickerRecords[0].slots[i].scale = 1.0f;
        g_inventoryStickerRecords[0].slots[i].schema = static_cast<BYTE>(i);
    }
    unsigned int emptyRecord = 0;
    INV_RUNTIME_EXPECT(InventoryRuntimeTryItemSignature(first, &emptyRecord));
    INV_RUNTIME_EXPECT(full == emptyRecord);
    g_inventoryStickerRecords[0].slots[0].id = 42;
    unsigned int withSticker = 0;
    INV_RUNTIME_EXPECT(InventoryRuntimeTryItemSignature(first, &withSticker));
    INV_RUNTIME_EXPECT(full != withSticker);
    g_inventoryStickerRecords[0] = savedSticker;
    g_inventoryStickerRecordCount = 0;
    const InventoryKeychainRecord savedKeychain = g_inventoryKeychainRecords[0];
    g_inventoryKeychainRecordCount = 1;
    g_inventoryKeychainRecords[0] = {};
    g_inventoryKeychainRecords[0].itemId = first.itemId;
    g_inventoryKeychainRecords[0].state.id = 27;
    unsigned int withKeychain = 0;
    INV_RUNTIME_EXPECT(InventoryRuntimeTryItemSignature(first, &withKeychain));
    INV_RUNTIME_EXPECT(full != withKeychain);
    g_inventoryKeychainRecords[0] = savedKeychain;
    g_inventoryStickerRecordCount = stickerCount;
    g_inventoryKeychainRecordCount = keychainCount;

    const InventoryStore savedStore = g_inventoryStore;
    g_inventoryStore = {};
    g_inventoryStore.enabled = true;
    g_inventoryStore.itemCount = 1;
    g_inventoryStore.items[0].itemId = 11;
    g_inventoryStore.items[0].slotDefinitionIndex = INVENTORY_SLOT_MUSIC;
    g_inventoryStore.items[0].overrideDefinitionIndex = 1;
    g_inventoryStore.items[0].equippedTeams = INVENTORY_TEAM_BOTH;
    VirtualInventoryItem selected{};
    selected.itemId = 777;
    bool found = true;
    AtomicExchange(&g_inventoryLock, 1);
    INV_RUNTIME_EXPECT(!InventoryRuntimeReadEquipped(INVENTORY_SLOT_MUSIC,
        INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(found && selected.itemId == 777);
    AtomicExchange(&g_inventoryLock, 0);
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(INVENTORY_SLOT_MUSIC,
        INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(found && selected.itemId == 11);
    g_inventoryStore.items[0].overrideDefinitionIndex = 65000;
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(INVENTORY_SLOT_MUSIC,
        INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(!found && g_inventoryStore.items[0].overrideDefinitionIndex == 65000);
    g_inventoryStore.enabled = false;
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(INVENTORY_SLOT_MUSIC,
        INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(!found);
    g_inventoryStore = savedStore;

    // Legacy BOTH items are projected only to catalog-allowed teams without
    // rewriting persistent intent or rejecting their supported team.
    InventoryGameCatalogRecord teamRecords[2]{};
    teamRecords[0].definitionIndex = 7;
    teamRecords[0].category = 1;
    teamRecords[0].teamMask = INVENTORY_TEAM_T;
    teamRecords[1].definitionIndex = 5200;
    teamRecords[1].category = 9;
    teamRecords[1].teamMask = INVENTORY_TEAM_T;
    InventoryGameCatalogRecord* savedCatalog = g_inventoryGameCatalog;
    const unsigned int savedCatalogCount = g_inventoryGameCatalogCount;
    g_inventoryGameCatalog = teamRecords;
    g_inventoryGameCatalogCount = 2;
    VirtualInventoryItem teamItem{};
    teamItem.itemId = 92;
    teamItem.slotDefinitionIndex = 7;
    teamItem.overrideDefinitionIndex = 7;
    teamItem.equippedTeams = INVENTORY_TEAM_BOTH;
    INV_RUNTIME_EXPECT(InventoryRuntimeEffectiveTeams(teamItem) == INVENTORY_TEAM_T);
    g_inventoryStore = {};
    g_inventoryStore.enabled = true;
    g_inventoryStore.itemCount = 1;
    g_inventoryStore.items[0] = teamItem;
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(7, INVENTORY_TEAM_CT, &selected, &found));
    INV_RUNTIME_EXPECT(!found && g_inventoryStore.items[0].equippedTeams == INVENTORY_TEAM_BOTH);
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(7, INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(found && selected.equippedTeams == INVENTORY_TEAM_T);
    teamItem.slotDefinitionIndex = INVENTORY_SLOT_AGENT;
    teamItem.overrideDefinitionIndex = 5200;
    g_inventoryStore.items[0] = teamItem;
    INV_RUNTIME_EXPECT(InventoryRuntimeEffectiveTeams(teamItem) == INVENTORY_TEAM_T);
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(INVENTORY_SLOT_AGENT,
        INVENTORY_TEAM_CT, &selected, &found));
    INV_RUNTIME_EXPECT(!found && g_inventoryStore.items[0].equippedTeams == INVENTORY_TEAM_BOTH);
    INV_RUNTIME_EXPECT(InventoryRuntimeReadEquipped(INVENTORY_SLOT_AGENT,
        INVENTORY_TEAM_T, &selected, &found));
    INV_RUNTIME_EXPECT(found && selected.equippedTeams == INVENTORY_TEAM_T);
    g_inventoryStore = savedStore;
    g_inventoryGameCatalog = savedCatalog;
    g_inventoryGameCatalogCount = savedCatalogCount;

    const InventoryRuntime savedRuntime = g_inventoryRuntime;
    const InventoryEconRuntime savedEcon = g_inventoryEconRuntime;
    BYTE view[128]{};
    g_inventoryRuntime.hasCustomName = true;
    g_inventoryRuntime.customNameOffset = 64;
    g_inventoryEconRuntime.viewReady = true;
    g_inventoryEconRuntime.viewItemIdOffset = 8;
    g_inventoryEconRuntime.viewAccountIdOffset = 16;
    g_inventoryEconRuntime.viewDisallowSocOffset = 20;
    g_inventoryEconRuntime.viewRestoreMaterialOffset = 21;
    *reinterpret_cast<unsigned long long*>(view + 8) = 0xABCDEF1234567890ULL;
    *reinterpret_cast<unsigned int*>(view + 16) = 12345;
    view[20] = 1;
    view[21] = 0;
    view[64] = 'n'; view[65] = 'a'; view[66] = 'm'; view[67] = 'e';
    InventoryOriginalViewState original{};
    InventoryRuntimeCaptureView(view, &original);
    INV_RUNTIME_EXPECT(original.hasIdentity && original.hasName && original.hasRestoreMaterial);
    ZeroBytes(view, sizeof(view));
    view[21] = 1;
    InventoryRuntimeRestoreView(view, original);
    INV_RUNTIME_EXPECT(*reinterpret_cast<unsigned long long*>(view + 8) == 0xABCDEF1234567890ULL);
    INV_RUNTIME_EXPECT(*reinterpret_cast<unsigned int*>(view + 16) == 12345);
    INV_RUNTIME_EXPECT(view[20] == 1 && view[21] == 0 && view[64] == 'n' && view[67] == 'e');
    g_inventoryRuntime = savedRuntime;
    g_inventoryEconRuntime = savedEcon;

    const unsigned int savedFrame = g_inventoryRuntimeFrame;
    g_inventoryRuntimeFrame = 5;
    INV_RUNTIME_EXPECT(InventoryRuntimeRetryDue(0));
    INV_RUNTIME_EXPECT(InventoryRuntimeRetryDue(4));
    INV_RUNTIME_EXPECT(!InventoryRuntimeRetryDue(65));
    INV_RUNTIME_EXPECT(InventoryRuntimeRetryDue(0xFFFFFFFEu));
    g_inventoryRuntimeFrame = savedFrame;

    const int savedOriginalCount = g_originalWeaponCosmeticsCount;
    const bool savedGloveCaptured = g_originalGloveState.captured;
    const bool savedMusicCaptured = g_originalMusicKitState.captured;
    const int savedCleanupMirrorCount = g_inventoryEconMirrorCount;
    g_originalWeaponCosmeticsCount = 0;
    g_originalGloveState.captured = false;
    g_originalMusicKitState.captured = false;
    g_inventoryEconMirrorCount = 0;
    INV_RUNTIME_EXPECT(InventoryRuntimeCleanupFinished());
    g_originalWeaponCosmeticsCount = 1;
    INV_RUNTIME_EXPECT(!InventoryRuntimeCleanupFinished());
    g_originalWeaponCosmeticsCount = 0;
    g_originalGloveState.captured = true;
    INV_RUNTIME_EXPECT(!InventoryRuntimeCleanupFinished());
    g_originalGloveState.captured = false;
    g_originalMusicKitState.captured = true;
    INV_RUNTIME_EXPECT(!InventoryRuntimeCleanupFinished());
    g_originalMusicKitState.captured = false;
    g_inventoryEconMirrorCount = 1;
    INV_RUNTIME_EXPECT(!InventoryRuntimeCleanupFinished());
    g_originalWeaponCosmeticsCount = savedOriginalCount;
    g_originalGloveState.captured = savedGloveCaptured;
    g_originalMusicKitState.captured = savedMusicCaptured;
    g_inventoryEconMirrorCount = savedCleanupMirrorCount;

    INV_RUNTIME_EXPECT(!InventoryEconRemoveMirrorAt(-1, false));
    InventoryEconMirror inactive{};
    INV_RUNTIME_EXPECT(InventoryEconDestroyMirror(&inactive, true));
    InventoryEconMirror stale{};
    stale.active = true;
    stale.object = reinterpret_cast<void*>(1);
    stale.inventory = reinterpret_cast<void*>(1);
    INV_RUNTIME_EXPECT(InventoryEconDestroyMirror(&stale, false));
    INV_RUNTIME_EXPECT(!stale.active && !stale.object);
    const int savedMirrorCount = g_inventoryEconMirrorCount;
    const InventoryEconMirror savedMirror0 = g_inventoryEconMirrors[0];
    const InventoryEconMirror savedMirror1 = g_inventoryEconMirrors[1];
    g_inventoryEconMirrorCount = 2;
    g_inventoryEconMirrors[0] = {};
    g_inventoryEconMirrors[1] = {};
    g_inventoryEconMirrors[0].virtualItemId = 101;
    g_inventoryEconMirrors[1].virtualItemId = 202;
    INV_RUNTIME_EXPECT(InventoryEconRemoveMirrorAt(0, false));
    INV_RUNTIME_EXPECT(g_inventoryEconMirrorCount == 1 &&
        g_inventoryEconMirrors[0].virtualItemId == 202 &&
        g_inventoryEconMirrors[1].virtualItemId == 0);
    g_inventoryEconMirrors[0] = savedMirror0;
    g_inventoryEconMirrors[1] = savedMirror1;
    g_inventoryEconMirrorCount = savedMirrorCount;

#undef INV_RUNTIME_EXPECT
    printf("Inventory runtime policy tests: %d failures\n", failures);
    return failures;
}
