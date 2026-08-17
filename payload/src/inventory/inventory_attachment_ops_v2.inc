// Attachment operations that depend on InventorySelectedItemId and the sticker
// sidecar being defined. Kept separate from inventory_attachments_v2.inc so the
// build compositor can place each layer at a compile-safe point.

static bool InventoryUiCycleRealStickerAttachment(int delta)
{
    unsigned long long itemId = 0;
    unsigned short slotDefinition = 0;
    if (delta == 0 || !InventorySelectedItemId(&itemId, &slotDefinition))
        return false;
    if (slotDefinition == INVENTORY_SLOT_GLOVE ||
        slotDefinition == INVENTORY_SLOT_MUSIC ||
        slotDefinition == INVENTORY_SLOT_CONTAINER)
        return false;

    const int limit = InventoryAttachmentSlotLimit(slotDefinition);
    if (g_inventoryStickerUiSlot < 0 || g_inventoryStickerUiSlot >= limit ||
        !InventoryStickerTryLock())
        return false;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, true);
    if (!record)
    {
        InventoryStickerUnlock();
        return false;
    }
    InventoryStickerSlot& slot = record->slots[g_inventoryStickerUiSlot];
    const BYTE kind = InventoryAttachmentKindForSlot(slotDefinition);
    const int steps = (delta <= -100 || delta >= 100) ? 10 : 1;
    for (int i = 0; i < steps; ++i)
    {
        const InventoryAttachmentCatalogRecord* next = InventoryAttachmentCycle(
            kind, slot.id, delta > 0 ? 1 : -1);
        if (!next)
            break;
        slot.id = next->id;
    }
    if (slot.id > 0 && slot.scale <= 0.0f)
        slot.scale = 1.0f;
    InventoryStickerSanitize(&slot);
    MarkInventoryStickerDirty();
    InventoryStickerUnlock();
    MarkInventoryDirty();
    return slot.id > 0;
}

static const wchar_t* InventorySelectedAttachmentName()
{
    unsigned long long itemId = 0;
    unsigned short slotDefinition = 0;
    if (!InventorySelectedItemId(&itemId, &slotDefinition))
        return L"None";
    InventoryStickerRecord record{};
    InventoryStickerCopyRecord(itemId, &record);
    const int limit = InventoryAttachmentSlotLimit(slotDefinition);
    const int slotIndex = InventoryClampInt(g_inventoryStickerUiSlot,
        0, limit > 0 ? limit - 1 : 0);
    return InventoryAttachmentName(
        InventoryAttachmentKindForSlot(slotDefinition),
        record.slots[slotIndex].id);
}

static void InventoryAttachmentClearSelected()
{
    unsigned long long itemId = 0;
    unsigned short slotDefinition = 0;
    if (!InventorySelectedItemId(&itemId, &slotDefinition))
        return;
    if (InventoryStickerTryLock())
    {
        InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, false);
        if (record)
        {
            for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
            {
                ZeroBytes(&record->slots[i], sizeof(record->slots[i]));
                record->slots[i].scale = 1.0f;
                record->slots[i].schema = static_cast<BYTE>(i);
            }
            MarkInventoryStickerDirty();
        }
        InventoryStickerUnlock();
    }
    InventoryKeychainOnItemDeleted(itemId);
    MarkInventoryDirty();
}
