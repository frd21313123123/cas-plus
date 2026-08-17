// cas+ redesigned Inventory detail editor.
// Injected after ui_redesign.inc.  It uses the existing InventoryStore mutation
// functions directly, so the visual migration does not duplicate inventory
// state or game-facing logic.

static const wchar_t* CasUiWearLabel(float wear)
{
    if (wear < 0.07f) return L"Factory New";
    if (wear < 0.15f) return L"Minimal Wear";
    if (wear < 0.38f) return L"Field-Tested";
    if (wear < 0.45f) return L"Well-Worn";
    return L"Battle-Scarred";
}

static void CasUiDrawValuePill(HDC hdc, int x, int y, int w,
    const wchar_t* text, bool active = false)
{
    DrawRoundedCard(hdc, x, y, w, 28,
        active ? CAS_UI_ACCENT_SOFT : CAS_UI_SURFACE_3,
        active ? CAS_UI_BORDER_HI : CAS_UI_BORDER, 6);
    CasUiDrawLabel(hdc, text, x, y, w, 28,
        active ? CAS_UI_TEXT : CAS_UI_MUTED, 11, 550, DT_CENTER);
}

static void CasUiDrawEditorSection(HDC hdc, int x, int y, int w, int h,
    const wchar_t* title)
{
    DrawRoundedCard(hdc, x, y, w, h,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawLabel(hdc, title, x + 16, y + 10, w - 32, 18,
        CAS_UI_MUTED_2, 10, 650, DT_LEFT);
}

static void CasUiCycleSelectedDefinitionV2(
    const VirtualInventoryItem& item, int direction)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        InventoryUiCycleAgentCatalog(direction);
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        InventoryUiCycleMusicCatalog(direction);
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        InventoryUiCycleGloveCatalog(direction);
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        InventoryUiCycleKnifeCatalog(direction);
    else
        InventoryUiCycleWeaponCatalog(direction);
}

static void CasUiDrawInventoryRuntimeStatus(HDC hdc, int x, int y, int w)
{
    const bool materialReady =
        g_inventoryVisualRefreshRuntime.compositeReady &&
        g_inventoryVisualRefreshRuntime.compositeOwnerOffsetReady;
    const bool setReady = g_inventoryVisualRefreshRuntime.compositeSetReady;
    const bool attrsReady = g_inventoryVisualRefreshRuntime.viewAttributeReady;

    DrawRoundedCard(hdc, x, y, w, 76,
        RGB_COLOR(13, 16, 25), CAS_UI_BORDER, 7);
    CasUiDrawLabel(hdc, L"LIVE SKIN BACKEND", x + 12, y + 8,
        w - 24, 16, CAS_UI_MUTED_2, 9, 650, DT_LEFT);
    CasUiDrawBadge(hdc, x + 12, y + 30, 76,
        materialReady ? L"MATERIAL" : L"MISSING",
        materialReady ? CAS_UI_SUCCESS : CAS_UI_DANGER,
        materialReady ? RGB_COLOR(25, 50, 37) : RGB_COLOR(58, 25, 34));
    CasUiDrawBadge(hdc, x + 94, y + 30, 58,
        setReady ? L"SET" : L"NO SET",
        setReady ? CAS_UI_SUCCESS : CAS_UI_MUTED,
        setReady ? RGB_COLOR(25, 50, 37) : CAS_UI_SURFACE_3);
    CasUiDrawBadge(hdc, x + 158, y + 30, 58,
        attrsReady ? L"ATTR" : L"NO ATTR",
        attrsReady ? CAS_UI_SUCCESS : CAS_UI_MUTED,
        attrsReady ? RGB_COLOR(25, 50, 37) : CAS_UI_SURFACE_3);

    CasUiDrawLabel(hdc, L"material rebuilds", x + 12, y + 54,
        102, 14, CAS_UI_MUTED_2, 9, 400, DT_LEFT);
    RECT countRc = { x + 118, y + 53, x + 160, y + 69 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, g_inventoryCompositeRefreshCalls,
        &countRc, DT_LEFT | DT_SINGLELINE);
}

