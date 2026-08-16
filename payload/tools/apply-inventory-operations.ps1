param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$operationsPath = Join-Path $PSScriptRoot '..\src\inventory\inventory_operations.inc'
if (-not (Test-Path -LiteralPath $operationsPath)) {
    throw "Inventory operations module was not found: $operationsPath"
}
$operations = Get-Content -LiteralPath $operationsPath -Raw -Encoding UTF8

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory operations frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $operations + "`r`n`r`n" +
    $source.Substring($hookIndex)

$statDrawAnchor = @'
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT stLabel = { 360, 326, 430, 346 };
        DrawTextW(hdc, L"StatTrak", -1, &stLabel,
            DT_LEFT | DT_SINGLELINE);
        DrawToggleSwitch(hdc, 440, 322, item.statTrak >= 0);

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT equipLabel = { 360, 366, 430, 386 };
'@
$statDrawReplacement = @'
        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT stLabel = { 360, 326, 425, 346 };
        DrawTextW(hdc, L"StatTrak", -1, &stLabel,
            DT_LEFT | DT_SINGLELINE);
        if (item.statTrak >= 0)
        {
            RECT stValue = { 425, 326, 480, 346 };
            DrawInventoryNumber(hdc,
                static_cast<unsigned int>(item.statTrak), &stValue,
                DT_LEFT | DT_SINGLELINE);
        }
        else
        {
            SetTextColor(hdc, RGB_COLOR(113, 113, 122));
            RECT stOff = { 425, 326, 480, 346 };
            DrawTextW(hdc, L"Off", -1, &stOff,
                DT_LEFT | DT_SINGLELINE);
        }
        DrawInventoryButton(hdc, 485, 319, 43, 28, L"Off",
            item.statTrak < 0);
        DrawInventoryButton(hdc, 532, 319, 44, 28, L"-100");
        DrawInventoryButton(hdc, 580, 319, 38, 28, L"-1");
        DrawInventoryButton(hdc, 622, 319, 38, 28, L"+1");
        DrawInventoryButton(hdc, 664, 319, 58, 28, L"+100");

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT qualityLabel = { 360, 350, 425, 368 };
        DrawTextW(hdc, L"Quality", -1, &qualityLabel,
            DT_LEFT | DT_SINGLELINE);
        RECT qualityValue = { 425, 350, 535, 368 };
        DrawTextW(hdc, InventoryQualityName(item.quality), -1,
            &qualityValue, DT_LEFT | DT_SINGLELINE);
        DrawInventoryButton(hdc, 542, 347, 82, 24, L"< Quality");
        DrawInventoryButton(hdc, 630, 347, 92, 24, L"Quality >");

        SetTextColor(hdc, RGB_COLOR(228, 228, 231));
        RECT equipLabel = { 360, 376, 430, 396 };
'@
if (-not $source.Contains($statDrawAnchor)) {
    throw 'Inventory StatTrak draw anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($statDrawAnchor, $statDrawReplacement)

# Move equip controls and footer down only a little while staying inside the
# existing editor card.
$source = $source.Replace(
    'DrawInventoryButton(hdc, 435, 359, 58, 28, L"None", item.equippedTeams == INVENTORY_TEAM_NONE);',
    'DrawInventoryButton(hdc, 435, 371, 58, 28, L"None", item.equippedTeams == INVENTORY_TEAM_NONE);')
$source = $source.Replace(
    'DrawInventoryButton(hdc, 497, 359, 58, 28, L"T", item.equippedTeams == INVENTORY_TEAM_T);',
    'DrawInventoryButton(hdc, 497, 371, 58, 28, L"T", item.equippedTeams == INVENTORY_TEAM_T);')
$source = $source.Replace(
    'DrawInventoryButton(hdc, 559, 359, 58, 28, L"CT", item.equippedTeams == INVENTORY_TEAM_CT);',
    'DrawInventoryButton(hdc, 559, 371, 58, 28, L"CT", item.equippedTeams == INVENTORY_TEAM_CT);')
$source = $source.Replace(
    'DrawInventoryButton(hdc, 621, 359, 70, 28, L"T + CT", item.equippedTeams == INVENTORY_TEAM_BOTH);',
    'DrawInventoryButton(hdc, 621, 371, 70, 28, L"T + CT", item.equippedTeams == INVENTORY_TEAM_BOTH);')
$source = $source.Replace(
    'DrawInventoryButton(hdc, 360, 407, 112, 30, L"Duplicate");',
    'DrawInventoryButton(hdc, 360, 411, 112, 26, L"Duplicate");')
$source = $source.Replace(
    'DrawInventoryButton(hdc, 478, 407, 112, 30, L"Delete item");',
    'DrawInventoryButton(hdc, 478, 411, 112, 26, L"Delete item");')

$statClickAnchor = @'
            else if (inv.hasSelectedItem && mouseX >= 430 && mouseX <= 490 &&
                mouseY >= 316 && mouseY <= 352)
            {
                InventoryUiToggleStatTrak();
            }
            else if (inv.hasSelectedItem && mouseY >= 354 && mouseY <= 392)
            {
                if (mouseX >= 430 && mouseX < 495)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_NONE);
                else if (mouseX >= 495 && mouseX < 557)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_T);
                else if (mouseX >= 557 && mouseX < 619)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_CT);
                else if (mouseX >= 619 && mouseX <= 698)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_BOTH);
            }
'@
$statClickReplacement = @'
            else if (inv.hasSelectedItem && mouseY >= 314 && mouseY <= 350)
            {
                if (mouseX >= 480 && mouseX < 531)
                    InventoryUiDisableStatTrak();
                else if (mouseX >= 531 && mouseX < 578)
                    InventoryUiAdjustStatTrak(-100);
                else if (mouseX >= 578 && mouseX < 620)
                    InventoryUiAdjustStatTrak(-1);
                else if (mouseX >= 620 && mouseX < 662)
                    InventoryUiAdjustStatTrak(1);
                else if (mouseX >= 662 && mouseX <= 725)
                    InventoryUiAdjustStatTrak(100);
            }
            else if (inv.hasSelectedItem && mouseY >= 345 && mouseY <= 372)
            {
                if (mouseX >= 538 && mouseX < 626)
                    InventoryUiCycleQuality(-1);
                else if (mouseX >= 626 && mouseX <= 725)
                    InventoryUiCycleQuality(1);
            }
            else if (inv.hasSelectedItem && mouseY >= 370 && mouseY <= 404)
            {
                if (mouseX >= 430 && mouseX < 495)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_NONE);
                else if (mouseX >= 495 && mouseX < 557)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_T);
                else if (mouseX >= 557 && mouseX < 619)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_CT);
                else if (mouseX >= 619 && mouseX <= 698)
                    InventoryUiSetEquipMask(INVENTORY_TEAM_BOTH);
            }
'@
if (-not $source.Contains($statClickAnchor)) {
    throw 'Inventory StatTrak click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($statClickAnchor, $statClickReplacement)

$source = $source.Replace(
    'mouseY >= 400 && mouseY <= 442)',
    'mouseY >= 407 && mouseY <= 442)')

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected virtual inventory operations UI: $OutputPath"
