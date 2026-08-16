param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_local_ops.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Local inventory operations module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    if (-not $source.Contains($Needle)) {
        throw "Local inventory operations anchor was not found: $Name. Refusing to patch blindly."
    }
    $script:source = $source.Replace($Needle, $Replacement)
}

$slotAnchor = 'constexpr unsigned short INVENTORY_SLOT_AGENT = 0xFFFBu;'
$slotReplacement = @'
constexpr unsigned short INVENTORY_SLOT_CONTAINER = 0xFFFAu;
constexpr unsigned short INVENTORY_SLOT_AGENT = 0xFFFBu;
'@
Replace-Required $slotAnchor $slotReplacement.TrimEnd() 'sandbox container slot'

$forwardAnchor = @'
static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static void InventoryExtendedOnItemDeleted(unsigned long long itemId);
static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId);
'@
$forwardReplacement = @'
static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static void InventoryExtendedOnItemDeleted(unsigned long long itemId);
static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId);
static void InventoryAdvancedOnItemDeleted(unsigned long long itemId);
static void InventoryAdvancedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId);
'@
Replace-Required $forwardAnchor $forwardReplacement 'advanced lifecycle declarations'

$sanitizeAnchor = @'
    if (item->slotDefinitionIndex == INVENTORY_SLOT_AGENT)
    {
        if (item->overrideDefinitionIndex < 4500 ||
            item->overrideDefinitionIndex > 5700)
            item->overrideDefinitionIndex = 5200;
        item->paintKit = 0;
        item->seed = 0;
        item->statTrak = -1;
    }
'@
$sanitizeReplacement = @'
    if (item->slotDefinitionIndex == INVENTORY_SLOT_CONTAINER)
    {
        if (item->overrideDefinitionIndex < 4000 ||
            item->overrideDefinitionIndex > 4999)
            item->overrideDefinitionIndex = 4001;
        item->paintKit = 0;
        item->seed = 0;
        item->statTrak = -1;
        item->equippedTeams = INVENTORY_TEAM_NONE;
    }
    else if (item->slotDefinitionIndex == INVENTORY_SLOT_AGENT)
    {
        if (item->overrideDefinitionIndex < 4500 ||
            item->overrideDefinitionIndex > 5700)
            item->overrideDefinitionIndex = 5200;
        item->paintKit = 0;
        item->seed = 0;
        item->statTrak = -1;
    }
'@
Replace-Required $sanitizeAnchor $sanitizeReplacement 'container-aware sanitizer'

