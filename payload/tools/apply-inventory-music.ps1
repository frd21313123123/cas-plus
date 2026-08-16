param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_music.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Inventory music module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

$shutdownHelper = @'
static void ShutdownInventoryMusic()
{
    if (!g_originalMusicKitState.captured ||
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
        &pawnHandle, &localTeam) && localController)
        RestoreInventoryMusic(localController);
}
'@

$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Inventory music frame-stage anchor was not found. Refusing to patch blindly.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $shutdownHelper + "`r`n`r`n" + $source.Substring($hookIndex)

$slotAnchor = @'
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
'@
$slotReplacement = @'
constexpr unsigned short INVENTORY_SLOT_MUSIC = 0xFFFCu;
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
'@
if (-not $source.Contains($slotAnchor)) {
    throw 'Inventory music slot anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($slotAnchor, $slotReplacement)

# The glove stage already routed UI display names through InventoryCatalogAnyName.
# Promote those final UI calls to the domain-aware helper from this module.
$nameNeedle = 'InventoryCatalogAnyName(item.slotDefinitionIndex, item.overrideDefinitionIndex)'
if (($source.Split($nameNeedle).Count - 1) -lt 2) {
    throw 'Inventory music name anchors were not found. Refusing to patch blindly.'
}
$source = $source.Replace($nameNeedle,
    'InventoryCatalogDomainName(item.slotDefinitionIndex, item.overrideDefinitionIndex)')

$definitionAnchor = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Gloves");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Gloves >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
'@
$definitionReplacement = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Music");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Music >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Gloves");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Gloves >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE)
'@
if (-not $source.Contains($definitionAnchor)) {
    throw 'Inventory music definition-control anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($definitionAnchor, $definitionReplacement)

$paintStatusAnchor = @'
        const wchar_t* paintCatalogName =
            item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ?
                L"stored only - glove attributes pending" :
                InventoryCatalogPaintName(item.paintKit,
                    item.slotDefinitionIndex);
'@
$paintStatusReplacement = @'
        const wchar_t* paintCatalogName =
            item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ?
                L"not used by music kits" :
            item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ?
                L"stored only - glove attributes pending" :
                InventoryCatalogPaintName(item.paintKit,
                    item.slotDefinitionIndex);
'@
if (-not $source.Contains($paintStatusAnchor)) {
    throw 'Inventory music paint-status anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($paintStatusAnchor, $paintStatusReplacement)

$footerAnchor = @'
    DrawInventoryButton(hdc, 35, 425, 64, 30, L"< Pg");
    DrawInventoryButton(hdc, 103, 425, 64, 30, L"Pg >");
    DrawInventoryButton(hdc, 171, 425, 74, 30, L"+ Weapon");
    DrawInventoryButton(hdc, 249, 425, 76, 30, L"Current");
'@
$footerReplacement = @'
    DrawInventoryButton(hdc, 35, 425, 50, 30, L"<Pg");
    DrawInventoryButton(hdc, 89, 425, 50, 30, L"Pg>");
    DrawInventoryButton(hdc, 143, 425, 56, 30, L"+Wpn");
    DrawInventoryButton(hdc, 203, 425, 58, 30, L"Current");
    DrawInventoryButton(hdc, 265, 425, 60, 30, L"+Music");
'@
if (-not $source.Contains($footerAnchor)) {
    throw 'Inventory music footer anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($footerAnchor, $footerReplacement)

$footerClickAnchor = @'
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
$footerClickReplacement = @'
            else if (mouseY >= 420 && mouseY <= 460)
            {
                if (mouseX >= 30 && mouseX < 87)
                    InventoryUiChangePage(-1);
                else if (mouseX >= 87 && mouseX < 141)
                    InventoryUiChangePage(1);
                else if (mouseX >= 141 && mouseX < 201)
                    InventoryUiAddWeaponPreset();
                else if (mouseX >= 201 && mouseX < 263)
                    InventoryUiRequestAddCurrent();
                else if (mouseX >= 263 && mouseX <= 330)
                    InventoryUiAddMusicPreset();
            }
'@
if (-not $source.Contains($footerClickAnchor)) {
    throw 'Inventory music footer-click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($footerClickAnchor, $footerClickReplacement)

$gloveClickAnchor = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleGloveCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleGloveCatalog(1);
            }
'@
$gloveClickReplacement = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleMusicCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleMusicCatalog(1);
            }
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleGloveCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleGloveCatalog(1);
            }
'@
if (-not $source.Contains($gloveClickAnchor)) {
    throw 'Inventory music definition-click anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($gloveClickAnchor, $gloveClickReplacement)

$frameAnchor = @'
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryMusic();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
if (-not $source.Contains($frameAnchor)) {
    throw 'Inventory music frame update anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($frameAnchor, $frameReplacement)

$shutdownAnchor = @'
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
$shutdownReplacement = @'
    ShutdownInventoryMusic();
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
if (-not $source.Contains($shutdownAnchor)) {
    throw 'Inventory music shutdown anchor was not found. Refusing to patch blindly.'
}
$source = $source.Replace($shutdownAnchor, $shutdownReplacement)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected schema-backed music kit adapter: $OutputPath"
