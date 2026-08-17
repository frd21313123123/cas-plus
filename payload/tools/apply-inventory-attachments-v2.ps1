param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$corePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_attachments_v2.inc'
$opsPath = Join-Path $PSScriptRoot '..\src\inventory\inventory_attachment_ops_v2.inc'
$uiPath = Join-Path $PSScriptRoot '..\src\ui\ui_inventory_keychain_v2.inc'
foreach ($path in @($corePath, $opsPath, $uiPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Attachment V2 module was not found: $path"
    }
}
$core = Get-Content -LiteralPath $corePath -Raw -Encoding UTF8
$ops = Get-Content -LiteralPath $opsPath -Raw -Encoding UTF8
$ui = Get-Content -LiteralPath $uiPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Attachment V2 anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

function Insert-BeforeRequired([string]$Anchor, [string]$Block, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Anchor))).Count
    if ($count -ne 1) {
        throw "Attachment V2 insert anchor '$Name' expected exactly once, found $count."
    }
    $index = $script:source.IndexOf($Anchor)
    $script:source = $script:source.Substring(0, $index) + $Block + "`r`n`r`n" +
        $script:source.Substring($index)
}

# Core state must exist before the extended delete/duplicate lifecycle and before
# InventoryEconApplyAttributes. The compact ops layer depends on
# InventorySelectedItemId, so it is placed immediately after that helper.
Insert-BeforeRequired 'static void InventoryExtendedOnItemDeleted(unsigned long long itemId)' `
    $core 'attachment catalog/keychain core'
Insert-BeforeRequired 'static bool InventoryMutateSelectedSticker(' `
    $ops 'real sticker/patch operations'

