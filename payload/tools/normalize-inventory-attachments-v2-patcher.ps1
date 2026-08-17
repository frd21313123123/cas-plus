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
$preciseCoreInsert = @'
$coreInsertAnchor = @'
static void InventoryExtendedOnItemDeleted(unsigned long long itemId)
{
    if (!itemId || !InventoryStickerTryLock())
'@
Insert-BeforeRequired $coreInsertAnchor $core 'attachment catalog/keychain core'
'@
Replace-ExactOnce $shortCoreInsert $preciseCoreInsert `
    'definition-vs-forward-declaration core insert'

$ambiguousSlotLoop = @'
Replace-Required `
    '    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)' `
    '    for (int i = 0; i < attachmentLimit; ++i)' `
    'Sticker V2 visible slot count'
'@
$preciseSlotLoop = @'
$slotLoopAnchor = @'
    CasUiDrawLabel(hdc, L"SLOTS", 280, 178, 100, 18,
        CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
'@
$slotLoopReplacement = @'
    CasUiDrawLabel(hdc, L"SLOTS", 280, 178, 100, 18,
        CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < attachmentLimit; ++i)
'@
Replace-Required $slotLoopAnchor $slotLoopReplacement 'Sticker V2 visible slot count'
'@
Replace-ExactOnce $ambiguousSlotLoop $preciseSlotLoop `
    'unique Sticker V2 slot loop'

Set-Content -LiteralPath $patcher -Value $source -Encoding UTF8 -NoNewline
Write-Host 'Normalized Inventory Attachments V2 patcher anchors for current generated source.'
