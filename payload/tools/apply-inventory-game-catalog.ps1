param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$catalogPath = Join-Path $PSScriptRoot '..\src\inventory\inventory_game_catalog.inc'
$livePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_game_catalog_live.inc'
if (-not (Test-Path -LiteralPath $catalogPath) -or
    -not (Test-Path -LiteralPath $livePath)) {
    throw 'Game-backed inventory catalog modules were not found.'
}
$catalog = Get-Content -LiteralPath $catalogPath -Raw -Encoding UTF8
$live = Get-Content -LiteralPath $livePath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Game-catalog anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Exact-size catalog storage is allocated only after the loader has generated a
# validated sidecar from the installed game. Avoid adding a multi-megabyte BSS
# array to the manual-mapped payload.
$virtualAnchor = '__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);'
$virtualReplacement = @'
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, DWORD*);
__declspec(dllimport) LPVOID WINAPI VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
__declspec(dllimport) BOOL WINAPI VirtualFree(LPVOID, SIZE_T, DWORD);
'@
Replace-Required $virtualAnchor $virtualReplacement.TrimEnd() 'VirtualAlloc imports'

# Earlier inventory functions call into the game-catalog layer while its body is
# injected immediately before FrameStageNotifyHook.
$forwardAnchor = @'
static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static void InventoryExtendedOnItemDeleted(unsigned long long itemId);
'@
$forwardReplacement = @'
struct VirtualInventoryItem;
static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static bool InventoryGameCatalogReady();
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
    int currentPaint, int direction);
static bool InventoryGameCatalogApplyLiveView(BYTE* itemView,
    const VirtualInventoryItem& item);
static void InventoryExtendedOnItemDeleted(unsigned long long itemId);
'@
Replace-Required $forwardAnchor $forwardReplacement.TrimEnd() 'catalog forward declarations'

# Raw +/- paint IDs were the main source of fake weapon/finish combinations.
# Once the loader-backed catalog exists, both buttons move only between real
# alternate_icons2 pairs for the current weapon. Without a catalog they do
# nothing instead of fabricating IDs.
$rawPaintAnchor = @'
static void InventoryUiAdjustPaint(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item)
    {
        item->paintKit = InventoryClampInt(
            item->paintKit + delta, 0, 100000);
        MarkInventoryDirty();
    }
    InventoryUnlock();
}
'@
$rawPaintReplacement = @'
static void InventoryUiAdjustPaint(int delta)
{
    if (!InventoryTryLock())
        return;
    VirtualInventoryItem* item = InventorySelectedLocked();
    if (item && InventoryGameCatalogReady())
    {
        const int next = InventoryGameCatalogCyclePaint(
            item->overrideDefinitionIndex, item->paintKit, delta);
        if (next != item->paintKit)
        {
            item->paintKit = next;
            MarkInventoryDirty();
        }
    }
    InventoryUnlock();
}
'@
Replace-Required $rawPaintAnchor $rawPaintReplacement.TrimEnd() 'raw paint adjustment removal'

$catalogPaintAnchor = @'
        const int next = InventoryCatalogNextPaint(
            item->slotDefinitionIndex, item->paintKit, direction);
        if (next != item->paintKit)
'@
$catalogPaintReplacement = @'
        const int next = InventoryGameCatalogReady() ?
            InventoryGameCatalogCyclePaint(item->overrideDefinitionIndex,
                item->paintKit, direction) : item->paintKit;
        if (next != item->paintKit)
'@
Replace-Required $catalogPaintAnchor $catalogPaintReplacement.TrimEnd() 'catalog paint cycling'

# CEcon binding can cause the current item view to be rebuilt by the client.
# Re-project the validated paint attributes immediately after binding so the
# live material refresh never sees an empty/vanilla view for that frame.
$liveBindAnchor = @'
    InventoryEconBindItemView(itemView, item.itemId);
    return true;
}

static bool InventoryReadWeaponFallbacks
'@
$liveBindReplacement = @'
    InventoryEconBindItemView(itemView, item.itemId);
    InventoryGameCatalogApplyLiveView(itemView, item);
    return true;
}

static bool InventoryReadWeaponFallbacks
'@
Replace-Required $liveBindAnchor $liveBindReplacement 'immediate live skin attribute projection'

# Inject catalog definitions after all CEcon/view-fallback helpers exist but
# before the frame-stage hook and redesigned menu call them.
$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Game-catalog FrameStage injection anchor was not found.'
}
$module = $catalog + "`r`n`r`n" + $live + "`r`n`r`n"
$source = $source.Substring(0, $hookIndex) + $module + $source.Substring($hookIndex)

# The loader writes the sidecar before injection. Load it after normal inventory
# persistence so old raw-paint records can be normalized once against the current
# game schema.
$loadAnchor = '    LoadInventoryStickerStore();'
$loadReplacement = @'
    LoadInventoryStickerStore();
    LoadInventoryGameCatalog();
    InventoryGameCatalogSanitizeLoadedStore();
'@
Replace-Required $loadAnchor $loadReplacement.TrimEnd() 'catalog startup load'

$shutdownAnchor = '    ShutdownInventoryExtended();'
$shutdownReplacement = @'
    ShutdownInventoryGameCatalog();
    ShutdownInventoryExtended();
'@
Replace-Required $shutdownAnchor $shutdownReplacement.TrimEnd() 'catalog shutdown'

