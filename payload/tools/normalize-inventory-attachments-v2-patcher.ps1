param()

$ErrorActionPreference = 'Stop'
$patcher = Join-Path $PSScriptRoot 'apply-inventory-attachments-v2.ps1'
$source = Get-Content -LiteralPath $patcher -Raw -Encoding UTF8

function Replace-ExactOnce([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Attachment patcher normalization '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

$shortCoreInsert = @'
Insert-BeforeRequired 'static void InventoryExtendedOnItemDeleted(unsigned long long itemId)' `
    $core 'attachment catalog/keychain core'
'@
$q = [char]39
$nl = "`r`n"
$preciseCoreInsert = '$coreInsertAnchor = @' + $q + $nl +
    'static void InventoryExtendedOnItemDeleted(unsigned long long itemId)' + $nl +
    '{' + $nl +
    '    if (!itemId || !InventoryStickerTryLock())' + $nl +
    $q + '@' + $nl +
    'Insert-BeforeRequired $coreInsertAnchor $core ' + $q +
    'attachment catalog/keychain core' + $q
Replace-ExactOnce $shortCoreInsert $preciseCoreInsert `
    'definition-vs-forward-declaration core insert'

$ambiguousSlotLoop = @'
Replace-Required `
    '    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)' `
    '    for (int i = 0; i < attachmentLimit; ++i)' `
    'Sticker V2 visible slot count'
'@
$preciseSlotLoop = '$slotLoopAnchor = @' + $q + $nl +
    '    CasUiDrawLabel(hdc, L"SLOTS", 280, 178, 100, 18,' + $nl +
    '        CAS_UI_MUTED_2, 10, 650, DT_LEFT);' + $nl +
    '    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)' + $nl +
    $q + '@' + $nl +
    '$slotLoopReplacement = @' + $q + $nl +
    '    CasUiDrawLabel(hdc, L"SLOTS", 280, 178, 100, 18,' + $nl +
    '        CAS_UI_MUTED_2, 10, 650, DT_LEFT);' + $nl +
    '    for (int i = 0; i < attachmentLimit; ++i)' + $nl +
    $q + '@' + $nl +
    'Replace-Required $slotLoopAnchor $slotLoopReplacement ' + $q +
    'Sticker V2 visible slot count' + $q
Replace-ExactOnce $ambiguousSlotLoop $preciseSlotLoop `
    'unique Sticker V2 slot loop'

# apply-inventory-game-catalog already extended InventoryExtendedLoad, and
# apply-inventory-local-ops already extended the worker flush block. Rewrite the
# Attachment V2 patcher's expected/replacement bodies so it composes with that
# final generated lifecycle rather than reverting either existing layer.
$oldLoadAnchorBody = '    LoadInventoryStickerStore();' + $nl +
    '    InventoryInstallTextCapture();'
$newLoadAnchorBody = '    LoadInventoryStickerStore();' + $nl +
    '    LoadInventoryGameCatalog();' + $nl +
    '    InventoryGameCatalogSanitizeLoadedStore();' + $nl +
    '    InventoryInstallTextCapture();'
Replace-ExactOnce $oldLoadAnchorBody $newLoadAnchorBody `
    'game-catalog-aware attachment load anchor'

$oldLoadReplacementBody = '    LoadInventoryAttachmentCatalog();' + $nl +
    '    LoadInventoryStickerStore();' + $nl +
    '    LoadInventoryKeychainStore();' + $nl +
    '    InventoryInstallTextCapture();'
$newLoadReplacementBody = '    LoadInventoryAttachmentCatalog();' + $nl +
    '    LoadInventoryStickerStore();' + $nl +
    '    LoadInventoryGameCatalog();' + $nl +
    '    InventoryGameCatalogSanitizeLoadedStore();' + $nl +
    '    LoadInventoryKeychainStore();' + $nl +
    '    InventoryInstallTextCapture();'
Replace-ExactOnce $oldLoadReplacementBody $newLoadReplacementBody `
    'game-catalog-preserving attachment load replacement'

$oldFlushAnchorBody = '        FlushInventoryStickerPersistenceIfNeeded();' + $nl +
    '        Sleep(8);'
$newFlushAnchorBody = '        FlushInventoryStickerPersistenceIfNeeded();' + $nl +
    '        FlushInventoryGroupPersistenceIfNeeded();' + $nl +
    '        Sleep(8);'
Replace-ExactOnce $oldFlushAnchorBody $newFlushAnchorBody `
    'group-aware keychain flush anchor'

$oldFlushReplacementBody = '        FlushInventoryStickerPersistenceIfNeeded();' + $nl +
    '        FlushInventoryKeychainPersistenceIfNeeded();' + $nl +
    '        Sleep(8);'
$newFlushReplacementBody = '        FlushInventoryStickerPersistenceIfNeeded();' + $nl +
    '        FlushInventoryKeychainPersistenceIfNeeded();' + $nl +
    '        FlushInventoryGroupPersistenceIfNeeded();' + $nl +
    '        Sleep(8);'
Replace-ExactOnce $oldFlushReplacementBody $newFlushReplacementBody `
    'group-preserving keychain flush replacement'

Set-Content -LiteralPath $patcher -Value $source -Encoding UTF8 -NoNewline
Write-Host 'Normalized Inventory Attachments V2 patcher anchors for current generated source.'
