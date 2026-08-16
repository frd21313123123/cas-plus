param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$catalogPath = Join-Path $PSScriptRoot '..\src\inventory\inventory_catalog.inc'
if (-not (Test-Path -LiteralPath $catalogPath)) {
    throw "Inventory catalog module was not found: $catalogPath"
}
$catalog = Get-Content -LiteralPath $catalogPath -Raw -Encoding UTF8

# inventory_changer.inc is already injected by the previous stage. Inject the
# catalog immediately after it and before FrameStageNotifyHook so all catalog
# helpers can reuse the virtual-store primitives while the UI can call them.
$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory catalog frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $catalog + "`r`n`r`n" +
    $source.Substring($hookIndex)

$rowNameAnchor = @'
    RECT defRc = { 82, y + 3, 170, y + 20 };
    DrawTextW(hdc,
        item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ? L"Knife" : L"Weapon",
        -1, &defRc, DT_LEFT | DT_SINGLELINE);
'@
$rowNameReplacement = @'
    RECT defRc = { 82, y + 3, 176, y + 20 };
    DrawTextW(hdc, InventoryCatalogWeaponName(item.overrideDefinitionIndex),
        -1, &defRc, DT_LEFT | DT_SINGLELINE);
'@
if (-not $source.Contains($rowNameAnchor)) {
    throw 'Inventory row catalog-name anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($rowNameAnchor, $rowNameReplacement)

$definitionLabelAnchor = @'
        RECT defLabel = { 360, 168, 455, 188 };
        DrawTextW(hdc, L"Definition", -1, &defLabel,
            DT_LEFT | DT_SINGLELINE);
'@
$definitionLabelReplacement = @'
        RECT defLabel = { 360, 168, 455, 188 };
        DrawTextW(hdc, InventoryCatalogWeaponName(item.overrideDefinitionIndex),
            -1, &defLabel, DT_LEFT | DT_SINGLELINE);
'@
if (-not $source.Contains($definitionLabelAnchor)) {
    throw 'Inventory definition-label anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($definitionLabelAnchor, $definitionLabelReplacement)

$knifeButtonsAnchor = @'
            DrawInventoryButton(hdc, 530, 163, 44, 28, L"Bay", item.overrideDefinitionIndex == 500);
            DrawInventoryButton(hdc, 578, 163, 44, 28, L"Flip", item.overrideDefinitionIndex == 505);
            DrawInventoryButton(hdc, 626, 163, 44, 28, L"Kara", item.overrideDefinitionIndex == 507);
            DrawInventoryButton(hdc, 674, 163, 50, 28, L"Butter", item.overrideDefinitionIndex == 515);
'@
$knifeButtonsReplacement = @'
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Knife");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Knife >");
'@
if (-not $source.Contains($knifeButtonsAnchor)) {
    throw 'Inventory knife catalog-buttons anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($knifeButtonsAnchor, $knifeButtonsReplacement)

$paintButtonsAnchor = @'
        DrawInventoryButton(hdc, 510, 203, 48, 28, L"-100");
        DrawInventoryButton(hdc, 562, 203, 44, 28, L"-1");
        DrawInventoryButton(hdc, 610, 203, 44, 28, L"+1");
        DrawInventoryButton(hdc, 658, 203, 52, 28, L"+100");
'@
$paintButtonsReplacement = @'
        const wchar_t* paintCatalogName = InventoryCatalogPaintName(
            item.paintKit, item.slotDefinitionIndex);
        RECT paintName = { 505, 232, 718, 250 };
        SetTextColor(hdc, RGB_COLOR(113, 113, 122));
        DrawTextW(hdc, paintCatalogName, -1, &paintName,
            DT_RIGHT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 510, 203, 48, 28, L"<Skin");
        DrawInventoryButton(hdc, 562, 203, 44, 28, L"-1");
        DrawInventoryButton(hdc, 610, 203, 44, 28, L"+1");
        DrawInventoryButton(hdc, 658, 203, 52, 28, L"Skin>");
'@
if (-not $source.Contains($paintButtonsAnchor)) {
    throw 'Inventory paint catalog-buttons anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintButtonsAnchor, $paintButtonsReplacement)

$deleteAnchor = '        DrawInventoryButton(hdc, 360, 407, 128, 30, L"Delete item");'
$deleteReplacement = @'
        DrawInventoryButton(hdc, 360, 407, 112, 30, L"Duplicate");
        DrawInventoryButton(hdc, 478, 407, 112, 30, L"Delete item");
'@
if (-not $source.Contains($deleteAnchor)) {
    throw 'Inventory duplicate/delete draw anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($deleteAnchor, $deleteReplacement.TrimEnd())

$knifeClickAnchor = @'
                if (mouseX >= 525 && mouseX < 576)
                    InventoryUiSetDefinition(500);
                else if (mouseX >= 576 && mouseX < 624)
                    InventoryUiSetDefinition(505);
                else if (mouseX >= 624 && mouseX < 672)
                    InventoryUiSetDefinition(507);
                else if (mouseX >= 672 && mouseX <= 730)
                    InventoryUiSetDefinition(515);
'@
$knifeClickReplacement = @'
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleKnifeCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleKnifeCatalog(1);
'@
if (-not $source.Contains($knifeClickAnchor)) {
    throw 'Inventory knife catalog click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($knifeClickAnchor, $knifeClickReplacement)

$paintClickAnchor = @'
                if (mouseX >= 505 && mouseX < 560)
                    InventoryUiAdjustPaint(-100);
                else if (mouseX >= 560 && mouseX < 608)
                    InventoryUiAdjustPaint(-1);
                else if (mouseX >= 608 && mouseX < 656)
                    InventoryUiAdjustPaint(1);
                else if (mouseX >= 656 && mouseX <= 715)
                    InventoryUiAdjustPaint(100);
'@
$paintClickReplacement = @'
                if (mouseX >= 505 && mouseX < 560)
                    InventoryUiCyclePaintCatalog(-1);
                else if (mouseX >= 560 && mouseX < 608)
                    InventoryUiAdjustPaint(-1);
                else if (mouseX >= 608 && mouseX < 656)
                    InventoryUiAdjustPaint(1);
                else if (mouseX >= 656 && mouseX <= 715)
                    InventoryUiCyclePaintCatalog(1);
'@
if (-not $source.Contains($paintClickAnchor)) {
    throw 'Inventory paint catalog click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintClickAnchor, $paintClickReplacement)

$deleteClickAnchor = @'
            else if (inv.hasSelectedItem && mouseX >= 355 && mouseX <= 493 &&
                mouseY >= 400 && mouseY <= 442)
            {
                InventoryUiDeleteSelected();
            }
'@
$deleteClickReplacement = @'
            else if (inv.hasSelectedItem && mouseY >= 400 && mouseY <= 442)
            {
                if (mouseX >= 355 && mouseX < 475)
                    InventoryUiDuplicateSelected();
                else if (mouseX >= 475 && mouseX <= 595)
                    InventoryUiDeleteSelected();
                else
                    return 0;
            }
'@
if (-not $source.Contains($deleteClickAnchor)) {
    throw 'Inventory duplicate/delete click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($deleteClickAnchor, $deleteClickReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected inventory catalog and catalog UI: $OutputPath"