# Inventory home/detail names prefer the localized name produced from the game
# localization file. Baseline names remain only for non-skin domains/fallback.
$nameAnchor = '    const wchar_t* name = CasUiInventoryDomainName(item);'
$nameReplacement = @'
    const wchar_t* gameCatalogName = InventoryGameCatalogDisplayName(item);
    const wchar_t* name = gameCatalogName ? gameCatalogName :
        CasUiInventoryDomainName(item);
'@
# The same line exists in both the item card and V2 detail editor; replace all
# occurrences intentionally, but fail if the expected two migrated sites change.
$nameCount = ([regex]::Matches($source, [regex]::Escape($nameAnchor))).Count
if ($nameCount -ne 2) {
    throw "Game-catalog localized-name anchor expected twice, found $nameCount."
}
$source = $source.Replace($nameAnchor, $nameReplacement.TrimEnd())

# Weapon/knife/glove/agent browse pages now enumerate the loader snapshot rather
# than hardcoded definition arrays. Music remains on its dedicated adapter.
$countStart = $source.IndexOf('static int CasUiCatalogCount(int category)')
$countEnd = $source.IndexOf('static bool CasUiResolveCatalogItem(', $countStart)
if ($countStart -lt 0 -or $countEnd -le $countStart) {
    throw 'CasUiCatalogCount function anchors were not found.'
}
$countBody = @'
static int CasUiCatalogCount(int category)
{
    if (category >= 1 && category <= 9)
        return InventoryGameCatalogCountCategory(category);
    if (category == 10)
        return InventoryMusicCatalogCount();
    return 0;
}

'@
$source = $source.Substring(0, $countStart) + $countBody +
    $source.Substring($countEnd)

$resolveStart = $source.IndexOf('static bool CasUiResolveCatalogItem(')
$resolveEnd = $source.IndexOf('static void CasUiDrawCatalogItemCard(', $resolveStart)
if ($resolveStart -lt 0 -or $resolveEnd -le $resolveStart) {
    throw 'CasUiResolveCatalogItem function anchors were not found.'
}
$resolveBody = @'
static bool CasUiResolveCatalogItem(int category, int ordinal,
    int* kind, int* sourceIndex, const wchar_t** name)
{
    if (!kind || !sourceIndex || !name || ordinal < 0)
        return false;
    *kind = 0;
    *sourceIndex = -1;
    *name = nullptr;

    if (category >= 1 && category <= 9)
    {
        int absolute = -1;
        const InventoryGameCatalogRecord* record =
            InventoryGameCatalogCategoryOrdinal(category, ordinal, &absolute);
        if (!record || absolute < 0)
            return false;
        *kind = 5;
        *sourceIndex = absolute;
        *name = record->displayName[0] ? record->displayName : L"Game item";
        return true;
    }
    if (category == 10 && ordinal < InventoryMusicCatalogCount())
    {
        *kind = 4;
        *sourceIndex = ordinal;
        *name = kInventoryMusicKits[ordinal].displayName;
        return true;
    }
    return false;
}

'@
$source = $source.Substring(0, $resolveStart) + $resolveBody +
    $source.Substring($resolveEnd)

# Add exactly the selected game record. Its definition/paint/team values have
# already been deduplicated and validated against the installed items_game.
$addAnchor = @'
    if (kind == 1 && sourceIndex >= 0 &&
        sourceIndex < InventoryCatalogWeaponCount())
    {
'@
$addReplacement = @'
    if (kind == 5 && sourceIndex >= 0)
    {
        const InventoryGameCatalogRecord* record =
            InventoryGameCatalogAt(static_cast<unsigned int>(sourceIndex));
        if (!record)
        {
            InventoryUnlock();
            return false;
        }
        item.overrideDefinitionIndex = record->definitionIndex;
        item.paintKit = record->paintKit;
        item.equippedTeams = record->teamMask ? record->teamMask : INVENTORY_TEAM_BOTH;
        if (record->category == 9)
        {
            item.slotDefinitionIndex = INVENTORY_SLOT_AGENT;
            item.quality = 4;
            item.paintKit = 0;
        }
        else if (record->category == 8)
        {
            item.slotDefinitionIndex = INVENTORY_SLOT_GLOVE;
            item.quality = 3;
        }
        else if (record->category == 7)
        {
            item.slotDefinitionIndex = INVENTORY_SLOT_KNIFE;
            item.quality = 3;
        }
        else
        {
            item.slotDefinitionIndex = record->definitionIndex;
            item.quality = 0;
        }
    }
    else if (kind == 1 && sourceIndex >= 0 &&
        sourceIndex < InventoryCatalogWeaponCount())
    {
'@
Replace-Required $addAnchor $addReplacement.TrimEnd() 'game-record item creation'

# The detail editor must show the real localized finish instead of the old tiny
# hardcoded paint table. Paint ID remains visible separately for diagnostics.
$paintNameAnchor = @'
    CasUiDrawLabel(hdc,
        InventoryCatalogPaintName(item.paintKit, item.slotDefinitionIndex),
        520, 224, 204, 22, CAS_UI_TEXT, 11, 550, DT_LEFT);
'@
$paintNameReplacement = @'
    const InventoryGameCatalogRecord* selectedGameSkin =
        InventoryGameCatalogForItem(item);
    CasUiDrawLabel(hdc,
        selectedGameSkin && selectedGameSkin->finishName[0] ?
            selectedGameSkin->finishName : L"Vanilla / unavailable",
        520, 224, 204, 22, CAS_UI_TEXT, 11, 550, DT_LEFT);
'@
Replace-Required $paintNameAnchor $paintNameReplacement.TrimEnd() 'real finish name in editor'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Applied game-backed inventory catalog, dedupe and live skin binding: $InputPath"
