param([Parameter(Mandatory = $true)][string]$InputPath)
$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$source = $source.Replace("`r`n", "`n")
if ($source.Contains('// CAS_INVENTORY_RUNTIME_STABILITY_V1')) {
    throw 'Inventory runtime stability stage already applied.'
}

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $Needle = $Needle.Replace("`r`n", "`n")
    $Replacement = $Replacement.Replace("`r`n", "`n")
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) { throw "Runtime stability anchor '$Name': expected 1, found $count." }
    $script:source = $script:source.Replace($Needle, $Replacement)
}
function Get-Function([string]$Name) {
    $pattern = '(?ms)^static [^\r\n;{}]*\b' + [regex]::Escape($Name) + '\([^;{}]*\)\r?\n\{.*?^\}'
    $matches = [regex]::Matches($script:source, $pattern)
    if ($matches.Count -ne 1) { throw "Runtime function '$Name': expected 1, found $($matches.Count)." }
    return $matches[0].Value
}
function Set-Function([string]$Name, [string]$Replacement) {
    $original = Get-Function $Name
    $script:source = $script:source.Replace($original, $Replacement.TrimEnd())
}
function Edit-Function([string]$Name, [string]$Needle, [string]$Replacement) {
    $Needle = $Needle.Replace("`r`n", "`n")
    $Replacement = $Replacement.Replace("`r`n", "`n")
    $original = Get-Function $Name
    $count = ([regex]::Matches($original, [regex]::Escape($Needle))).Count
    if ($count -ne 1) { throw "Runtime function '$Name' fragment: expected 1, found $count." }
    Set-Function $Name ($original.Replace($Needle, $Replacement))
}

$declarations = @'
// CAS_INVENTORY_RUNTIME_STABILITY_V1
struct InventoryOriginalViewState {
    unsigned long long itemId;
    unsigned int accountId;
    BYTE disallowSoc;
    BYTE restoreMaterial;
    bool hasIdentity;
    bool hasRestoreMaterial;
    bool hasName;
    char customName[INVENTORY_CUSTOM_NAME_CAPACITY];
};
static bool InventoryGameCatalogValidateItem(const VirtualInventoryItem& item);
static bool InventoryRuntimeValidateItem(const VirtualInventoryItem& item);
static unsigned int InventoryRuntimeAppearanceSignature(const VirtualInventoryItem& item);
static bool InventoryRuntimeTryItemSignature(const VirtualInventoryItem& item, unsigned int* output);
static bool InventoryRuntimeReadEquipped(unsigned short slot, BYTE team,
    VirtualInventoryItem* output, bool* found);
static void InventoryRuntimeCaptureView(BYTE* view, InventoryOriginalViewState* state);
static void InventoryRuntimeRestoreView(BYTE* view, const InventoryOriginalViewState& state);
static bool InventoryRuntimeViewNeedsBind(BYTE* view, unsigned long long itemId);
static bool InventoryRuntimeHasNativeView(BYTE* view);
static BYTE InventoryRuntimeEffectiveTeams(const VirtualInventoryItem& item);
static bool InventoryRuntimeBuildVerifiedReward(VirtualInventoryItem* reward);
static volatile LONG g_inventoryFrameLock = 0;
static volatile LONG g_inventoryStopRequested = 0;
static volatile LONG g_inventoryCleanupComplete = 0;

struct InventoryStore {
'@
Replace-Required 'struct InventoryStore {' $declarations 'runtime declarations'
Replace-Required '    BYTE attachmentDirty;' @'
    BYTE attachmentDirty;
    InventoryOriginalViewState viewState;
    unsigned int appearanceSignature;
    bool appearanceApplied;
    int appearanceCooldown;
    bool wroteViewAttributes;
'@ 'weapon original view state'
Replace-Required 'static OriginalWeaponCosmetics g_originalWeaponCosmetics[INVENTORY_MAX_ORIGINALS]{};' @'
static void InventoryRuntimeRefreshRestoredWeapon(void* entity,
    const OriginalWeaponCosmetics& original, bool definitionChanged);
static OriginalWeaponCosmetics g_originalWeaponCosmetics[INVENTORY_MAX_ORIGINALS]{};
'@ 'restore appearance forward declaration'
Replace-Required "    BYTE initialized;`r`n};" @'
    BYTE initialized;
    InventoryOriginalViewState viewState;
};
'@ 'glove original view state'
Replace-Required 'static unsigned short g_lastProjectedGloveDefinition = 0;' @'
static unsigned short g_lastProjectedGloveDefinition = 0;
static unsigned int g_lastProjectedGloveSignature = 0;
'@ 'glove appearance cache'
Replace-Required '    unsigned int signature;' @'
    unsigned int signature;
    BYTE* appliedView;
    unsigned int appliedViewSignature;
    bool viewApplied;
    unsigned int retryFrame;
    unsigned int equipRetryFrame;
