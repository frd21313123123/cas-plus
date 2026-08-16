param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\inventory\inventory_full.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Extended inventory module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    if (-not $source.Contains($Needle)) {
        throw "Extended inventory anchor was not found: $Name. Refusing to patch blindly."
    }
    $script:source = $source.Replace($Needle, $Replacement)
}

# The base payload intentionally imports only the Win32 surface it uses. Text
# editing subclasses the game window only while an Inventory text field is
# active, so add the two reversible WndProc helpers explicitly.
$importAnchor = '__declspec(dllimport) LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);'
$importReplacement = @'
__declspec(dllimport) LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) LONG_PTR WINAPI SetWindowLongPtrW(HWND, int, LONG_PTR);
__declspec(dllimport) LRESULT WINAPI CallWindowProcW(WNDPROC, HWND, UINT, WPARAM, LPARAM);
'@
Replace-Required $importAnchor $importReplacement.TrimEnd() 'Win32 WndProc imports'

# The final pre-existing domain constants (after music/glove patchers) leave one
# reserved value for agents. Keep the ordering stable so v2 stored item records
# continue to decode unchanged.
$slotAnchor = @'
constexpr unsigned short INVENTORY_SLOT_MUSIC = 0xFFFCu;
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
'@
$slotReplacement = @'
constexpr unsigned short INVENTORY_SLOT_AGENT = 0xFFFBu;
constexpr unsigned short INVENTORY_SLOT_MUSIC = 0xFFFCu;
constexpr unsigned short INVENTORY_SLOT_GLOVE = 0xFFFDu;
constexpr unsigned short INVENTORY_SLOT_KNIFE = 0xFFFEu;
'@
Replace-Required $slotAnchor $slotReplacement 'agent slot domain'

# Calls patched into already-defined inventory helpers need declarations before
# inventory_changer.inc's function bodies. Implementations are injected later,
# immediately before FrameStageNotifyHook.
$forwardAnchor = 'enum InventoryTeamMask : BYTE {'
$forwardReplacement = @'
static bool InventoryEconBindItemView(BYTE* view, unsigned long long virtualItemId);
static void InventoryExtendedOnItemDeleted(unsigned long long itemId);
static void InventoryExtendedOnItemDuplicated(unsigned long long sourceId,
    unsigned long long newId);

