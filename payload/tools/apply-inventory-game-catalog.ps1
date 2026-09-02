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
__declspec(dllimport) BOOL WINAPI GetProcessTimes(HANDLE, void*, void*, void*, void*);
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
static bool InventoryGameCatalogValidatePair(unsigned short definitionIndex, int paintKit);
static bool InventoryGameCatalogValidateItem(const VirtualInventoryItem& item);
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
    int currentPaint, int direction);
static int InventoryGameCatalogCountForDefinition(unsigned short definitionIndex);
static const struct InventoryGameCatalogRecord* InventoryGameCatalogOrdinalForDefinition(
    unsigned short definitionIndex, int ordinal, int* absoluteIndex);
static const wchar_t* InventoryGameCatalogFinishName(
    const struct InventoryGameCatalogRecord* record);
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
        int next = item->paintKit;
        if (InventoryGameCatalogReady() && InventoryGameCatalogCountForDefinition(item->overrideDefinitionIndex) > 0)
            next = InventoryGameCatalogCyclePaint(item->overrideDefinitionIndex, item->paintKit, direction);
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
# persistence. Old records are preserved; application gates validate exact pairs.
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

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Applied game-backed inventory catalog, dedupe and live skin binding: $InputPath"
