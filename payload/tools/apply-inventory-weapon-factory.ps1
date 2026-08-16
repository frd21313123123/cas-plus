param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_weapon_factory.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Inventory weapon factory module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory weapon-factory frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $source.Substring($hookIndex)

$footerAnchor = @'
    DrawInventoryButton(hdc, 35, 425, 92, 30, L"< Page");
    DrawInventoryButton(hdc, 133, 425, 92, 30, L"Page >");
    DrawInventoryButton(hdc, 231, 425, 94, 30, L"Add current");
'@
$footerReplacement = @'
    DrawInventoryButton(hdc, 35, 425, 64, 30, L"< Pg");
    DrawInventoryButton(hdc, 103, 425, 64, 30, L"Pg >");
    DrawInventoryButton(hdc, 171, 425, 74, 30, L"+ Weapon");
    DrawInventoryButton(hdc, 249, 425, 76, 30, L"Current");
'@
if (-not $source.Contains($footerAnchor)) {
    throw 'Inventory weapon-factory footer anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($footerAnchor, $footerReplacement)

$definitionAnchor = @'
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
$definitionReplacement = @'
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
        else if (item.slotDefinitionIndex <= 4095 &&
            InventoryCatalogFindWeapon(item.overrideDefinitionIndex))
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Weapon");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Weapon >");
        }
'@
if (-not $source.Contains($definitionAnchor)) {
    throw 'Inventory weapon-factory definition anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($definitionAnchor, $definitionReplacement)

$footerClickAnchor = @'
            else if (mouseY >= 420 && mouseY <= 460)
            {
                if (mouseX >= 30 && mouseX < 130)
                    InventoryUiChangePage(-1);
                else if (mouseX >= 130 && mouseX < 230)
                    InventoryUiChangePage(1);
                else if (mouseX >= 230 && mouseX <= 330)
                    InventoryUiRequestAddCurrent();
            }
'@
$footerClickReplacement = @'
            else if (mouseY >= 420 && mouseY <= 460)
            {
                if (mouseX >= 30 && mouseX < 101)
                    InventoryUiChangePage(-1);
                else if (mouseX >= 101 && mouseX < 169)
                    InventoryUiChangePage(1);
                else if (mouseX >= 169 && mouseX < 247)
                    InventoryUiAddWeaponPreset();
                else if (mouseX >= 247 && mouseX <= 330)
                    InventoryUiRequestAddCurrent();
            }
'@
if (-not $source.Contains($footerClickAnchor)) {
    throw 'Inventory weapon-factory footer click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($footerClickAnchor, $footerClickReplacement)

$knifeClickAnchor = @'
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
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleKnifeCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleKnifeCatalog(1);
            }
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex <= 4095 &&
                InventoryCatalogFindWeapon(
                    inv.selectedItem.overrideDefinitionIndex))
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleWeaponCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleWeaponCatalog(1);
            }
'@
if (-not $source.Contains($knifeClickAnchor)) {
    throw 'Inventory weapon-factory definition click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($knifeClickAnchor, $knifeClickReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected catalog-driven virtual weapon factory: $OutputPath"