$deleteAnchor = @'
static void InventoryExtendedOnItemDeleted(unsigned long long itemId)
{
    if (!itemId || !InventoryStickerTryLock())
'@
$deleteReplacement = @'
static void InventoryExtendedOnItemDeleted(unsigned long long itemId)
{
    InventoryKeychainOnItemDeleted(itemId);
    if (!itemId || !InventoryStickerTryLock())
'@
Replace-Required $deleteAnchor $deleteReplacement 'keychain delete lifecycle'

$duplicateAnchor = @'
static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId)
{
    if (!sourceId || !newId || !InventoryStickerTryLock())
'@
$duplicateReplacement = @'
static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId)
{
    InventoryKeychainOnItemDuplicated(sourceId, newId);
    if (!sourceId || !newId || !InventoryStickerTryLock())
'@
Replace-Required $duplicateAnchor $duplicateReplacement 'keychain duplicate lifecycle'

$openAnchor = @'
    if (slot == INVENTORY_SLOT_GLOVE || slot == INVENTORY_SLOT_MUSIC ||
        slot == INVENTORY_SLOT_AGENT)
        return;
    g_inventoryStickerUiSlot = InventoryClampInt(
        g_inventoryStickerUiSlot, 0, INVENTORY_STICKER_SLOT_COUNT - 1);
    g_inventoryStickerModal = true;
'@
$openReplacement = @'
    if (slot == INVENTORY_SLOT_GLOVE || slot == INVENTORY_SLOT_MUSIC ||
        slot == INVENTORY_SLOT_CONTAINER)
        return;
    const int attachmentLimit = InventoryAttachmentSlotLimit(slot);
    g_inventoryStickerUiSlot = InventoryClampInt(
        g_inventoryStickerUiSlot, 0, attachmentLimit - 1);
    g_inventoryStickerModal = LoadInventoryAttachmentCatalog();
'@
Replace-Required $openAnchor $openReplacement 'agent patch modal enable + real catalog gate'

$mutateAnchor = @'
    if (!InventorySelectedItemId(&itemId, &slotDefinition) ||
        slotDefinition == INVENTORY_SLOT_GLOVE ||
        slotDefinition == INVENTORY_SLOT_MUSIC ||
        slotDefinition == INVENTORY_SLOT_AGENT ||
        !InventoryStickerTryLock())
        return false;
'@
$mutateReplacement = @'
    if (!InventorySelectedItemId(&itemId, &slotDefinition) ||
        slotDefinition == INVENTORY_SLOT_GLOVE ||
        slotDefinition == INVENTORY_SLOT_MUSIC ||
        slotDefinition == INVENTORY_SLOT_CONTAINER ||
        g_inventoryStickerUiSlot >= InventoryAttachmentSlotLimit(slotDefinition) ||
        !InventoryStickerTryLock())
        return false;
'@
Replace-Required $mutateAnchor $mutateReplacement 'agent patch mutation enable'

$adjustIdAnchor = @'
static void InventoryUiAdjustStickerId(int delta)
{
    g_inventoryStickerDelta = delta;
    InventoryMutateSelectedSticker(InventoryStickerAdjustIdMutation);
}
'@
$adjustIdReplacement = @'
static void InventoryUiAdjustStickerId(int delta)
{
    InventoryUiCycleRealStickerAttachment(delta);
}
'@
Replace-Required $adjustIdAnchor $adjustIdReplacement 'real-only sticker/patch cycling'

$moveAnchor = @'
static void InventoryUiMoveSticker(int direction)
{
    if (direction == 0)
        return;
    unsigned long long itemId = 0;
    if (!InventorySelectedItemId(&itemId) || !InventoryStickerTryLock())
        return;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, false);
    if (!record)
    {
        InventoryStickerUnlock();
        return;
    }
    const int from = InventoryClampInt(g_inventoryStickerUiSlot,
        0, INVENTORY_STICKER_SLOT_COUNT - 1);
    const int to = from + (direction > 0 ? 1 : -1);
    if (to >= 0 && to < INVENTORY_STICKER_SLOT_COUNT)
    {
        const InventoryStickerSlot tmp = record->slots[from];
        record->slots[from] = record->slots[to];
        record->slots[to] = tmp;
        g_inventoryStickerUiSlot = to;
        MarkInventoryStickerDirty();
    }
    InventoryStickerUnlock();
}
'@
$moveReplacement = @'
static void InventoryUiMoveSticker(int direction)
{
    if (direction == 0)
        return;
    unsigned long long itemId = 0;
    unsigned short slotDefinition = 0;
    if (!InventorySelectedItemId(&itemId, &slotDefinition) ||
        !InventoryStickerTryLock())
        return;
    InventoryStickerRecord* record = InventoryStickerFindLocked(itemId, false);
    if (!record)
    {
        InventoryStickerUnlock();
        return;
    }
    const int limit = InventoryAttachmentSlotLimit(slotDefinition);
    const int from = InventoryClampInt(g_inventoryStickerUiSlot, 0, limit - 1);
    const int to = from + (direction > 0 ? 1 : -1);
    if (to >= 0 && to < limit)
    {
        const InventoryStickerSlot tmp = record->slots[from];
        record->slots[from] = record->slots[to];
        record->slots[to] = tmp;
        g_inventoryStickerUiSlot = to;
        MarkInventoryStickerDirty();
        MarkInventoryDirty();
    }
    InventoryStickerUnlock();
}
'@
Replace-Required $moveAnchor $moveReplacement 'attachment-aware slot movement'

# Native CEconItem attributes already receive integer sticker IDs. Enforce the
# current-game attachment catalog and the agent patch slot limit before writing
# any of those IDs into a newly-created mirror.
$dynamicLoopAnchor = @'
    InventoryStickerRecord stickers;
    InventoryStickerCopyRecord(item.itemId, &stickers);
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        InventoryStickerSlot slot = stickers.slots[i];
        InventoryStickerSanitize(&slot);
        if (slot.id <= 0)
            continue;
        const int idIndex = 113 + i * 4;
'@
$dynamicLoopReplacement = @'
    InventoryStickerRecord stickers;
    InventoryStickerCopyRecord(item.itemId, &stickers);
    const int attachmentLimit = InventoryAttachmentSlotLimit(
        item.slotDefinitionIndex);
    const BYTE attachmentKind = InventoryAttachmentKindForSlot(
        item.slotDefinitionIndex);
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        InventoryStickerSlot slot = stickers.slots[i];
        InventoryStickerSanitize(&slot);
        if (i >= attachmentLimit || slot.id <= 0 ||
            !InventoryAttachmentFind(attachmentKind, slot.id))
            continue;
        const int idIndex = 113 + i * 4;
'@
Replace-Required $dynamicLoopAnchor $dynamicLoopReplacement 'native attachment validation'

$signatureAnchor = @'
    for (SIZE_T i = 0; i < sizeof(stickers.slots); ++i)
    {
        hash ^= stickerBytes[i];
        hash *= 16777619u;
    }
    return hash;
}
'@
$signatureReplacement = @'
    for (SIZE_T i = 0; i < sizeof(stickers.slots); ++i)
    {
        hash ^= stickerBytes[i];
        hash *= 16777619u;
    }
    InventoryAttachmentHashKeychain(item.itemId, &hash);
    return hash;
}
'@
Replace-Required $signatureAnchor $signatureReplacement 'keychain projection signature'

