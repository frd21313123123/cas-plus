param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_gloves.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Inventory glove module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

$shutdownHelper = @'
static void ShutdownInventoryGloves()
{
    if (!g_originalGloveState.captured ||
        !g_preResolvedEntityRuntimeReady || !g_botHighlightRuntimeReady)
        return;
    void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
    if (!entitySystem)
        return;
    void* localController = nullptr;
    void* localPawn = nullptr;
    unsigned int pawnHandle = 0xFFFFFFFFu;
    BYTE localTeam = 0;
    if (InventoryResolveLocal(entitySystem, &localController, &localPawn,
        &pawnHandle, &localTeam) && localPawn)
        RestoreInventoryGloves(localPawn);
}
'@

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory glove frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $shutdownHelper + "`r`n`r`n" + $source.Substring($hookIndex)

# Reserve one virtual-inventory slot domain for gloves. This does not change the
# persisted item struct, so existing v2 inventory files remain binary-compatible.
$slotAnchor = 'constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;'
$slotReplacement = @'
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
'@
if (-not $source.Contains($slotAnchor)) {
    throw 'Inventory glove slot anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($slotAnchor, $slotReplacement.TrimEnd())

# Weapon definitions are deliberately bounded to the ordinary client item range.
# Glove definitions live above 4095, so allow only a narrow glove range for the
# dedicated glove slot while keeping the old fail-closed weapon sanitizer.
$sanitizeAnchor = @'
    if (item->overrideDefinitionIndex == 0 ||
        item->overrideDefinitionIndex > 4095)
        item->overrideDefinitionIndex =
            item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE ?
                42 : item->slotDefinitionIndex;
'@
$sanitizeReplacement = @'
    if (item->slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
    {
        if (item->overrideDefinitionIndex < 4000 ||
            item->overrideDefinitionIndex > 6000)
            item->overrideDefinitionIndex = 5030;
    }
    else if (item->overrideDefinitionIndex == 0 ||
        item->overrideDefinitionIndex > 4095)
    {
        item->overrideDefinitionIndex =
            item->slotDefinitionIndex == INVENTORY_SLOT_KNIFE ?
                42 : item->slotDefinitionIndex;
    }
'@
if (-not $source.Contains($sanitizeAnchor)) {
    throw 'Inventory glove sanitizer anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($sanitizeAnchor, $sanitizeReplacement)

# Catalog UI should name both ordinary weapons/knives and the separate glove
# domain. There are exactly two generated calls at this stage: list row + editor.
$nameNeedle = 'InventoryCatalogWeaponName(item.overrideDefinitionIndex)'
if (($source.Split($nameNeedle).Count - 1) -lt 2) {
    throw 'Inventory glove catalog-name anchors were not found. Refusing to patch blindly.'
}
$source = $source.Replace($nameNeedle,
    'InventoryCatalogAnyName(item.slotDefinitionIndex, item.overrideDefinitionIndex)')

$definitionButtonsAnchor = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Knife");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Knife >");
        }
'@
$definitionButtonsReplacement = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Gloves");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Gloves >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Knife");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Knife >");
        }
'@
if (-not $source.Contains($definitionButtonsAnchor)) {
    throw 'Inventory glove definition-button anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($definitionButtonsAnchor, $definitionButtonsReplacement)

$paintNameAnchor = @'
        const wchar_t* paintCatalogName = InventoryCatalogPaintName(
            item.paintKit, item.slotDefinitionIndex);
'@
$paintNameReplacement = @'
        const wchar_t* paintCatalogName =
            item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ?
                L"stored only - glove attributes pending" :
                InventoryCatalogPaintName(item.paintKit,
                    item.slotDefinitionIndex);
'@
if (-not $source.Contains($paintNameAnchor)) {
    throw 'Inventory glove paint-status anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintNameAnchor, $paintNameReplacement)

# A separate creation path is necessary because gloves are embedded in the pawn
# rather than discoverable as the active weapon.
$headerAnchor = '    DrawToggleSwitch(hdc, 690, 65, snapshot.enabled);'
$headerReplacement = @'
    DrawInventoryButton(hdc, 445, 62, 100, 26, L"+ Gloves");
    DrawToggleSwitch(hdc, 690, 65, snapshot.enabled);
'@
if (-not $source.Contains($headerAnchor)) {
    throw 'Inventory glove Add button anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($headerAnchor, $headerReplacement.TrimEnd())

$toggleClickAnchor = @'
            if (mouseX >= 684 && mouseX <= 735 &&
                mouseY >= 60 && mouseY <= 92)
            {
                InventoryUiToggleEnabled();
            }
'@
$toggleClickReplacement = @'
            if (mouseX >= 440 && mouseX <= 550 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiAddGlovePreset();
            }
            else if (mouseX >= 684 && mouseX <= 735 &&
                mouseY >= 60 && mouseY <= 92)
            {
                InventoryUiToggleEnabled();
            }
'@
if (-not $source.Contains($toggleClickAnchor)) {
    throw 'Inventory glove Add click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($toggleClickAnchor, $toggleClickReplacement)

$knifeClickBlock = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleKnifeCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleKnifeCatalog(1);
            }
'@
$knifeClickReplacement = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleGloveCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleGloveCatalog(1);
            }
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleKnifeCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleKnifeCatalog(1);
            }
'@
if (-not $source.Contains($knifeClickBlock)) {
    throw 'Inventory glove definition click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($knifeClickBlock, $knifeClickReplacement)

# Run after the weapon projection has resolved local/schema state, but before
# optional weapon visual refresh work.
$frameAnchor = @'
        UpdateInventoryChanger();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
if (-not $source.Contains($frameAnchor)) {
    throw 'Inventory glove frame update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($frameAnchor, $frameReplacement)

$shutdownAnchor = @'
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
$shutdownReplacement = @'
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
if (-not $source.Contains($shutdownAnchor)) {
    throw 'Inventory glove shutdown anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($shutdownAnchor, $shutdownReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected schema-backed glove definition adapter: $OutputPath"