'@ 'econ view cache'

$signature = Get-Function 'InventoryEconItemSignature'
$module = (Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\inventory\inventory_runtime_stability.inc') -Raw -Encoding UTF8).Replace("`r`n", "`n")
$helpers = @'
static InventoryEconMirror* InventoryEconFindMirror(unsigned long long virtualId);
static int InventoryEconSlotForItem(void* inventory, const VirtualInventoryItem& item, int team);
'@
$source = $source.Replace($signature, $helpers + "`n" + $module)

# Snapshot the store once per weapon pass. A UI/persistence lock miss must not
# mark already-projected entities unseen and restore them for one frame.
$enabled = @'
    bool enabled = false;
    if (InventoryTryLock())
    {
        enabled = g_inventoryStore.enabled;
        InventoryUnlock();
    }
    else
        return;
'@
$snapshot = @'
    bool enabled = false;
    VirtualInventoryItem equippedItems[INVENTORY_MAX_ITEMS];
    int equippedCount = 0;
    if (!InventoryTryLock())
        return;
    enabled = g_inventoryStore.enabled;
    equippedCount = InventoryClampInt(g_inventoryStore.itemCount, 0, INVENTORY_MAX_ITEMS);
    for (int i = 0; i < equippedCount; ++i)
        equippedItems[i] = g_inventoryStore.items[i];
    InventoryUnlock();
'@
Edit-Function 'UpdateInventoryChanger' $enabled $snapshot
$selection = @'
        if (InventoryTryLock())
        {
            VirtualInventoryItem* item =
                FindEquippedVirtualItemLocked(definition, teamBit);
            if (item)
            {
                selected = *item;
                hasSelected = true;
            }
            InventoryUnlock();
        }
        else
            continue;
'@
$selectionReplacement = @'
        const unsigned short slot = InventoryNormalizeSlot(definition);
        for (int candidate = 0; candidate < equippedCount; ++candidate)
        {
            const VirtualInventoryItem& item = equippedItems[candidate];
            if (item.slotDefinitionIndex == slot && (InventoryRuntimeEffectiveTeams(item) & teamBit) &&
                InventoryRuntimeValidateItem(item))
            {
                selected = item;
                hasSelected = true;
                break;
            }
        }
'@
Edit-Function 'UpdateInventoryChanger' $selection $selectionReplacement
Edit-Function 'CaptureOriginalWeaponCosmetics' '    original.seen = true;' @'
    original.seen = true;
    InventoryRuntimeCaptureView(itemView, &original.viewState);
'@
Edit-Function 'RestoreOneInventoryEntity' '    return true;' @'
    InventoryRuntimeRefreshRestoredWeapon(entity, original, definitionChanged);
    if (g_inventoryRuntime.hasItemInitialized &&
        IsAccessible(itemView + g_inventoryRuntime.itemInitializedOffset, 1, true))
        *(itemView + g_inventoryRuntime.itemInitializedOffset) = original.itemInitialized;
    InventoryRuntimeRestoreView(itemView, original.viewState);
    return true;
'@
Edit-Function 'RestoreOneInventoryEntity' '    *definition = original.itemDefinitionIndex;' @'
    const bool definitionChanged = *definition != original.itemDefinitionIndex;
    *definition = original.itemDefinitionIndex;
'@
Set-Function 'InventoryRestoreUnseen' @'
static void InventoryRestoreUnseen(void* entitySystem)
{
    int destination = 0;
    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
    {
        OriginalWeaponCosmetics& original = g_originalWeaponCosmetics[i];
        if (!original.seen)
        {
            void* entity = EntityFromHandle(g_preResolvedEntityRuntime,
                entitySystem, original.entityHandle);
            if (!entity || !InventoryOriginalMatchesEntity(original, entity) ||
                RestoreOneInventoryEntity(original, entity))
                continue;
            // A transient inaccessible entity is retried, not forgotten.
        }
        g_originalWeaponCosmetics[destination++] = original;
    }
    g_originalWeaponCosmeticsCount = destination;
}
'@
Set-Function 'RestoreInventoryChanger' @'
static void RestoreInventoryChanger(void* entitySystem)
{
    if (!entitySystem || !g_inventoryRuntime.ready)
        return;
    for (int i = 0; i < g_originalWeaponCosmeticsCount; ++i)
        g_originalWeaponCosmetics[i].seen = false;
    InventoryRestoreUnseen(entitySystem);
    g_inventoryLastAppliedCount = 0;
}
'@
# RestoreInventoryChanger precedes the normal unseen helper definition.
Replace-Required 'static void RestoreInventoryChanger(void* entitySystem)' @'
static void InventoryRestoreUnseen(void* entitySystem);
static void RestoreInventoryChanger(void* entitySystem)
'@ 'restore unseen forward declaration'
Edit-Function 'ApplyVirtualInventoryItem' '    if (!weapon || !g_inventoryRuntime.ready)' @'
    if (!weapon || !g_inventoryRuntime.ready || !InventoryRuntimeValidateItem(source) ||
        !(InventoryRuntimeEffectiveTeams(source) & InventoryTeamBit(g_inventoryActiveTeam)))
'@
Edit-Function 'ApplyVirtualInventoryItem' '    *definition = item.overrideDefinitionIndex;' @'
    OriginalWeaponCosmetics* original = FindOriginalWeaponCosmetics(handle);
    unsigned int appearanceSignature = 0;
    if (!original || !InventoryRuntimeTryItemSignature(item, &appearanceSignature))
        return false;
    if (original->appearanceSignature != appearanceSignature)
    {
        original->appearanceSignature = appearanceSignature;
        original->appearanceApplied = false;
        original->appearanceCooldown = 0;
    }
    const bool appearanceChanged = !original->appearanceApplied ||
        *definition != item.overrideDefinitionIndex || *paintKit != item.paintKit ||
        *seed != item.seed || *wear != item.wear || *statTrak != item.statTrak ||
        InventoryRuntimeViewNeedsBind(itemView, item.itemId);
    if (original->appearanceCooldown > 0)
        --original->appearanceCooldown;
    const bool projectAppearance = appearanceChanged && original->appearanceCooldown == 0;
    *definition = item.overrideDefinitionIndex;
'@
Edit-Function 'ApplyVirtualInventoryItem' '    if (g_inventoryRuntime.hasAttachmentDirty)' '    if (projectAppearance && g_inventoryRuntime.hasAttachmentDirty)'
Edit-Function 'ApplyVirtualInventoryItem' '                    3 : *quality);' '                    3 : 0);'
Edit-Function 'ApplyVirtualInventoryItem' '    InventoryGameCatalogApplyLiveView(itemView, item);' @'
    if (projectAppearance)
    {
        if (InventoryRuntimeHasNativeView(itemView))
            original->appearanceApplied = true;
        else if (InventoryGameCatalogApplyLiveView(itemView, item))
        {
            original->appearanceApplied = true;
            original->wroteViewAttributes = true;
        }
        else
            original->appearanceCooldown = 60;
    }
'@
Edit-Function 'InventoryVisualApplyItemViewState' @'
    if (g_inventoryVisualRefreshRuntime.viewAttributeReady &&
        g_inventoryVisualRefreshRuntime.setViewAttributeByName)
'@ @'
    if (g_inventoryVisualRefreshRuntime.viewAttributeReady &&
        g_inventoryVisualRefreshRuntime.setViewAttributeByName &&
        !InventoryRuntimeHasNativeView(itemView))
'@
Edit-Function 'InventoryVisualApplyItemViewState' '        ++g_inventoryViewAttributeRefreshCalls;' @'
        OriginalWeaponCosmetics* original = FindOriginalWeaponCosmetics(EntityHandleFor(entity));
        if (original && InventoryOriginalMatchesEntity(*original, entity))
            original->wroteViewAttributes = true;
        ++g_inventoryViewAttributeRefreshCalls;
'@
Edit-Function 'UpdateInventoryVisualRefresh' @'
static void UpdateInventoryVisualRefresh()
{
'@ @'
static void UpdateInventoryVisualRefresh()
{
    if (g_originalWeaponCosmeticsCount <= 0)
    {
        g_inventoryVisualRefreshRecordCount = 0;
        return;
    }
'@

foreach ($domain in @('Gloves', 'Music')) {
    $slot = if ($domain -eq 'Gloves') { 'GLOVE' } else { 'MUSIC' }
    $old = @"
    bool storeEnabled = false;
    if (InventoryTryLock())
    {
        storeEnabled = g_inventoryStore.enabled;
        InventoryUnlock();
    }
    else
        return;

    VirtualInventoryItem selected;
    const bool hasSelected = storeEnabled &&
        FindEquippedVirtual${domain}(InventoryTeamBit(localTeam), &selected);
"@
    $new = @"
    VirtualInventoryItem selected{};
    bool hasSelected = false;
    if (!InventoryRuntimeReadEquipped(INVENTORY_SLOT_${slot}, InventoryTeamBit(localTeam),
        &selected, &hasSelected))
        return;
"@
    Edit-Function "UpdateInventory${domain}" $old.TrimEnd() $new.TrimEnd()
}
Edit-Function 'CaptureOriginalGloveState' '    g_originalGloveState.itemIdHigh = *itemIdHigh;' @'
    g_originalGloveState.itemIdHigh = *itemIdHigh;
    InventoryRuntimeCaptureView(view, &g_originalGloveState.viewState);
'@
Edit-Function 'RestoreInventoryGloves' '    MarkGlovesForReapply(localPawn);' @'
    InventoryRuntimeRestoreView(view, g_originalGloveState.viewState);
    MarkGlovesForReapply(localPawn);
'@
Edit-Function 'UpdateInventoryGloves' @'
    if (g_lastProjectedGloveItemId == selected.itemId &&
        g_lastProjectedGloveDefinition == selected.overrideDefinitionIndex &&
        g_lastProjectedGloveTeam == localTeam)
        return;
'@ @'
    const unsigned int appearanceSignature = InventoryRuntimeAppearanceSignature(selected);
    if (g_lastProjectedGloveItemId == selected.itemId &&
        g_lastProjectedGloveDefinition == selected.overrideDefinitionIndex &&
        g_lastProjectedGloveSignature == appearanceSignature &&
        g_lastProjectedGloveTeam == localTeam &&
        !InventoryRuntimeViewNeedsBind(InventoryGloveItemView(localPawn), selected.itemId))
        return;
'@
Edit-Function 'UpdateInventoryGloves' '    InventoryEconBindItemView(view, selected.itemId);' @'
    InventoryEconBindItemView(view, selected.itemId);
    if (!InventoryRuntimeHasNativeView(view))
        InventoryGameCatalogApplyLiveView(view, selected);
    g_lastProjectedGloveSignature = appearanceSignature;
'@

# Identity/material flags are event-driven. The live projection still repairs
# cheap ID fields if the game refreshes them; no unconditional material rebuild.
Edit-Function 'InventoryEconBindItemView' @'
    *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset) = id;
'@ @'
    const bool bindingChanged = *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset) != id;
    *reinterpret_cast<unsigned long long*>(view +
        g_inventoryEconRuntime.viewItemIdOffset) = id;
'@
Edit-Function 'InventoryEconBindItemView' '    if (g_inventoryEconRuntime.viewRestoreMaterialOffset &&' '    if (bindingChanged && g_inventoryEconRuntime.viewRestoreMaterialOffset &&'

# Do not allow busy sticker state to leave uninitialized stack bytes in runtime
# attributes. Missing records and busy records have deliberately different paths.
Edit-Function 'InventoryEconApplyViewAttributes' '    else if (item.paintKit > 0)' '    else if (item.slotDefinitionIndex != INVENTORY_SLOT_AGENT)'
Edit-Function 'InventoryEconApplyViewAttributes' '                *quality = item.quality;' @'
                *quality = item.quality ? item.quality :
                    (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ||
                     item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ||
                     item.slotDefinitionIndex == INVENTORY_SLOT_AGENT ? 3 : 0);
'@
foreach ($function in @('InventoryEconApplyAttributes', 'InventoryEconApplyViewAttributes')) {
    $body = Get-Function $function
    $needle = 'InventoryStickerCopyRecord(item.itemId, &stickers);'
    $replacement = @'
if (!InventoryStickerTryLock())
            return false;
        InventoryStickerRecord* stickerSource = InventoryStickerFindLocked(item.itemId, false);
        ZeroBytes(&stickers, sizeof(stickers));
        if (stickerSource)
            stickers = *stickerSource;
        InventoryStickerUnlock();
'@
    Edit-Function $function $needle $replacement.TrimEnd()
}
Edit-Function 'InventoryAttachmentApplyKeychainView' '    InventoryKeychainCopy(item.itemId, &state);' @'
    if (!InventoryKeychainTryLock())
        return false;
    InventoryKeychainRecord* source = InventoryKeychainFindLocked(item.itemId, false);
    if (source)
        state = source->state;
    InventoryKeychainUnlock();
'@

# Keep lifecycle destruction conditional on actual cache removal. If a runtime
# disappears, retain/retry rather than freeing memory still owned by the cache.
Set-Function 'InventoryEconDestroyMirror' @'
static bool InventoryEconDestroyMirror(InventoryEconMirror* mirror,
    bool inventoryKnownValid)
{
    if (!mirror || !mirror->active)
        return true;
    if (!inventoryKnownValid)
    {
        ZeroBytes(mirror, sizeof(*mirror));
        return true;
    }
    InventorySOID owner{};
    void* current = nullptr;
    if (!InventoryEconResolveInventory(&current, &owner) || current != mirror->inventory)
        return false;
    void* typeCache = InventoryEconTypeCache(current);
    void* removeObject = InventoryEconVtableFunction(typeCache, 3);
    void* destructor = InventoryEconVtableFunction(mirror->object, 1);
    if (!removeObject || !destructor || !InventoryEconVtableFunction(current, 2) ||
        !InventoryRuntimeRestoreLoadout(mirror->econItemId, current))
        return false;
    InventoryEconNotify(current, 2, owner, mirror->object);
    using RemoveFn = void* (*)(void*, void*);
    if (reinterpret_cast<RemoveFn>(removeObject)(typeCache, mirror->object) != mirror->object)
        return false;
    reinterpret_cast<void (*)(void*, bool)>(destructor)(mirror->object, true);
    ++g_inventoryEconRemoves;
    ZeroBytes(mirror, sizeof(*mirror));
    return true;
}
'@
Set-Function 'InventoryEconRemoveMirrorAt' @'
static bool InventoryEconRemoveMirrorAt(int index, bool validInventory)
{
    if (index < 0 || index >= g_inventoryEconMirrorCount)
        return false;
    if (!InventoryEconDestroyMirror(&g_inventoryEconMirrors[index], validInventory))
        return false;
    for (int i = index + 1; i < g_inventoryEconMirrorCount; ++i)
        g_inventoryEconMirrors[i - 1] = g_inventoryEconMirrors[i];
    --g_inventoryEconMirrorCount;
    ZeroBytes(&g_inventoryEconMirrors[g_inventoryEconMirrorCount], sizeof(InventoryEconMirror));
    return true;
}
'@
Edit-Function 'InventoryEconCreateMirror' '        !g_inventoryEconRuntime.sharedReady)' '        !g_inventoryEconRuntime.sharedReady || !InventoryRuntimeValidateItem(item))'
Edit-Function 'InventoryEconCreateMirror' '    if (true)' @'
    // Resolve rollback operations before transferring ownership to the cache.
    void* rollbackCache = InventoryEconTypeCache(inventory);
    if (!InventoryEconVtableFunction(rollbackCache, 1) ||
        !InventoryEconVtableFunction(rollbackCache, 3) ||
        !InventoryEconVtableFunction(inventory, 0) ||
        !InventoryEconVtableFunction(inventory, 2))
        return false;
    if (true)
'@
Edit-Function 'InventoryEconCreateMirror' @'
        if (!object || !InventoryEconVtableFunction(object, 1) ||
            !IsAccessible(reinterpret_cast<BYTE*>(object) + 0x30,
                sizeof(unsigned short), true))
            return false;
'@ @'
        if (!object)
            return false;
        void* objectDestructor = InventoryEconVtableFunction(object, 1);
        if (!objectDestructor)
            return false;
        if (!IsAccessible(reinterpret_cast<BYTE*>(object) + 0x10, 0x24, true))
        {
            reinterpret_cast<void (*)(void*, bool)>(objectDestructor)(object, true);
            return false;
        }
'@
Edit-Function 'InventoryEconCreateMirror' @'
        mirror.teams = item.equippedTeams;
        InventoryEconEquipMirror(mirror, item);
'@ @'
        mirror.teams = INVENTORY_TEAM_NONE;
        InventoryRuntimeEquipMirror(&mirror, item);
'@
# SOCreated notification target was validated above; if it disappears before
# notification, never destroy a still-cache-owned object on a failed rollback.
Edit-Function 'InventoryEconCreateMirror' @'
            if (removeObject)
                reinterpret_cast<void* (*)(void*, void*)>(removeObject)(typeCache, object);
            void* destructor = InventoryEconVtableFunction(object, 1);
'@ @'
            if (!removeObject || reinterpret_cast<void* (*)(void*, void*)>(
                removeObject)(typeCache, object) != object)
                return false;
            void* destructor = InventoryEconVtableFunction(object, 1);
'@

Edit-Function 'UpdateInventoryEconBackend' '            if (!item.itemId)' '            if (!InventoryRuntimeValidateItem(item))'
Edit-Function 'UpdateInventoryEconBackend' '            const VirtualInventoryItem& item = items[i];' @'
            VirtualInventoryItem item = items[i];
            item.equippedTeams = InventoryRuntimeEffectiveTeams(item);
'@
Edit-Function 'UpdateInventoryEconBackend' '    g_inventoryEconDiagInventoryReady = false;' @'
    ++g_inventoryRuntimeFrame;
    g_inventoryEconDiagInventoryReady = false;
'@
Edit-Function 'UpdateInventoryEconBackend' @'
            const unsigned int signature = InventoryEconItemSignature(item);
            InventoryEconMirror* mirror = InventoryEconFindMirror(item.itemId);
'@ @'
            InventoryEconMirror* mirror = InventoryEconFindMirror(item.itemId);
            unsigned int signature = 0;
            if (!InventoryRuntimeTryItemSignature(item, &signature))
            {
                if (mirror)
                    mirror->seen = true;
                continue;
            }
'@
Edit-Function 'UpdateInventoryEconBackend' '                InventoryEconRemoveMirrorAt(index, mirror->inventory == inventory);' @'
                if (!InventoryEconRemoveMirrorAt(index, mirror->inventory == inventory))
                {
                    mirror->seen = true;
                    continue;
                }
'@
Edit-Function 'UpdateInventoryEconBackend' @'
                if (inventoryView)
                    InventoryEconApplyViewAttributes(inventoryView, item);
'@ @'
                if (inventoryView && (!mirror->viewApplied || mirror->appliedView != inventoryView ||
                    mirror->appliedViewSignature != signature) &&
                    InventoryRuntimeRetryDue(mirror->retryFrame))
                {
                    if (InventoryEconApplyViewAttributes(inventoryView, item))
                    {
                        mirror->appliedView = inventoryView;
                        mirror->appliedViewSignature = signature;
                        mirror->viewApplied = true;
                    }
                    else
                        mirror->retryFrame = g_inventoryRuntimeFrame + 60;
                }
'@
Edit-Function 'UpdateInventoryEconBackend' @'
            if (!mirror)
            {
                if (!InventoryEconCreateMirror(item, inventory, owner, signature))
'@ @'
            if (!mirror)
            {
                InventoryRuntimeRetry* retry = InventoryRuntimeRetryFor(item.itemId);
                if (retry && retry->signature == signature && !InventoryRuntimeRetryDue(retry->nextFrame))
                    continue;
                if (retry)
                {
                    retry->signature = signature;
                    retry->nextFrame = g_inventoryRuntimeFrame + 60;
                }
                if (!InventoryEconCreateMirror(item, inventory, owner, signature))
'@
Edit-Function 'UpdateInventoryEconBackend' '                if (mirror->teams != item.equippedTeams)' @'
                if (mirror->teams != item.equippedTeams &&
                    InventoryRuntimeRetryDue(mirror->equipRetryFrame))
'@
Edit-Function 'UpdateInventoryEconBackend' @'
                    mirror->teams = item.equippedTeams;
                    InventoryEconEquipMirror(*mirror, item);
'@ @'
                    InventoryRuntimeEquipMirror(mirror, item);
                    mirror->equipRetryFrame = mirror->teams == item.equippedTeams ? 0 :
                        g_inventoryRuntimeFrame + 60;
'@
Edit-Function 'UpdateInventoryEconBackend' '        g_inventoryEconMirrorCount = 0;' @'
        g_inventoryEconMirrorCount = 0;
        ZeroBytes(g_inventoryOriginalLoadout, sizeof(g_inventoryOriginalLoadout));
'@
Set-Function 'ShutdownInventoryChanger' @'
static void ShutdownInventoryChanger()
{
    // Persistence is independent of successful schema/runtime initialization.
    FlushInventoryPersistenceIfNeeded();
    if (g_inventoryRuntime.ready && g_preResolvedEntityRuntimeReady)
    {
        void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
        if (entitySystem)
            RestoreInventoryChanger(entitySystem);
    }
}
'@

# Shutdown is cooperative: inventory mutations/restore run on the same game
# callback as normal projection. The worker only frees read-only buffers after
# acquiring the inventory frame guard. A stalled callback must not cause UAF.
$frameHelper = @'
static BYTE InventoryRuntimeEffectiveTeams(const VirtualInventoryItem& item)
{
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        return item.equippedTeams & INVENTORY_TEAM_BOTH;
    const InventoryGameCatalogRecord* record = InventoryGameCatalogForItem(item);
    return record ? static_cast<BYTE>(item.equippedTeams & record->teamMask &
        INVENTORY_TEAM_BOTH) : static_cast<BYTE>(INVENTORY_TEAM_NONE);
}

static bool InventoryRuntimeBuildVerifiedReward(VirtualInventoryItem* reward)
{
    if (!reward || !InventoryGameCatalogReady())
        return false;
    // Uniform sandbox sampling from verified weapon/knife pairs only. These
    // are not real case contents and do not claim Valve drop probabilities.
    const InventoryGameCatalogRecord* selected = nullptr;
    unsigned int eligible = 0;
    for (unsigned int i = 0; i < InventoryGameCatalogTotalCount(); ++i)
    {
        const InventoryGameCatalogRecord* record = InventoryGameCatalogAt(i);
        if (!record || record->category < 1 || record->category > 7)
            continue;
        if (InventorySandboxNextRandom() % ++eligible == 0)
            selected = record;
    }
    if (!selected)
        return false;
    ZeroBytes(reward, sizeof(*reward));
    reward->overrideDefinitionIndex = selected->definitionIndex;
    reward->slotDefinitionIndex = selected->category == 7 ?
        INVENTORY_SLOT_KNIFE : selected->definitionIndex;
    reward->paintKit = selected->paintKit;
    reward->quality = selected->category == 7 ? 3 : 0;
    reward->seed = static_cast<int>(InventorySandboxNextRandom() % 1001u);
    reward->wear = 0.0001f;
    reward->statTrak = -1;
    reward->equippedTeams = INVENTORY_TEAM_NONE;
    return InventoryGameCatalogValidateItem(*reward);
}

static bool InventoryRuntimeCleanupFinished()
{
    return g_originalWeaponCosmeticsCount == 0 &&
        !g_originalGloveState.captured &&
        !g_originalMusicKitState.captured &&
        g_inventoryEconMirrorCount == 0;
}

static void InventoryRuntimeCleanupOnGameThread()
{
    ShutdownInventoryMusic();
    ShutdownInventoryGloves();
    if (g_inventoryRuntime.ready && g_preResolvedEntityRuntimeReady)
    {
        void* entitySystem = CurrentEntitySystem(g_preResolvedEntityRuntime);
        if (entitySystem)
            RestoreInventoryChanger(entitySystem);
    }
    ResetInventoryVisualRefresh();
    if (g_inventoryEconLastInventory)
        for (int i = g_inventoryEconMirrorCount - 1; i >= 0; --i)
            InventoryEconRemoveMirrorAt(i, true);
    // A transient entity/cache failure is retried on a later render pass. Do
    // not let the worker tear down catalog/runtime state while any projection
    // still needs game-thread restoration.
    if (InventoryRuntimeCleanupFinished())
        AtomicExchange(&g_inventoryCleanupComplete, 1);
}

static bool InventoryRuntimeStopAndDrain()
{
    AtomicExchange(&g_inventoryStopRequested, 1);
    bool cleanupComplete = false;
    for (int attempt = 0; attempt < 250; ++attempt)
    {
        if (_InterlockedCompareExchange(&g_inventoryCleanupComplete, 0, 0))
        {
            cleanupComplete = true;
            break;
        }
        Sleep(4);
    }
    if (!cleanupComplete)
        return false;
    // Retain the guard permanently once quiescent. If no render pass arrives,
    // cleanupComplete stays false and game-owned/runtime state is retained.
    for (int attempt = 0; attempt < 250; ++attempt)
    {
        if (_InterlockedCompareExchange(&g_inventoryFrameLock, 1, 0) == 0)
        {
            // The cleanup flag is written while holding this same guard. Check
            // it again after acquisition before permitting worker teardown.
            if (_InterlockedCompareExchange(&g_inventoryCleanupComplete, 0, 0))
                return true;
            AtomicExchange(&g_inventoryFrameLock, 0);
            return false;
        }
        Sleep(4);
    }
    return false;
}

static void FrameStageNotifyHook(void* client, int stage)
{
'@
Replace-Required @'
static void FrameStageNotifyHook(void* client, int stage)
{
'@ $frameHelper 'inventory cooperative shutdown helpers'
Set-Function 'InventoryBuildSandboxReward' @'
static bool InventoryBuildSandboxReward(VirtualInventoryItem* reward)
{
    return InventoryRuntimeBuildVerifiedReward(reward);
}
'@
Edit-Function 'FrameStageNotifyHook' @'
        UpdateInventoryEconBackend();
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryMusic();
        UpdateInventoryVisualRefresh();
'@ @'
        if (_InterlockedCompareExchange(&g_inventoryFrameLock, 1, 0) == 0)
        {
            if (_InterlockedCompareExchange(&g_inventoryStopRequested, 0, 0))
            {
                if (!_InterlockedCompareExchange(&g_inventoryCleanupComplete, 0, 0))
                    InventoryRuntimeCleanupOnGameThread();
            }
            else
            {
                UpdateInventoryEconBackend();
                UpdateInventoryChanger();
                UpdateInventoryGloves();
                UpdateInventoryMusic();
                UpdateInventoryVisualRefresh();
            }
            AtomicExchange(&g_inventoryFrameLock, 0);
        }
'@
Edit-Function 'ShutdownInventoryExtended' @'
    if (g_inventoryEconLastInventory)
    {
        for (int i = g_inventoryEconMirrorCount - 1; i >= 0; --i)
            InventoryEconRemoveMirrorAt(i, true);
    }
'@ @'
    // Game-owned objects were detached by the game-thread cleanup callback.
    // On timeout, deliberately retain them rather than call client code here.
'@
Replace-Required @'
    ShutdownInventoryMusic();
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    ShutdownInventoryAdvanced();
    ShutdownInventoryGameCatalog();
    ShutdownInventoryExtended();
    RemoveFrameStageBridge();
'@ @'
    const bool inventoryDrained = InventoryRuntimeStopAndDrain();
    FlushInventoryPersistenceIfNeeded();
    FlushInventoryStickerPersistenceIfNeeded();
    FlushInventoryKeychainPersistenceIfNeeded();
    FlushInventoryGroupPersistenceIfNeeded();
    InventoryRemoveTextCapture();
    if (inventoryDrained)
    {
        ShutdownInventoryAdvanced();
        ShutdownInventoryGameCatalog();
        ShutdownInventoryExtended();
    }
    RemoveFrameStageBridge();
'@ 'drained worker cleanup'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Applied inventory snapshot, lifecycle and change-driven projection fixes: $InputPath"