# The view fallback used numeric float casts for integer sticker IDs/schema and
# skipped empty slots. Source 2's typed econ path expects the integer bit pattern;
# write zero IDs too so removing a sticker cannot leave stale view attributes.
$viewStickerAnchor = @'
    if (item.slotDefinitionIndex != INVENTORY_SLOT_GLOVE &&
        item.slotDefinitionIndex != INVENTORY_SLOT_MUSIC &&
        item.slotDefinitionIndex != INVENTORY_SLOT_AGENT)
    {
        InventoryStickerRecord stickers;
        InventoryStickerCopyRecord(item.itemId, &stickers);
        for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
        {
            InventoryStickerSlot slot = stickers.slots[i];
            InventoryStickerSanitize(&slot);
            if (slot.id <= 0)
                continue;
            ok = InventoryEconSetViewFloat(view, stickerId[i],
                static_cast<float>(slot.id)) && ok;
            ok = InventoryEconSetViewFloat(view, stickerWear[i], slot.wear) && ok;
            ok = InventoryEconSetViewFloat(view, stickerScale[i], slot.scale) && ok;
            ok = InventoryEconSetViewFloat(view, stickerRotation[i], slot.rotation) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetX[i], slot.offsetX) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetY[i], slot.offsetY) && ok;
            ok = InventoryEconSetViewFloat(view, stickerSchema[i],
                static_cast<float>(slot.schema)) && ok;
        }
    }

    if (!g_inventoryRuntime.ready)
'@
$viewStickerReplacement = @'
    if (item.slotDefinitionIndex != INVENTORY_SLOT_GLOVE &&
        item.slotDefinitionIndex != INVENTORY_SLOT_MUSIC &&
        item.slotDefinitionIndex != INVENTORY_SLOT_CONTAINER)
    {
        InventoryStickerRecord stickers;
        InventoryStickerCopyRecord(item.itemId, &stickers);
        const int attachmentLimit = InventoryAttachmentSlotLimit(
            item.slotDefinitionIndex);
        const BYTE attachmentKind = InventoryAttachmentKindForSlot(
            item.slotDefinitionIndex);
        for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
        {
            InventoryStickerSlot slot = stickers.slots[i];
            InventoryStickerSanitize(&slot);
            if (i >= attachmentLimit || slot.id <= 0 ||
                !InventoryAttachmentFind(attachmentKind, slot.id))
            {
                slot.id = 0;
                slot.wear = 0.0f;
                slot.scale = 1.0f;
                slot.rotation = 0.0f;
                slot.offsetX = 0.0f;
                slot.offsetY = 0.0f;
                slot.schema = static_cast<BYTE>(i);
            }
            ok = InventoryEconSetViewFloat(view, stickerId[i],
                InventoryAttachmentUIntBits(
                    static_cast<unsigned int>(slot.id))) && ok;
            if (slot.id <= 0)
                continue;
            ok = InventoryEconSetViewFloat(view, stickerWear[i], slot.wear) && ok;
            ok = InventoryEconSetViewFloat(view, stickerScale[i], slot.scale) && ok;
            ok = InventoryEconSetViewFloat(view, stickerRotation[i], slot.rotation) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetX[i], slot.offsetX) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetY[i], slot.offsetY) && ok;
            ok = InventoryEconSetViewFloat(view, stickerSchema[i],
                InventoryAttachmentUIntBits(
                    static_cast<unsigned int>(slot.schema))) && ok;
        }
    }
    ok = InventoryAttachmentApplyKeychainView(view, item) && ok;

    if (!g_inventoryRuntime.ready)
'@
Replace-Required $viewStickerAnchor $viewStickerReplacement 'typed/stale-safe item-view attachments'

$loadAnchor = @'
static void InventoryExtendedLoad()
{
    LoadInventoryStickerStore();
    InventoryInstallTextCapture();
}
'@
$loadReplacement = @'
static void InventoryExtendedLoad()
{
    LoadInventoryAttachmentCatalog();
    LoadInventoryStickerStore();
    LoadInventoryKeychainStore();
    InventoryInstallTextCapture();
}
'@
Replace-Required $loadAnchor $loadReplacement 'attachment/keychain load'

$flushAnchor = @'
        FlushInventoryStickerPersistenceIfNeeded();
        Sleep(8);
'@
$flushReplacement = @'
        FlushInventoryStickerPersistenceIfNeeded();
        FlushInventoryKeychainPersistenceIfNeeded();
        Sleep(8);
'@
Replace-Required $flushAnchor $flushReplacement 'keychain persistence worker flush'