static void CasUiDrawInventoryDetailV2(HDC hdc, int width, int height)
{
    CasUiDrawPageChrome(hdc, width, height, 3);
    CasUiDrawButton(hdc, CAS_UI_CONTENT_X, 66, 86, 30, L"<  Items");
    CasUiDrawLabel(hdc, L"Item editor", CAS_UI_CONTENT_X + 102, 66,
        220, 30, CAS_UI_TEXT, 15, 650, DT_LEFT);

    InventoryUiSnapshot snapshot{};
    if (!InventoryGetFilteredUiSnapshot(&snapshot) ||
        !snapshot.hasSelectedItem)
    {
        DrawRoundedCard(hdc, CAS_UI_CONTENT_X, 112, 762, 210,
            CAS_UI_SURFACE, CAS_UI_BORDER, 8);
        CasUiDrawLabel(hdc, L"No item selected", CAS_UI_CONTENT_X, 174,
            762, 28, CAS_UI_TEXT, 17, 650, DT_CENTER);
        CasUiDrawLabel(hdc, L"Return to My items and select a card.",
            CAS_UI_CONTENT_X, 207, 762, 22,
            CAS_UI_MUTED, 12, 400, DT_CENTER);
        return;
    }

    const VirtualInventoryItem& item = snapshot.selectedItem;
    const wchar_t* name = CasUiInventoryDomainName(item);
    const COLORREF accent = CasUiItemAccent(item);

    // Left hero/summary panel.
    DrawRoundedCard(hdc, CAS_UI_CONTENT_X, 110, 250, 430,
        CAS_UI_SURFACE, CAS_UI_BORDER, 8);
    CasUiDrawRect(hdc, CAS_UI_CONTENT_X, 110,
        CAS_UI_CONTENT_X + 3, 540, accent);
    CasUiDrawItemGlyph(hdc, CAS_UI_CONTENT_X + 22, 136, name, accent);
    CasUiDrawLabel(hdc, name, CAS_UI_CONTENT_X + 22, 190,
        206, 24, CAS_UI_TEXT, 17, 650, DT_LEFT);

    wchar_t customName[40]{};
    if (item.customNameLength > 0)
        CasUiAsciiToWide(item.customName, customName,
            static_cast<int>(sizeof(customName) / sizeof(customName[0])));
    CasUiDrawLabel(hdc,
        item.customNameLength > 0 ? customName : L"No custom name",
        CAS_UI_CONTENT_X + 22, 216, 206, 20,
        item.customNameLength > 0 ? CAS_UI_ACCENT : CAS_UI_MUTED_2,
        11, 400, DT_LEFT);

    CasUiDrawLabel(hdc, L"Definition", CAS_UI_CONTENT_X + 22, 256,
        90, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    RECT defValue = { CAS_UI_CONTENT_X + 128, 255,
        CAS_UI_CONTENT_X + 218, 274 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, item.overrideDefinitionIndex, &defValue,
        DT_RIGHT | DT_SINGLELINE);

    CasUiDrawLabel(hdc, L"Paint kit", CAS_UI_CONTENT_X + 22, 282,
        90, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    RECT paintValue = { CAS_UI_CONTENT_X + 128, 281,
        CAS_UI_CONTENT_X + 218, 300 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, static_cast<unsigned int>(item.paintKit),
        &paintValue, DT_RIGHT | DT_SINGLELINE);

    CasUiDrawLabel(hdc, L"Wear", CAS_UI_CONTENT_X + 22, 308,
        90, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    CasUiDrawLabel(hdc, CasUiWearLabel(item.wear),
        CAS_UI_CONTENT_X + 112, 306, 106, 20,
        CAS_UI_TEXT, 10, 550, DT_RIGHT);

    CasUiDrawLabel(hdc, L"Quality", CAS_UI_CONTENT_X + 22, 334,
        90, 18, CAS_UI_MUTED_2, 10, 500, DT_LEFT);
    CasUiDrawLabel(hdc, InventoryQualityName(item.quality),
        CAS_UI_CONTENT_X + 112, 332, 106, 20,
        CAS_UI_TEXT, 10, 550, DT_RIGHT);

    if (item.equippedTeams != INVENTORY_TEAM_NONE)
        CasUiDrawBadge(hdc, CAS_UI_CONTENT_X + 22, 370, 72,
            CasUiTeamBadge(item.equippedTeams), CAS_UI_SUCCESS,
            RGB_COLOR(25, 50, 37));
    else
        CasUiDrawBadge(hdc, CAS_UI_CONTENT_X + 22, 370, 72,
            L"UNEQUIPPED", CAS_UI_MUTED, CAS_UI_SURFACE_3);

    CasUiDrawInventoryRuntimeStatus(hdc,
        CAS_UI_CONTENT_X + 16, 444, 218);

    // Definition / item selection.
    CasUiDrawEditorSection(hdc, 438, 110, 498, 78, L"ITEM");
    CasUiDrawLabel(hdc, name, 454, 136, 280, 28,
        CAS_UI_TEXT, 14, 600, DT_LEFT);
    CasUiDrawButton(hdc, 802, 132, 54, 30, L"<");
    CasUiDrawButton(hdc, 864, 132, 56, 30, L">");

    // Finish.
    CasUiDrawEditorSection(hdc, 438, 198, 498, 178, L"FINISH");
    CasUiDrawLabel(hdc, L"Paint", 454, 226, 60, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawLabel(hdc,
        InventoryCatalogPaintName(item.paintKit, item.slotDefinitionIndex),
        520, 224, 204, 22, CAS_UI_TEXT, 11, 550, DT_LEFT);
    CasUiDrawButton(hdc, 742, 220, 78, 28, L"< Skin");
    CasUiDrawButton(hdc, 826, 220, 94, 28, L"Skin >");
    CasUiDrawButton(hdc, 742, 254, 38, 26, L"-1");
    CasUiDrawButton(hdc, 784, 254, 38, 26, L"+1");

    CasUiDrawLabel(hdc, L"Seed", 454, 264, 60, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    RECT seedValue = { 520, 263, 590, 282 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, static_cast<unsigned int>(item.seed),
        &seedValue, DT_LEFT | DT_SINGLELINE);
    CasUiDrawButton(hdc, 604, 254, 42, 26, L"-10");
    CasUiDrawButton(hdc, 650, 254, 38, 26, L"-1");
    CasUiDrawButton(hdc, 692, 254, 38, 26, L"+1");
    CasUiDrawButton(hdc, 826, 254, 94, 26, L"+10");

    CasUiDrawLabel(hdc, L"Wear", 454, 306, 60, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawValuePill(hdc, 520, 300, 72, L"FN", item.wear < 0.07f);
    CasUiDrawValuePill(hdc, 598, 300, 72, L"MW",
        item.wear >= 0.07f && item.wear < 0.15f);
    CasUiDrawValuePill(hdc, 676, 300, 72, L"FT",
        item.wear >= 0.15f && item.wear < 0.38f);
    CasUiDrawValuePill(hdc, 754, 300, 72, L"WW",
        item.wear >= 0.38f && item.wear < 0.45f);
    CasUiDrawValuePill(hdc, 832, 300, 72, L"BS", item.wear >= 0.45f);

    // StatTrak / quality.
    CasUiDrawEditorSection(hdc, 438, 386, 498, 80, L"TRACKING & QUALITY");
    CasUiDrawLabel(hdc, L"StatTrak", 454, 412, 68, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    if (item.statTrak >= 0)
    {
        RECT statValue = { 526, 411, 584, 430 };
        SetTextColor(hdc, CAS_UI_TEXT);
        DrawInventoryNumber(hdc, static_cast<unsigned int>(item.statTrak),
            &statValue, DT_LEFT | DT_SINGLELINE);
    }
    else
        CasUiDrawLabel(hdc, L"Off", 526, 410, 50, 20,
            CAS_UI_MUTED_2, 11, 500, DT_LEFT);
    CasUiDrawButton(hdc, 594, 404, 46, 28, L"Off");
    CasUiDrawButton(hdc, 646, 404, 46, 28, L"-100");
    CasUiDrawButton(hdc, 698, 404, 40, 28, L"-1");
    CasUiDrawButton(hdc, 744, 404, 40, 28, L"+1");
    CasUiDrawButton(hdc, 790, 404, 54, 28, L"+100");
    CasUiDrawButton(hdc, 850, 404, 70, 28, L"Toggle");

    CasUiDrawLabel(hdc, InventoryQualityName(item.quality),
        594, 438, 140, 20, CAS_UI_TEXT, 11, 550, DT_LEFT);
    CasUiDrawButton(hdc, 744, 436, 82, 24, L"< Quality");
    CasUiDrawButton(hdc, 832, 436, 88, 24, L"Quality >");

    // Loadout.
    CasUiDrawEditorSection(hdc, 438, 476, 498, 64, L"LOADOUT");
    CasUiDrawValuePill(hdc, 454, 502, 88, L"None",
        item.equippedTeams == INVENTORY_TEAM_NONE);
    CasUiDrawValuePill(hdc, 548, 502, 82, L"T",
        item.equippedTeams == INVENTORY_TEAM_T);
    CasUiDrawValuePill(hdc, 636, 502, 82, L"CT",
        item.equippedTeams == INVENTORY_TEAM_CT);
    CasUiDrawValuePill(hdc, 724, 502, 112, L"T + CT",
        item.equippedTeams == INVENTORY_TEAM_BOTH);

    // Bottom action bar.
    CasUiDrawButton(hdc, 174, 552, 104, 32, L"Duplicate");
    CasUiDrawButton(hdc, 284, 552, 92, 32, L"Delete");
    CasUiDrawButton(hdc, 382, 552, 98, 32, L"Stickers");
    CasUiDrawButton(hdc, 486, 552, 90, 32, L"Name");
    CasUiDrawButton(hdc, 582, 552, 106, 32, L"Swap ST");
    CasUiDrawButton(hdc, 694, 552, 104, 32, L"Reset");
}

static bool CasUiHandleInventoryDetailV2Click(int mouseX, int mouseY)
{
    if (CasUiPointInRect(mouseX, mouseY,
        CAS_UI_CONTENT_X, 66, 86, 30))
    {
        g_casUiInventoryView = 0;
        return true;
    }

    InventoryUiSnapshot snapshot{};
    if (!InventoryGetFilteredUiSnapshot(&snapshot) ||
        !snapshot.hasSelectedItem)
        return true;
    const VirtualInventoryItem& item = snapshot.selectedItem;

    if (CasUiPointInRect(mouseX, mouseY, 802, 132, 54, 30))
        CasUiCycleSelectedDefinitionV2(item, -1);
    else if (CasUiPointInRect(mouseX, mouseY, 864, 132, 56, 30))
        CasUiCycleSelectedDefinitionV2(item, 1);
    else if (CasUiPointInRect(mouseX, mouseY, 742, 220, 78, 28))
        InventoryUiCyclePaintCatalog(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 826, 220, 94, 28))
        InventoryUiCyclePaintCatalog(1);
    else if (CasUiPointInRect(mouseX, mouseY, 742, 254, 38, 26))
        InventoryUiAdjustPaint(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 784, 254, 38, 26))
        InventoryUiAdjustPaint(1);
    else if (CasUiPointInRect(mouseX, mouseY, 604, 254, 42, 26))
        InventoryUiAdjustSeed(-10);
    else if (CasUiPointInRect(mouseX, mouseY, 650, 254, 38, 26))
        InventoryUiAdjustSeed(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 692, 254, 38, 26))
        InventoryUiAdjustSeed(1);
    else if (CasUiPointInRect(mouseX, mouseY, 826, 254, 94, 26))
        InventoryUiAdjustSeed(10);
    else if (CasUiPointInRect(mouseX, mouseY, 520, 300, 72, 28))
        InventoryUiSetWear(0.0001f);
    else if (CasUiPointInRect(mouseX, mouseY, 598, 300, 72, 28))
        InventoryUiSetWear(0.08f);
    else if (CasUiPointInRect(mouseX, mouseY, 676, 300, 72, 28))
        InventoryUiSetWear(0.20f);
    else if (CasUiPointInRect(mouseX, mouseY, 754, 300, 72, 28))
        InventoryUiSetWear(0.40f);
    else if (CasUiPointInRect(mouseX, mouseY, 832, 300, 72, 28))
        InventoryUiSetWear(0.55f);
    else if (CasUiPointInRect(mouseX, mouseY, 594, 404, 46, 28))
        InventoryUiDisableStatTrak();
    else if (CasUiPointInRect(mouseX, mouseY, 646, 404, 46, 28))
        InventoryUiAdjustStatTrak(-100);
    else if (CasUiPointInRect(mouseX, mouseY, 698, 404, 40, 28))
        InventoryUiAdjustStatTrak(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 744, 404, 40, 28))
        InventoryUiAdjustStatTrak(1);
    else if (CasUiPointInRect(mouseX, mouseY, 790, 404, 54, 28))
        InventoryUiAdjustStatTrak(100);
    else if (CasUiPointInRect(mouseX, mouseY, 850, 404, 70, 28))
        InventoryUiToggleStatTrak();
    else if (CasUiPointInRect(mouseX, mouseY, 744, 436, 82, 24))
        InventoryUiCycleQuality(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 832, 436, 88, 24))
        InventoryUiCycleQuality(1);
    else if (CasUiPointInRect(mouseX, mouseY, 454, 502, 88, 28))
        InventoryUiSetEquipMask(INVENTORY_TEAM_NONE);
    else if (CasUiPointInRect(mouseX, mouseY, 548, 502, 82, 28))
        InventoryUiSetEquipMask(INVENTORY_TEAM_T);
    else if (CasUiPointInRect(mouseX, mouseY, 636, 502, 82, 28))
        InventoryUiSetEquipMask(INVENTORY_TEAM_CT);
    else if (CasUiPointInRect(mouseX, mouseY, 724, 502, 112, 28))
        InventoryUiSetEquipMask(INVENTORY_TEAM_BOTH);
    else if (CasUiPointInRect(mouseX, mouseY, 174, 552, 104, 32))
        InventoryUiDuplicateSelected();
    else if (CasUiPointInRect(mouseX, mouseY, 284, 552, 92, 32))
    {
        InventoryUiDeleteSelected();
        g_casUiInventoryView = 0;
    }
    else if (CasUiPointInRect(mouseX, mouseY, 382, 552, 98, 32))
        InventoryUiOpenStickerModal();
    else if (CasUiPointInRect(mouseX, mouseY, 486, 552, 90, 32))
        InventoryUiBeginNameEdit();
    else if (CasUiPointInRect(mouseX, mouseY, 582, 552, 106, 32))
        InventoryUiArmStatTrakSwap();
    else if (CasUiPointInRect(mouseX, mouseY, 694, 552, 104, 32))
        InventoryUiResetSelectedCosmetics();

    // Inventory V2 owns its whole page. Unknown clicks are intentionally
    // consumed so invisible legacy controls can never fire underneath it.
    return true;
}