$deleteAnchor = @'
        InventoryExtendedOnItemDeleted(
            g_inventoryStore.items[g_inventoryUiSelected].itemId);
        for (int i = g_inventoryUiSelected + 1;
'@
$deleteReplacement = @'
        InventoryExtendedOnItemDeleted(
            g_inventoryStore.items[g_inventoryUiSelected].itemId);
        InventoryAdvancedOnItemDeleted(
            g_inventoryStore.items[g_inventoryUiSelected].itemId);
        for (int i = g_inventoryUiSelected + 1;
'@
Replace-Required $deleteAnchor $deleteReplacement 'group cleanup on delete'

$duplicateAnchor = @'
        InventoryExtendedOnItemDuplicated(sourceItemId, copy.itemId);
        // Duplicates start unequipped so they never silently evict the source
'@
$duplicateReplacement = @'
        InventoryExtendedOnItemDuplicated(sourceItemId, copy.itemId);
        InventoryAdvancedOnItemDuplicated(sourceItemId, copy.itemId);
        // Duplicates start unequipped so they never silently evict the source
'@
Replace-Required $duplicateAnchor $duplicateReplacement 'group copy on duplicate'

$econSkipAnchor = @'
            if (!item.itemId || item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
                continue;
'@
$econSkipReplacement = @'
            if (!item.itemId ||
                item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ||
                item.slotDefinitionIndex == INVENTORY_SLOT_CONTAINER)
                continue;
'@
Replace-Required $econSkipAnchor $econSkipReplacement 'skip local containers in CEcon mirror'

$searchAnchor = @'
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT) domain = "agent";
    return InventoryAsciiContainsInsensitive(domain, g_inventorySearchText);
'@
$searchReplacement = @'
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT) domain = "agent";
    else if (item.slotDefinitionIndex == INVENTORY_SLOT_CONTAINER) domain = "case container";
    return InventoryAsciiContainsInsensitive(domain, g_inventorySearchText);
'@
Replace-Required $searchAnchor $searchReplacement 'container search domain'

# Upgrade the already-patched UI name sites before injecting the final helper,
# so this textual replacement cannot rewrite its own delegation call.
$nameNeedle = 'InventoryCatalogFullDomainName(item.slotDefinitionIndex, item.overrideDefinitionIndex)'
if (($source.Split($nameNeedle).Count - 1) -lt 2) {
    throw 'Final inventory domain-name anchors were not found.'
}
$source = $source.Replace($nameNeedle,
    'InventoryCatalogFinalDomainName(item.slotDefinitionIndex, item.overrideDefinitionIndex)')

$headerAnchor = @'
    DrawInventoryButton(hdc, 330, 62, 108, 26,
        g_inventorySearchLength ? L"Search *" : L"Search");
    DrawInventoryButton(hdc, 445, 62, 100, 26, L"+ Gloves");
'@
$headerReplacement = @'
    DrawInventoryButton(hdc, 330, 62, 108, 26,
        g_inventorySearchLength ? L"Search *" : L"Search");
    DrawInventoryButton(hdc, 445, 62, 100, 26, L"+ Gloves");
    DrawInventoryButton(hdc, 552, 62, 104, 26, L"Local Ops",
        g_inventoryAdvancedModal);
'@
Replace-Required $headerAnchor $headerReplacement 'local operations header button'

$headerClickAnchor = @'
            else if (mouseX >= 440 && mouseX <= 550 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiAddGlovePreset();
            }
            else if (mouseX >= 684 && mouseX <= 735 &&
'@
$headerClickReplacement = @'
            else if (mouseX >= 440 && mouseX <= 550 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiAddGlovePreset();
            }
            else if (mouseX >= 548 && mouseX < 664 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiOpenAdvancedModal();
            }
            else if (mouseX >= 684 && mouseX <= 735 &&
'@
Replace-Required $headerClickAnchor $headerClickReplacement 'local operations header click'

$advancedUi = @'
static void DrawInventoryAdvancedModal(HDC hdc,
    const InventoryUiSnapshot& snapshot)
{
    if (!g_inventoryAdvancedModal)
        return;
    DrawRoundedCard(hdc, 345, 98, 395, 330,
        RGB_COLOR(20, 20, 24), RGB_COLOR(82, 82, 91), 6);
    SetTextColor(hdc, RGB_COLOR(244, 244, 245));
    RECT title = { 360, 112, 620, 134 };
    DrawTextW(hdc, L"LOCAL INVENTORY OPERATIONS", -1, &title,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 674, 108, 50, 24, L"Close");

    DrawInventoryButton(hdc, 360, 150, 112, 30, L"+ Sandbox Case");
    DrawInventoryButton(hdc, 480, 150, 112, 30, L"Open Case",
        snapshot.hasSelectedItem &&
        snapshot.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_CONTAINER);
    SetTextColor(hdc, RGB_COLOR(161, 161, 170));
    RECT caseHint = { 360, 186, 705, 215 };
    DrawTextW(hdc,
        L"Sandbox roll creates only a local virtual item; no GC/Steam mutation.",
        -1, &caseHint, DT_LEFT);

    unsigned short collection = 0;
    unsigned short storage = 0;
    if (snapshot.hasSelectedItem)
        InventoryGroupSnapshot(snapshot.selectedItem.itemId,
            &collection, &storage);

    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT collectionLabel = { 360, 230, 455, 250 };
    DrawTextW(hdc, L"Collection", -1, &collectionLabel,
        DT_LEFT | DT_SINGLELINE);
    RECT collectionValue = { 455, 230, 500, 250 };
    DrawInventoryNumber(hdc, collection, &collectionValue,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 510, 222, 45, 28, L"-");
    DrawInventoryButton(hdc, 560, 222, 45, 28, L"+");
    DrawInventoryButton(hdc, 610, 222, 72, 28, L"Clear");

    RECT storageLabel = { 360, 275, 455, 295 };
    DrawTextW(hdc, L"Storage", -1, &storageLabel,
        DT_LEFT | DT_SINGLELINE);
    RECT storageValue = { 455, 275, 500, 295 };
    DrawInventoryNumber(hdc, storage, &storageValue,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 510, 267, 45, 28, L"-");
    DrawInventoryButton(hdc, 560, 267, 45, 28, L"+");
    DrawInventoryButton(hdc, 610, 267, 72, 28, L"Clear");

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT groupHint = { 360, 318, 705, 355 };
    DrawTextW(hdc,
        L"Collection/storage IDs are persistent local groups keyed by virtual item ID.",
        -1, &groupHint, DT_LEFT);
    RECT opens = { 360, 365, 650, 385 };
    DrawTextW(hdc, L"Sandbox opens:", -1, &opens,
        DT_LEFT | DT_SINGLELINE);
    RECT opensValue = { 475, 365, 540, 385 };
    DrawInventoryNumber(hdc, g_inventorySandboxOpens, &opensValue,
        DT_LEFT | DT_SINGLELINE);
}

'@
$menuProcAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuProcIndex = $source.IndexOf($menuProcAnchor)
if ($menuProcIndex -lt 0) {
    throw 'Advanced inventory MenuWindowProc anchor was not found.'
}
$source = $source.Substring(0, $menuProcIndex) + $advancedUi +
    $source.Substring($menuProcIndex)

$drawAnchor = @'
            DrawInventoryChangerPanel(memDC);
            DrawInventoryStickerModal(memDC, inventoryModalSnapshot);
            SelectObject(memDC, oldFont);
'@
$drawReplacement = @'
            DrawInventoryChangerPanel(memDC);
            DrawInventoryStickerModal(memDC, inventoryModalSnapshot);
            DrawInventoryAdvancedModal(memDC, inventoryModalSnapshot);
            SelectObject(memDC, oldFont);
'@
Replace-Required $drawAnchor $drawReplacement 'advanced operations modal draw'

$modalAnchor = @'
            if (g_inventoryStickerModal)
            {
'@
$modalReplacement = @'
            if (g_inventoryAdvancedModal)
            {
                if (mouseX >= 670 && mouseX <= 730 &&
                    mouseY >= 104 && mouseY <= 136)
                    InventoryUiCloseAdvancedModal();
                else if (mouseY >= 145 && mouseY <= 184)
                {
                    if (mouseX >= 356 && mouseX < 476)
                        InventoryUiAddSandboxCase();
                    else if (mouseX >= 476 && mouseX <= 596)
                        InventoryUiOpenSandboxCase();
                }
                else if (inv.hasSelectedItem &&
                    mouseY >= 218 && mouseY <= 254)
                {
                    if (mouseX >= 506 && mouseX < 558)
                        InventoryUiAdjustGroup(-1, 0, false, false);
                    else if (mouseX >= 558 && mouseX < 608)
                        InventoryUiAdjustGroup(1, 0, false, false);
                    else if (mouseX >= 608 && mouseX <= 686)
                        InventoryUiAdjustGroup(0, 0, true, false);
                }
                else if (inv.hasSelectedItem &&
                    mouseY >= 263 && mouseY <= 299)
                {
                    if (mouseX >= 506 && mouseX < 558)
                        InventoryUiAdjustGroup(0, -1, false, false);
                    else if (mouseX >= 558 && mouseX < 608)
                        InventoryUiAdjustGroup(0, 1, false, false);
                    else if (mouseX >= 608 && mouseX <= 686)
                        InventoryUiAdjustGroup(0, 0, false, true);
                }
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }

            if (g_inventoryStickerModal)
            {
'@
Replace-Required $modalAnchor $modalReplacement 'advanced operations click routing'

$loadAnchor = @'
    LoadInventoryStore();
    InventoryExtendedLoad();
    if (!InstallFrameStageBridge())
'@
$loadReplacement = @'
    LoadInventoryStore();
    InventoryExtendedLoad();
    InventoryAdvancedLoad();
    if (!InstallFrameStageBridge())
'@
Replace-Required $loadAnchor $loadReplacement 'advanced sidecar load'

$flushAnchor = @'
        FlushInventoryPersistenceIfNeeded();
        FlushInventoryStickerPersistenceIfNeeded();
        Sleep(8);
'@
$flushReplacement = @'
        FlushInventoryPersistenceIfNeeded();
        FlushInventoryStickerPersistenceIfNeeded();
        FlushInventoryGroupPersistenceIfNeeded();
        Sleep(8);
'@
Replace-Required $flushAnchor $flushReplacement 'advanced sidecar flush'

$shutdownAnchor = @'
    ShutdownInventoryChanger();
    ShutdownInventoryExtended();
    RemoveFrameStageBridge();
'@
$shutdownReplacement = @'
    ShutdownInventoryChanger();
    ShutdownInventoryAdvanced();
    ShutdownInventoryExtended();
    RemoveFrameStageBridge();
'@
Replace-Required $shutdownAnchor $shutdownReplacement 'advanced sidecar shutdown'

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Advanced inventory module injection anchor was not found.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $source.Substring($hookIndex)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected local sandbox cases and collection/storage grouping: $OutputPath"