$shutdownAnchor = @'
static void ShutdownInventoryExtended()
{
    InventoryRemoveTextCapture();
    FlushInventoryStickerPersistenceIfNeeded();
    if (g_inventoryEconLastInventory)
'@
$shutdownReplacement = @'
static void ShutdownInventoryExtended()
{
    InventoryRemoveTextCapture();
    FlushInventoryStickerPersistenceIfNeeded();
    FlushInventoryKeychainPersistenceIfNeeded();
    if (g_inventoryEconLastInventory)
'@
Replace-Required $shutdownAnchor $shutdownReplacement 'keychain shutdown flush'

$shutdownTailAnchor = @'
    g_inventoryEconLastInventory = nullptr;
    g_inventoryStickerModal = false;
}
'@
$shutdownTailReplacement = @'
    g_inventoryEconLastInventory = nullptr;
    g_inventoryStickerModal = false;
    g_inventoryKeychainModal = false;
    ShutdownInventoryAttachmentCatalog();
}
'@
Replace-Required $shutdownTailAnchor $shutdownTailReplacement 'attachment catalog teardown'

# Redesigned Item Editor gets a dedicated Charm button. Agents rename the
# sticker action to Patches; unsupported domains simply reject Charm open.
$actionBarAnchor = @'
    // Bottom action bar.
    CasUiDrawButton(hdc, 174, 552, 104, 32, L"Duplicate");
    CasUiDrawButton(hdc, 284, 552, 92, 32, L"Delete");
    CasUiDrawButton(hdc, 382, 552, 98, 32, L"Stickers");
    CasUiDrawButton(hdc, 486, 552, 90, 32, L"Name");
    CasUiDrawButton(hdc, 582, 552, 106, 32, L"Swap ST");
    CasUiDrawButton(hdc, 694, 552, 104, 32, L"Reset");
'@
$actionBarReplacement = @'
    // Bottom action bar.
    const wchar_t* attachmentAction =
        item.slotDefinitionIndex == INVENTORY_SLOT_AGENT ? L"Patches" : L"Stickers";
    CasUiDrawButton(hdc, 174, 552, 96, 32, L"Duplicate");
    CasUiDrawButton(hdc, 276, 552, 82, 32, L"Delete");
    CasUiDrawButton(hdc, 364, 552, 96, 32, attachmentAction);
    CasUiDrawButton(hdc, 466, 552, 82, 32, L"Charm");
    CasUiDrawButton(hdc, 554, 552, 78, 32, L"Name");
    CasUiDrawButton(hdc, 638, 552, 96, 32, L"Swap ST");
    CasUiDrawButton(hdc, 740, 552, 86, 32, L"Reset");
'@
Replace-Required $actionBarAnchor $actionBarReplacement 'Inventory V2 attachment action bar'

$actionClickAnchor = @'
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
'@
$actionClickReplacement = @'
    else if (CasUiPointInRect(mouseX, mouseY, 174, 552, 96, 32))
        InventoryUiDuplicateSelected();
    else if (CasUiPointInRect(mouseX, mouseY, 276, 552, 82, 32))
    {
        InventoryUiDeleteSelected();
        g_casUiInventoryView = 0;
    }
    else if (CasUiPointInRect(mouseX, mouseY, 364, 552, 96, 32))
        InventoryUiOpenStickerModal();
    else if (CasUiPointInRect(mouseX, mouseY, 466, 552, 82, 32))
        InventoryUiOpenKeychainModal();
    else if (CasUiPointInRect(mouseX, mouseY, 554, 552, 78, 32))
        InventoryUiBeginNameEdit();
    else if (CasUiPointInRect(mouseX, mouseY, 638, 552, 96, 32))
        InventoryUiArmStatTrakSwap();
    else if (CasUiPointInRect(mouseX, mouseY, 740, 552, 86, 32))
    {
        InventoryUiResetSelectedCosmetics();
        InventoryAttachmentClearSelected();
    }
'@
Replace-Required $actionClickAnchor $actionClickReplacement 'Inventory V2 attachment clicks'

# Sticker overlay becomes Sticker/Patch depending on the selected domain and
# uses the current game_info slot count. IDs still render numerically for
# diagnostics, with the localized name shown underneath.
$stickerRecordAnchor = @'
    InventoryStickerRecord record{};
    InventoryStickerCopyRecord(snapshot.selectedItem.itemId, &record);
    const int slotIndex = InventoryClampInt(g_inventoryStickerUiSlot,
        0, INVENTORY_STICKER_SLOT_COUNT - 1);
    const InventoryStickerSlot& sticker = record.slots[slotIndex];