enum InventoryTeamMask : BYTE {
'@
Replace-Required $forwardAnchor $forwardReplacement.TrimEnd() 'extended forward declarations'

# Existing sanitizer only knows the glove exception above 4095. Agent item
# definitions are also in the 4xxx/5xxx range; treat that domain explicitly.
$sanitizeAnchor = @'
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
$sanitizeReplacement = @'
    if (item->slotDefinitionIndex == INVENTORY_SLOT_AGENT)
    {
        if (item->overrideDefinitionIndex < 4500 ||
            item->overrideDefinitionIndex > 5700)
            item->overrideDefinitionIndex = 5200;
        item->paintKit = 0;
        item->seed = 0;
        item->statTrak = -1;
    }
    else if (item->slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
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
Replace-Required $sanitizeAnchor $sanitizeReplacement 'agent-aware sanitizer'

# Emptying a local custom name must clear a previously projected name instead
# of leaving stale bytes in the current weapon view.
$customNameAnchor = @'
    if (g_inventoryRuntime.hasCustomName &&
        item.customNameLength > 0)
    {
        char* customName = reinterpret_cast<char*>(
            itemView + g_inventoryRuntime.customNameOffset);
        if (IsAccessible(customName,
            INVENTORY_CUSTOM_NAME_CAPACITY, true))
        {
            int i = 0;
            for (; i < item.customNameLength &&
                i + 1 < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
                customName[i] = item.customName[i];
            customName[i] = 0;
        }
    }
'@
$customNameReplacement = @'
    if (g_inventoryRuntime.hasCustomName)
    {
        char* customName = reinterpret_cast<char*>(
            itemView + g_inventoryRuntime.customNameOffset);
        if (IsAccessible(customName,
            INVENTORY_CUSTOM_NAME_CAPACITY, true))
        {
            int i = 0;
            for (; i < item.customNameLength &&
                i + 1 < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
                customName[i] = item.customName[i];
            customName[i] = 0;
        }
    }
'@
Replace-Required $customNameAnchor $customNameReplacement 'custom-name clear path'

# Bind projected live weapon views to the validated local CEconItem mirror when
# available. If the mirror backend is unavailable the original direct projection
# remains the fallback and this function simply returns false.
$weaponBindAnchor = @'
    if (g_inventoryRuntime.hasAttachmentDirty)
    {
        BYTE* dirty = base + g_inventoryRuntime.attachmentDirtyOffset;
        if (IsAccessible(dirty, sizeof(BYTE), true))
            *dirty = 1;
    }
    return true;
}

static bool InventoryReadWeaponFallbacks
'@
$weaponBindReplacement = @'
    if (g_inventoryRuntime.hasAttachmentDirty)
    {
        BYTE* dirty = base + g_inventoryRuntime.attachmentDirtyOffset;
        if (IsAccessible(dirty, sizeof(BYTE), true))
            *dirty = 1;
    }
    InventoryEconBindItemView(itemView, item.itemId);
    return true;
}

static bool InventoryReadWeaponFallbacks
'@
Replace-Required $weaponBindAnchor $weaponBindReplacement 'weapon CEconItem view binding'

# Keep sticker sidecar state in lockstep with the base v2 inventory lifecycle.
$deleteAnchor = @'
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
    {
        for (int i = g_inventoryUiSelected + 1;
'@
$deleteReplacement = @'
    if (g_inventoryUiSelected >= 0 &&
        g_inventoryUiSelected < g_inventoryStore.itemCount)
    {
        InventoryExtendedOnItemDeleted(
            g_inventoryStore.items[g_inventoryUiSelected].itemId);
        for (int i = g_inventoryUiSelected + 1;
'@
Replace-Required $deleteAnchor $deleteReplacement 'sticker cleanup on item delete'

$duplicateAnchor = @'
        VirtualInventoryItem copy = *selected;
        copy.itemId = g_inventoryStore.nextItemId++;
        // Duplicates start unequipped so they never silently evict the source
'@
$duplicateReplacement = @'
        VirtualInventoryItem copy = *selected;
        const unsigned long long sourceItemId = copy.itemId;
        copy.itemId = g_inventoryStore.nextItemId++;
        InventoryExtendedOnItemDuplicated(sourceItemId, copy.itemId);
        // Duplicates start unequipped so they never silently evict the source
'@
Replace-Required $duplicateAnchor $duplicateReplacement 'sticker copy on item duplicate'

# Current glove lifecycle has a flag and a change counter. Resolve the counter
# when present and advance both halves of the client handshake.
$gloveStructAnchor = @'
struct InventoryGloveRuntime {
    bool ready;
    unsigned int needReapplyOffset;
    unsigned int econGlovesOffset;
    unsigned int pawnSize;
    unsigned int econItemViewSize;
};
'@
$gloveStructReplacement = @'
struct InventoryGloveRuntime {
    bool ready;
    bool hasChangeCounter;
    unsigned int needReapplyOffset;
    unsigned int changeCounterOffset;
    unsigned int econGlovesOffset;
    unsigned int pawnSize;
    unsigned int econItemViewSize;
};
'@
Replace-Required $gloveStructAnchor $gloveStructReplacement 'glove change-counter runtime'

$gloveResolveAnchor = @'
    unsigned int needReapply = 0;
    unsigned int econGloves = 0;
    if (!FindSchemaField(pawn, "m_bNeedToReApplyGloves", &needReapply) ||
        !FindSchemaField(pawn, "m_EconGloves", &econGloves))
        return false;

    if (needReapply + sizeof(BYTE) > static_cast<unsigned int>(pawn->size) ||
        econGloves + static_cast<unsigned int>(econItemView->size) >
            static_cast<unsigned int>(pawn->size))
        return false;

    runtime->needReapplyOffset = needReapply;
    runtime->econGlovesOffset = econGloves;
'@
$gloveResolveReplacement = @'
    unsigned int needReapply = 0;
    unsigned int changeCounter = 0;
    unsigned int econGloves = 0;
    if (!FindSchemaField(pawn, "m_bNeedToReApplyGloves", &needReapply) ||
        !FindSchemaField(pawn, "m_EconGloves", &econGloves))
        return false;
    const bool hasChangeCounter = FindSchemaField(pawn,
        "m_nEconGlovesChanged", &changeCounter) &&
        changeCounter + sizeof(BYTE) <= static_cast<unsigned int>(pawn->size);

    if (needReapply + sizeof(BYTE) > static_cast<unsigned int>(pawn->size) ||
        econGloves + static_cast<unsigned int>(econItemView->size) >
            static_cast<unsigned int>(pawn->size))
        return false;

    runtime->hasChangeCounter = hasChangeCounter;
    runtime->needReapplyOffset = needReapply;
    runtime->changeCounterOffset = changeCounter;
    runtime->econGlovesOffset = econGloves;
'@
Replace-Required $gloveResolveAnchor $gloveResolveReplacement 'glove change-counter schema resolve'

$markGloveAnchor = @'
static void MarkGlovesForReapply(void* localPawn)
{
    if (!localPawn || !g_inventoryGloveRuntime.ready)
        return;
    BYTE* flag = reinterpret_cast<BYTE*>(localPawn) +
        g_inventoryGloveRuntime.needReapplyOffset;
    if (IsAccessible(flag, sizeof(BYTE), true))
        *flag = 1;
}
'@
$markGloveReplacement = @'
static void MarkGlovesForReapply(void* localPawn)
{
    if (!localPawn || !g_inventoryGloveRuntime.ready)
        return;
    BYTE* flag = reinterpret_cast<BYTE*>(localPawn) +
        g_inventoryGloveRuntime.needReapplyOffset;
    if (IsAccessible(flag, sizeof(BYTE), true))
        *flag = 1;
    if (g_inventoryGloveRuntime.hasChangeCounter)
    {
        BYTE* counter = reinterpret_cast<BYTE*>(localPawn) +
            g_inventoryGloveRuntime.changeCounterOffset;
        if (IsAccessible(counter, sizeof(BYTE), true))
            ++(*counter);
    }
}
'@
Replace-Required $markGloveAnchor $markGloveReplacement 'glove change handshake'

$gloveBindAnchor = @'
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = view + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    MarkGlovesForReapply(localPawn);
    g_lastProjectedGloveItemId = selected.itemId;
'@
$gloveBindReplacement = @'
    if (g_inventoryRuntime.hasItemInitialized)
    {
        BYTE* initialized = view + g_inventoryRuntime.itemInitializedOffset;
        if (IsAccessible(initialized, sizeof(BYTE), true))
            *initialized = 1;
    }
    InventoryEconBindItemView(view, selected.itemId);
    MarkGlovesForReapply(localPawn);
    g_lastProjectedGloveItemId = selected.itemId;
'@
Replace-Required $gloveBindAnchor $gloveBindReplacement 'glove CEconItem view binding'

# Promote final domain name calls (music patcher currently owns these UI sites)
# before injecting inventory_full.inc, avoiding accidental replacement inside
# InventoryCatalogFullDomainName itself.
$nameNeedle = 'InventoryCatalogDomainName(item.slotDefinitionIndex, item.overrideDefinitionIndex)'
if (($source.Split($nameNeedle).Count - 1) -lt 2) {
    throw 'Extended inventory domain-name anchors were not found.'
}
$source = $source.Replace($nameNeedle,
    'InventoryCatalogFullDomainName(item.slotDefinitionIndex, item.overrideDefinitionIndex)')

# Agent definition selector joins the final music/glove/knife selector chain.
$definitionAnchor = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Music");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Music >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
'@
$definitionReplacement = @'
        if (item.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Agent");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Agent >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
        {
            DrawInventoryButton(hdc, 530, 163, 92, 28, L"< Music");
            DrawInventoryButton(hdc, 628, 163, 96, 28, L"Music >");
        }
        else if (item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE)
'@
Replace-Required $definitionAnchor $definitionReplacement 'agent definition controls'

$definitionClickAnchor = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleMusicCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleMusicCatalog(1);
            }
'@
$definitionClickReplacement = @'
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_AGENT)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleAgentCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleAgentCatalog(1);
            }
            else if (inv.hasSelectedItem &&
                mouseY >= 158 && mouseY <= 195 &&
                inv.selectedItem.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
            {
                if (mouseX >= 525 && mouseX < 625)
                    InventoryUiCycleMusicCatalog(-1);
                else if (mouseX >= 625 && mouseX <= 730)
                    InventoryUiCycleMusicCatalog(1);
            }
'@
Replace-Required $definitionClickAnchor $definitionClickReplacement 'agent definition click controls'

$paintStatusAnchor = @'
        const wchar_t* paintCatalogName =
            item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ?
                L"not used by music kits" :
            item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ?
                L"stored only - glove attributes pending" :
                InventoryCatalogPaintName(item.paintKit,
                    item.slotDefinitionIndex);
'@
$paintStatusReplacement = @'
        const wchar_t* paintCatalogName =
            item.slotDefinitionIndex == INVENTORY_SLOT_AGENT ?
                L"not used by agents" :
            item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ?
                L"not used by music kits" :
            item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ?
                L"client econ attributes" :
                InventoryCatalogPaintName(item.paintKit,
                    item.slotDefinitionIndex);
'@
Replace-Required $paintStatusAnchor $paintStatusReplacement 'domain paint status'

# Search lives beside the existing header glove factory. It does not activate the
# overlay; text capture comes from the reversible game-window subclass.
$headerAnchor = '    DrawInventoryButton(hdc, 445, 62, 100, 26, L"+ Gloves");'
$headerReplacement = @'
    DrawInventoryButton(hdc, 330, 62, 108, 26,
        g_inventorySearchLength ? L"Search *" : L"Search");
    DrawInventoryButton(hdc, 445, 62, 100, 26, L"+ Gloves");
'@
Replace-Required $headerAnchor $headerReplacement.TrimEnd() 'inventory search header'

$headerClickAnchor = @'
            if (mouseX >= 440 && mouseX <= 550 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiAddGlovePreset();
            }
            else if (mouseX >= 684 && mouseX <= 735 &&
'@
$headerClickReplacement = @'
            if (mouseX >= 326 && mouseX < 440 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiBeginSearchEdit();
            }
            else if (mouseX >= 440 && mouseX <= 550 &&
                mouseY >= 58 && mouseY <= 94)
            {
                InventoryUiAddGlovePreset();
            }
            else if (mouseX >= 684 && mouseX <= 735 &&
'@
Replace-Required $headerClickAnchor $headerClickReplacement 'inventory search click'

# Final footer gets a compact Agent factory without changing panel dimensions.
$footerAnchor = @'
    DrawInventoryButton(hdc, 35, 425, 50, 30, L"<Pg");
    DrawInventoryButton(hdc, 89, 425, 50, 30, L"Pg>");
    DrawInventoryButton(hdc, 143, 425, 56, 30, L"+Wpn");
    DrawInventoryButton(hdc, 203, 425, 58, 30, L"Current");
    DrawInventoryButton(hdc, 265, 425, 60, 30, L"+Music");
'@
$footerReplacement = @'
    DrawInventoryButton(hdc, 35, 425, 44, 30, L"<");
    DrawInventoryButton(hdc, 83, 425, 44, 30, L">");
    DrawInventoryButton(hdc, 131, 425, 46, 30, L"+W");
    DrawInventoryButton(hdc, 181, 425, 48, 30, L"Cur");
    DrawInventoryButton(hdc, 233, 425, 44, 30, L"+M");
    DrawInventoryButton(hdc, 281, 425, 44, 30, L"+A");
'@
Replace-Required $footerAnchor $footerReplacement 'agent footer factory'

$footerClickAnchor = @'
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
$footerClickReplacement = @'
            else if (mouseY >= 420 && mouseY <= 460)
            {
                if (mouseX >= 30 && mouseX < 81)
                    InventoryUiChangePage(-1);
                else if (mouseX >= 81 && mouseX < 129)
                    InventoryUiChangePage(1);
                else if (mouseX >= 129 && mouseX < 179)
                    InventoryUiAddWeaponPreset();
                else if (mouseX >= 179 && mouseX < 231)
                    InventoryUiRequestAddCurrent();
                else if (mouseX >= 231 && mouseX < 279)
                    InventoryUiAddMusicPreset();
                else if (mouseX >= 279 && mouseX <= 330)
                    InventoryUiAddAgentPreset();
            }
'@
Replace-Required $footerClickAnchor $footerClickReplacement 'agent footer click'

# Five compact item operations fit in the existing bottom editor row.
$actionsAnchor = @'
        DrawInventoryButton(hdc, 360, 411, 112, 26, L"Duplicate");
        DrawInventoryButton(hdc, 478, 411, 112, 26, L"Delete item");
'@
$actionsReplacement = @'
        DrawInventoryButton(hdc, 360, 411, 68, 26, L"Copy");
        DrawInventoryButton(hdc, 432, 411, 68, 26, L"Delete");
        DrawInventoryButton(hdc, 504, 411, 68, 26, L"Sticker");
        DrawInventoryButton(hdc, 576, 411, 68, 26, L"Name",
            g_inventoryTextMode == INVENTORY_TEXT_NAME);
        DrawInventoryButton(hdc, 648, 411, 76, 26, L"Swap ST",
            g_inventoryStatTrakSwapSource >= 0);
'@
Replace-Required $actionsAnchor $actionsReplacement 'extended item operations'

$localNoteAnchor = @'
        SetTextColor(hdc, RGB_COLOR(113, 113, 122));
        RECT localNote = { 500, 405, 720, 440 };
        DrawTextW(hdc,
            L"Local cosmetic/loadout state only; Steam inventory is untouched.",
            -1, &localNote, DT_LEFT);
'@
if ($source.Contains($localNoteAnchor)) {
    $source = $source.Replace($localNoteAnchor, '')
}

$actionsClickAnchor = @'
            else if (inv.hasSelectedItem && mouseY >= 407 && mouseY <= 442)
            {
                if (mouseX >= 355 && mouseX < 475)
                    InventoryUiDuplicateSelected();
                else if (mouseX >= 475 && mouseX <= 595)
                    InventoryUiDeleteSelected();
                else
                    return 0;
            }
'@
$actionsClickReplacement = @'
            else if (inv.hasSelectedItem && mouseY >= 407 && mouseY <= 442)
            {
                if (mouseX >= 355 && mouseX < 430)
                    InventoryUiDuplicateSelected();
                else if (mouseX >= 430 && mouseX < 502)
                    InventoryUiDeleteSelected();
                else if (mouseX >= 502 && mouseX < 574)
                    InventoryUiOpenStickerModal();
                else if (mouseX >= 574 && mouseX < 646)
                    InventoryUiBeginNameEdit();
                else if (mouseX >= 646 && mouseX <= 730)
                    InventoryUiArmStatTrakSwap();
                else
                    return 0;
            }
'@
Replace-Required $actionsClickAnchor $actionsClickReplacement 'extended item operation clicks'

# When a swap is armed, selecting the target row completes the operation. An
# invalid target still becomes the selected row, making failure obvious rather
# than silently consuming the click.
$rowClickAnchor = @'
                if (row >= 0 && row < inv.visibleCount &&
                    mouseY <= 122 + row * 42 + 38)
                    InventoryUiSelect(inv.visibleIndices[row]);
'@
$rowClickReplacement = @'
                if (row >= 0 && row < inv.visibleCount &&
                    mouseY <= 122 + row * 42 + 38)
                {
                    const int target = inv.visibleIndices[row];
                    if (g_inventoryStatTrakSwapSource >= 0)
                    {
                        if (!InventoryUiCompleteStatTrakSwap(target))
                            InventoryUiSelect(target);
                    }
                    else
                        InventoryUiSelect(target);
                }
'@
Replace-Required $rowClickAnchor $rowClickReplacement 'StatTrak swap target selection'

# Switch both drawing and hit-testing snapshots to search-filtered paging.
if (($source.Split('InventoryGetUiSnapshot(&').Count - 1) -lt 2) {
    throw 'Inventory snapshot anchors were not found for search filtering.'
}
$source = $source.Replace('InventoryGetUiSnapshot(&snapshot)',
    'InventoryGetFilteredUiSnapshot(&snapshot)')
$source = $source.Replace('InventoryGetUiSnapshot(&inv)',
    'InventoryGetFilteredUiSnapshot(&inv)')

# Extended backend runs before direct projections so a freshly-created mirror is
# already available when weapon/glove item views are bound on the same frame.
$frameAnchor = @'
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryMusic();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
$frameReplacement = @'
        UpdateInventoryEconBackend();
        UpdateInventoryChanger();
        UpdateInventoryGloves();
        UpdateInventoryMusic();
        UpdateInventoryVisualRefresh();
        const LONG botRequest = AtomicExchange(
'@
Replace-Required $frameAnchor $frameReplacement 'extended frame update order'

# Load/save sidecar and install/remove the temporary keyboard subclass from the
# payload worker, never from FrameStageNotify.
$loadAnchor = @'
    LoadInventoryStore();
    if (!InstallFrameStageBridge())
'@
$loadReplacement = @'
    LoadInventoryStore();
    InventoryExtendedLoad();
    if (!InstallFrameStageBridge())
'@
Replace-Required $loadAnchor $loadReplacement 'extended inventory load'

$flushAnchor = @'
        FlushInventoryPersistenceIfNeeded();
        Sleep(8);
'@
$flushReplacement = @'
        FlushInventoryPersistenceIfNeeded();
        FlushInventoryStickerPersistenceIfNeeded();
        Sleep(8);
'@
Replace-Required $flushAnchor $flushReplacement 'sticker persistence flush'

$shutdownAnchor = @'
    ShutdownInventoryMusic();
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    RemoveFrameStageBridge();
'@
$shutdownReplacement = @'
    ShutdownInventoryMusic();
    ShutdownInventoryGloves();
    ShutdownInventoryChanger();
    ShutdownInventoryExtended();
    RemoveFrameStageBridge();
'@
Replace-Required $shutdownAnchor $shutdownReplacement 'extended inventory shutdown'

# Add the sticker editor overlay immediately before MenuWindowProc. This helper
# is drawn on top of the normal inventory editor and consumes clicks first.
$stickerUi = @'
static void DrawInventoryStickerModal(HDC hdc, const InventoryUiSnapshot& snapshot)
{
    if (!g_inventoryStickerModal || !snapshot.hasSelectedItem)
        return;
    InventoryStickerRecord record;
    InventoryStickerCopyRecord(snapshot.selectedItem.itemId, &record);
    const int slotIndex = InventoryClampInt(g_inventoryStickerUiSlot,
        0, INVENTORY_STICKER_SLOT_COUNT - 1);
    const InventoryStickerSlot& sticker = record.slots[slotIndex];

    DrawRoundedCard(hdc, 345, 98, 395, 357,
        RGB_COLOR(20, 20, 24), RGB_COLOR(82, 82, 91), 6);
    SetTextColor(hdc, RGB_COLOR(244, 244, 245));
    RECT title = { 360, 112, 620, 134 };
    DrawTextW(hdc, L"STICKER EDITOR", -1, &title,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 674, 108, 50, 24, L"Close");

    for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
    {
        wchar_t label[2] = { static_cast<wchar_t>(L'1' + i), 0 };
        DrawInventoryButton(hdc, 360 + i * 55, 145, 48, 26, label,
            i == slotIndex);
    }

    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT idLabel = { 360, 186, 410, 206 };
    DrawTextW(hdc, L"ID", -1, &idLabel, DT_LEFT | DT_SINGLELINE);
    RECT idValue = { 405, 186, 475, 206 };
    DrawInventoryNumber(hdc, static_cast<unsigned int>(sticker.id), &idValue,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 480, 179, 50, 26, L"-100");
    DrawInventoryButton(hdc, 534, 179, 42, 26, L"-1");
    DrawInventoryButton(hdc, 580, 179, 42, 26, L"+1");
    DrawInventoryButton(hdc, 626, 179, 54, 26, L"+100");

    RECT wearLabel = { 360, 220, 430, 240 };
    DrawTextW(hdc, L"Wear", -1, &wearLabel, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 435, 213, 68, 26, L"Scrape");
    DrawInventoryButton(hdc, 507, 213, 68, 26, L"Remove");

    RECT rotation = { 360, 254, 440, 274 };
    DrawTextW(hdc, L"Rotation", -1, &rotation, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 445, 247, 54, 26, L"-15");
    DrawInventoryButton(hdc, 503, 247, 54, 26, L"+15");
    RECT scale = { 570, 254, 615, 274 };
    DrawTextW(hdc, L"Scale", -1, &scale, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 618, 247, 48, 26, L"-.1");
    DrawInventoryButton(hdc, 670, 247, 48, 26, L"+.1");

    RECT schema = { 360, 288, 425, 308 };
    DrawTextW(hdc, L"Anchor", -1, &schema, DT_LEFT | DT_SINGLELINE);
    RECT schemaValue = { 425, 288, 455, 308 };
    DrawInventoryNumber(hdc, sticker.schema, &schemaValue,
        DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 460, 281, 40, 26, L"<");
    DrawInventoryButton(hdc, 504, 281, 40, 26, L">");

    RECT offsetX = { 360, 322, 410, 342 };
    DrawTextW(hdc, L"X", -1, &offsetX, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 400, 315, 48, 26, L"-.01");
    DrawInventoryButton(hdc, 452, 315, 48, 26, L"+.01");
    RECT offsetY = { 520, 322, 570, 342 };
    DrawTextW(hdc, L"Y", -1, &offsetY, DT_LEFT | DT_SINGLELINE);
    DrawInventoryButton(hdc, 555, 315, 48, 26, L"-.01");
    DrawInventoryButton(hdc, 607, 315, 48, 26, L"+.01");

    DrawInventoryButton(hdc, 360, 365, 82, 28, L"Move <");
    DrawInventoryButton(hdc, 446, 365, 82, 28, L"Move >");
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT hint = { 360, 400, 715, 430 };
    DrawTextW(hdc,
        L"Changes rebuild the local CEconItem mirror; no raw attribute vector writes.",
        -1, &hint, DT_LEFT);
}

'@
$menuProcAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuProcIndex = $source.IndexOf($menuProcAnchor)
if ($menuProcIndex -lt 0) {
    throw 'Extended sticker UI MenuWindowProc anchor was not found.'
}
$source = $source.Substring(0, $menuProcIndex) + $stickerUi +
    $source.Substring($menuProcIndex)

# Draw sticker modal after the main panel but before the inventory paint branch
# returns the completed backbuffer.
$drawAnchor = @'
            DrawInventoryChangerPanel(memDC);
            SelectObject(memDC, oldFont);
'@
$drawReplacement = @'
            InventoryUiSnapshot inventoryModalSnapshot;
            InventoryGetFilteredUiSnapshot(&inventoryModalSnapshot);
            DrawInventoryChangerPanel(memDC);
            DrawInventoryStickerModal(memDC, inventoryModalSnapshot);
            SelectObject(memDC, oldFont);
'@
Replace-Required $drawAnchor $drawReplacement 'sticker modal draw'

# Sticker modal consumes clicks before any underlying inventory controls.
$modalClickAnchor = @'
            InventoryUiSnapshot inv;
            InventoryGetFilteredUiSnapshot(&inv);

            if (mouseX >= 326 && mouseX < 440 &&
'@
$modalClickReplacement = @'
            InventoryUiSnapshot inv;
            InventoryGetFilteredUiSnapshot(&inv);

            if (g_inventoryStickerModal)
            {
                if (mouseX >= 670 && mouseX <= 730 && mouseY >= 104 && mouseY <= 136)
                    InventoryUiCloseStickerModal();
                else if (mouseY >= 141 && mouseY <= 176 && mouseX >= 356 && mouseX < 638)
                    InventoryUiSelectStickerSlot((mouseX - 356) / 55);
                else if (mouseY >= 175 && mouseY <= 210)
                {
                    if (mouseX >= 476 && mouseX < 532) InventoryUiAdjustStickerId(-100);
                    else if (mouseX >= 532 && mouseX < 578) InventoryUiAdjustStickerId(-1);
                    else if (mouseX >= 578 && mouseX < 624) InventoryUiAdjustStickerId(1);
                    else if (mouseX >= 624 && mouseX <= 684) InventoryUiAdjustStickerId(100);
                }
                else if (mouseY >= 209 && mouseY <= 244)
                {
                    if (mouseX >= 431 && mouseX < 505) InventoryUiScrapeSticker();
                    else if (mouseX >= 505 && mouseX <= 578) InventoryUiRemoveSticker();
                }
                else if (mouseY >= 243 && mouseY <= 278)
                {
                    if (mouseX >= 441 && mouseX < 501) InventoryUiAdjustStickerRotation(-15.0f);
                    else if (mouseX >= 501 && mouseX < 560) InventoryUiAdjustStickerRotation(15.0f);
                    else if (mouseX >= 614 && mouseX < 668) InventoryUiAdjustStickerScale(-0.1f);
                    else if (mouseX >= 668 && mouseX <= 722) InventoryUiAdjustStickerScale(0.1f);
                }
                else if (mouseY >= 277 && mouseY <= 312)
                {
                    if (mouseX >= 456 && mouseX < 502) InventoryUiAdjustStickerSchema(-1);
                    else if (mouseX >= 502 && mouseX <= 548) InventoryUiAdjustStickerSchema(1);
                }
                else if (mouseY >= 311 && mouseY <= 346)
                {
                    if (mouseX >= 396 && mouseX < 450) InventoryUiAdjustStickerOffsetX(-0.01f);
                    else if (mouseX >= 450 && mouseX < 502) InventoryUiAdjustStickerOffsetX(0.01f);
                    else if (mouseX >= 551 && mouseX < 605) InventoryUiAdjustStickerOffsetY(-0.01f);
                    else if (mouseX >= 605 && mouseX <= 659) InventoryUiAdjustStickerOffsetY(0.01f);
                }
                else if (mouseY >= 361 && mouseY <= 398)
                {
                    if (mouseX >= 356 && mouseX < 444) InventoryUiMoveSticker(-1);
                    else if (mouseX >= 444 && mouseX <= 532) InventoryUiMoveSticker(1);
                }
                InvalidateRect(wnd, nullptr, FALSE);
                return 0;
            }

            if (mouseX >= 326 && mouseX < 440 &&
'@
Replace-Required $modalClickAnchor $modalClickReplacement 'sticker modal click routing'

# Finally inject implementations after all source transformations above. The
# preceding modules are already present at this point, while FrameStageNotify is
# still below, so the backend update call resolves without a forward declaration.
$hookAnchor = 'static void FrameStageNotifyHook(void* client, int stage)'
$hookIndex = $source.IndexOf($hookAnchor)
if ($hookIndex -lt 0) {
    throw 'Extended inventory frame-stage injection anchor was not found.'
}
$source = $source.Substring(0, $hookIndex) + $module + "`r`n`r`n" +
    $source.Substring($hookIndex)

Set-Content -LiteralPath $OutputPath -Value $source -Encoding UTF8
Write-Host "Injected extended inventory backend, stickers, agents, search and operations: $OutputPath"
