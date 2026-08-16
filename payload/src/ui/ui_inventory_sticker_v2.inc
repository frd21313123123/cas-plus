// cas+ redesigned sticker editor overlay.
// Uses the existing sticker sidecar and mutation helpers; this is UI only.

static void CasUiFormatSignedHundredths(float value,
    wchar_t* output, int capacity)
{
    if (!output || capacity < 6)
        return;
    int scaled = static_cast<int>(value * 100.0f);
    int pos = 0;
    if (scaled < 0)
    {
        output[pos++] = L'-';
        scaled = -scaled;
    }
    else
        output[pos++] = L'+';
    const int whole = scaled / 100;
    const int fraction = scaled % 100;
    if (whole >= 10)
        output[pos++] = static_cast<wchar_t>(L'0' + (whole / 10) % 10);
    output[pos++] = static_cast<wchar_t>(L'0' + whole % 10);
    output[pos++] = L'.';
    output[pos++] = static_cast<wchar_t>(L'0' + fraction / 10);
    output[pos++] = static_cast<wchar_t>(L'0' + fraction % 10);
    output[pos] = 0;
}

static void CasUiDrawStickerModalV2(HDC hdc)
{
    if (!g_inventoryStickerModal)
        return;

    InventoryUiSnapshot snapshot{};
    if (!InventoryGetFilteredUiSnapshot(&snapshot) ||
        !snapshot.hasSelectedItem)
        return;

    InventoryStickerRecord record{};
    InventoryStickerCopyRecord(snapshot.selectedItem.itemId, &record);
    const int slotIndex = InventoryClampInt(g_inventoryStickerUiSlot,
        0, INVENTORY_STICKER_SLOT_COUNT - 1);
    const InventoryStickerSlot& sticker = record.slots[slotIndex];

    // Opaque modal veil keeps the no-CRT/GDI path deterministic while still
    // making the editor visually separate from the page underneath.
    DrawRoundedCard(hdc, 254, 92, 626, 452,
        RGB_COLOR(9, 11, 18), CAS_UI_BORDER_HI, 10);
    CasUiDrawRect(hdc, 254, 92, 258, 544, CAS_UI_ACCENT);

    CasUiDrawLabel(hdc, L"Sticker editor", 280, 112, 240, 28,
        CAS_UI_TEXT, 18, 650, DT_LEFT);
    CasUiDrawLabel(hdc,
        CasUiInventoryDomainName(snapshot.selectedItem),
        280, 141, 300, 20, CAS_UI_MUTED, 11, 400, DT_LEFT);
    CasUiDrawButton(hdc, 786, 112, 68, 30, L"Close");

    CasUiDrawLabel(hdc, L"SLOTS", 280, 178, 100, 18,
        CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        wchar_t label[2] = { static_cast<wchar_t>(L'1' + i), 0 };
        CasUiDrawValuePill(hdc, 280 + i * 64, 202, 54, label,
            i == slotIndex);
    }

    CasUiDrawEditorSection(hdc, 280, 246, 574, 96, L"STICKER");
    CasUiDrawLabel(hdc, L"Sticker ID", 296, 274, 82, 22,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    RECT idRc = { 382, 273, 456, 294 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, static_cast<unsigned int>(sticker.id),
        &idRc, DT_LEFT | DT_SINGLELINE);
    CasUiDrawButton(hdc, 478, 268, 54, 28, L"-100");
    CasUiDrawButton(hdc, 538, 268, 44, 28, L"-1");
    CasUiDrawButton(hdc, 588, 268, 44, 28, L"+1");
    CasUiDrawButton(hdc, 638, 268, 58, 28, L"+100");
    CasUiDrawButton(hdc, 712, 268, 58, 28, L"Scrape");
    CasUiDrawButton(hdc, 776, 268, 62, 28, L"Remove");

    CasUiDrawEditorSection(hdc, 280, 352, 574, 112, L"PLACEMENT");
    CasUiDrawLabel(hdc, L"Rotation", 296, 378, 74, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawButton(hdc, 374, 372, 48, 28, L"-15");
    CasUiDrawButton(hdc, 428, 372, 48, 28, L"+15");
    CasUiDrawLabel(hdc, L"Scale", 498, 378, 48, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawButton(hdc, 550, 372, 48, 28, L"-.1");
    CasUiDrawButton(hdc, 604, 372, 48, 28, L"+.1");
    CasUiDrawLabel(hdc, L"Anchor", 676, 378, 56, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    RECT anchorRc = { 734, 377, 756, 398 };
    SetTextColor(hdc, CAS_UI_TEXT);
    DrawInventoryNumber(hdc, sticker.schema, &anchorRc,
        DT_LEFT | DT_SINGLELINE);
    CasUiDrawButton(hdc, 764, 372, 34, 28, L"<");
    CasUiDrawButton(hdc, 804, 372, 34, 28, L">");

    wchar_t xText[16]{};
    wchar_t yText[16]{};
    CasUiFormatSignedHundredths(sticker.offsetX, xText,
        static_cast<int>(sizeof(xText) / sizeof(xText[0])));
    CasUiFormatSignedHundredths(sticker.offsetY, yText,
        static_cast<int>(sizeof(yText) / sizeof(yText[0])));
    CasUiDrawLabel(hdc, L"Offset X", 296, 420, 62, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawLabel(hdc, xText, 360, 420, 58, 20,
        CAS_UI_TEXT, 10, 550, DT_LEFT);
    CasUiDrawButton(hdc, 420, 414, 48, 28, L"-.01");
    CasUiDrawButton(hdc, 474, 414, 48, 28, L"+.01");
    CasUiDrawLabel(hdc, L"Offset Y", 548, 420, 62, 20,
        CAS_UI_MUTED, 11, 500, DT_LEFT);
    CasUiDrawLabel(hdc, yText, 612, 420, 58, 20,
        CAS_UI_TEXT, 10, 550, DT_LEFT);
    CasUiDrawButton(hdc, 672, 414, 48, 28, L"-.01");
    CasUiDrawButton(hdc, 726, 414, 48, 28, L"+.01");

    CasUiDrawButton(hdc, 280, 480, 104, 32, L"Move left");
    CasUiDrawButton(hdc, 392, 480, 112, 32, L"Move right");
    CasUiDrawLabel(hdc,
        L"Sticker changes rebuild the local econ item; no raw attribute-vector writes.",
        530, 480, 308, 32, CAS_UI_MUTED_2, 10, 400, DT_RIGHT);
}

static bool CasUiHandleStickerModalV2Click(int mouseX, int mouseY)
{
    if (!g_inventoryStickerModal)
        return false;

    if (CasUiPointInRect(mouseX, mouseY, 786, 112, 68, 30))
        InventoryUiCloseStickerModal();
    else if (mouseY >= 202 && mouseY < 230 &&
        mouseX >= 280 && mouseX < 600)
    {
        const int slot = (mouseX - 280) / 64;
        if (slot >= 0 && slot < INVENTORY_STICKER_SLOT_COUNT &&
            CasUiPointInRect(mouseX, mouseY, 280 + slot * 64, 202, 54, 28))
            InventoryUiSelectStickerSlot(slot);
    }
    else if (CasUiPointInRect(mouseX, mouseY, 478, 268, 54, 28))
        InventoryUiAdjustStickerId(-100);
    else if (CasUiPointInRect(mouseX, mouseY, 538, 268, 44, 28))
        InventoryUiAdjustStickerId(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 588, 268, 44, 28))
        InventoryUiAdjustStickerId(1);
    else if (CasUiPointInRect(mouseX, mouseY, 638, 268, 58, 28))
        InventoryUiAdjustStickerId(100);
    else if (CasUiPointInRect(mouseX, mouseY, 712, 268, 58, 28))
        InventoryUiScrapeSticker();
    else if (CasUiPointInRect(mouseX, mouseY, 776, 268, 62, 28))
        InventoryUiRemoveSticker();
    else if (CasUiPointInRect(mouseX, mouseY, 374, 372, 48, 28))
        InventoryUiAdjustStickerRotation(-15.0f);
    else if (CasUiPointInRect(mouseX, mouseY, 428, 372, 48, 28))
        InventoryUiAdjustStickerRotation(15.0f);
    else if (CasUiPointInRect(mouseX, mouseY, 550, 372, 48, 28))
        InventoryUiAdjustStickerScale(-0.1f);
    else if (CasUiPointInRect(mouseX, mouseY, 604, 372, 48, 28))
        InventoryUiAdjustStickerScale(0.1f);
    else if (CasUiPointInRect(mouseX, mouseY, 764, 372, 34, 28))
        InventoryUiAdjustStickerSchema(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 804, 372, 34, 28))
        InventoryUiAdjustStickerSchema(1);
    else if (CasUiPointInRect(mouseX, mouseY, 420, 414, 48, 28))
        InventoryUiAdjustStickerOffsetX(-0.01f);
    else if (CasUiPointInRect(mouseX, mouseY, 474, 414, 48, 28))
        InventoryUiAdjustStickerOffsetX(0.01f);
    else if (CasUiPointInRect(mouseX, mouseY, 672, 414, 48, 28))
        InventoryUiAdjustStickerOffsetY(-0.01f);
    else if (CasUiPointInRect(mouseX, mouseY, 726, 414, 48, 28))
        InventoryUiAdjustStickerOffsetY(0.01f);
    else if (CasUiPointInRect(mouseX, mouseY, 280, 480, 104, 32))
        InventoryUiMoveSticker(-1);
    else if (CasUiPointInRect(mouseX, mouseY, 392, 480, 112, 32))
        InventoryUiMoveSticker(1);

    // A modal owns all pointer input, including clicks outside the panel.
    return true;
}