'@
$stickerRecordReplacement = @'
    InventoryStickerRecord record{};
    InventoryStickerCopyRecord(snapshot.selectedItem.itemId, &record);
    const bool patchMode =
        snapshot.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_AGENT;
    const int attachmentLimit = InventoryAttachmentSlotLimit(
        snapshot.selectedItem.slotDefinitionIndex);
    const int slotIndex = InventoryClampInt(g_inventoryStickerUiSlot,
        0, attachmentLimit - 1);
    const InventoryStickerSlot& sticker = record.slots[slotIndex];
'@
Replace-Required $stickerRecordAnchor $stickerRecordReplacement 'Sticker V2 patch mode state'

Replace-Required `
    '    CasUiDrawLabel(hdc, L"Sticker editor", 280, 112, 240, 28,' `
    '    CasUiDrawLabel(hdc, patchMode ? L"Patch editor" : L"Sticker editor", 280, 112, 240, 28,' `
    'Sticker V2 title'
Replace-Required `
    '    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)' `
    '    for (int i = 0; i < attachmentLimit; ++i)' `
    'Sticker V2 visible slot count'
Replace-Required `
    '    CasUiDrawEditorSection(hdc, 280, 246, 574, 96, L"STICKER");' `
    '    CasUiDrawEditorSection(hdc, 280, 246, 574, 96, patchMode ? L"PATCH" : L"STICKER");' `
    'Sticker V2 section title'
Replace-Required `
    '    CasUiDrawLabel(hdc, L"Sticker ID", 296, 274, 82, 22,' `
    '    CasUiDrawLabel(hdc, patchMode ? L"Patch ID" : L"Sticker ID", 296, 274, 82, 22,' `
    'Sticker V2 ID label'

$nameInsertAnchor = @'
    DrawInventoryNumber(hdc, static_cast<unsigned int>(sticker.id),
        &idRc, DT_LEFT | DT_SINGLELINE);
'@
$nameInsertReplacement = @'
    DrawInventoryNumber(hdc, static_cast<unsigned int>(sticker.id),
        &idRc, DT_LEFT | DT_SINGLELINE);
    CasUiDrawLabel(hdc, InventorySelectedAttachmentName(), 382, 298, 310, 18,
        CAS_UI_MUTED_2, 9, 400, DT_LEFT);
'@
Replace-Required $nameInsertAnchor $nameInsertReplacement 'localized attachment name'

$slotClickAnchor = @'
        const int slot = (mouseX - 280) / 64;
        if (slot >= 0 && slot < INVENTORY_STICKER_SLOT_COUNT &&
            CasUiPointInRect(mouseX, mouseY, 280 + slot * 64, 202, 54, 28))
            InventoryUiSelectStickerSlot(slot);
'@
$slotClickReplacement = @'
        const int slot = (mouseX - 280) / 64;
        unsigned long long itemId = 0;
        unsigned short slotDefinition = 0;
        const int limit = InventorySelectedItemId(&itemId, &slotDefinition) ?
            InventoryAttachmentSlotLimit(slotDefinition) : 0;
        if (slot >= 0 && slot < limit &&
            CasUiPointInRect(mouseX, mouseY, 280 + slot * 64, 202, 54, 28))
            InventoryUiSelectStickerSlot(slot);
'@
Replace-Required $slotClickAnchor $slotClickReplacement 'Sticker V2 patch slot hit-test'

# Put the keychain modal after the already-injected sticker modal and before the
# Win32 menu proc so all shared UI helpers are in scope.
Insert-BeforeRequired `
    'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)' `
    $ui 'keychain modal UI'

$drawRouteAnchor = '            CasUiDrawStickerModalV2(memDC);'
$drawRouteReplacement = @'
            CasUiDrawStickerModalV2(memDC);
            CasUiDrawKeychainModalV2(memDC);
'@
Replace-Required $drawRouteAnchor $drawRouteReplacement 'keychain modal draw route'

$inputRouteAnchor = @'
        if (g_inventoryStickerModal)
        {
            CasUiHandleStickerModalV2Click(mouseX, mouseY);
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }
        if (CasUiPrepareMenuClick(wnd, &mouseX, &mouseY,
'@
$inputRouteReplacement = @'
        if (g_inventoryKeychainModal)
        {
            CasUiHandleKeychainModalV2Click(mouseX, mouseY);
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }
        if (g_inventoryStickerModal)
        {
            CasUiHandleStickerModalV2Click(mouseX, mouseY);
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }
        if (CasUiPrepareMenuClick(wnd, &mouseX, &mouseY,
'@
Replace-Required $inputRouteAnchor $inputRouteReplacement 'keychain modal input ownership'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Applied real sticker/patch/keychain inventory projection V2: $InputPath"