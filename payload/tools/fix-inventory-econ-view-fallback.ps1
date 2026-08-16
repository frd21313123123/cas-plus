param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Expected exactly one inventory view-fallback anchor '$Name', found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# A missing GetEconItemSystem must not block SharedObject registration. Dynamic
# CEconItem attributes become optional; C_EconItemView attributes are projected
# through the independently-resolved SetAttributeValueByName client function.
$gateAnchor = @'
    if (!inventory || !owner.id || g_inventoryEconMirrorCount >= INVENTORY_EXT_MAX_RECORDS ||
        !g_inventoryEconRuntime.sharedReady || !g_inventoryEconRuntime.attributeReady)
        return false;
'@
$gateReplacement = @'
    if (!inventory || !owner.id || g_inventoryEconMirrorCount >= INVENTORY_EXT_MAX_RECORDS ||
        !g_inventoryEconRuntime.sharedReady)
        return false;
'@
Replace-Required $gateAnchor $gateReplacement 'shared-only CEconItem creation gate'

$applyAnchor = @'
        if (!InventoryEconApplyAttributes(object, item))
        {
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }
'@
$applyReplacement = @'
        if (g_inventoryEconRuntime.attributeReady &&
            !InventoryEconApplyAttributes(object, item))
        {
            void* destructor = InventoryEconVtableFunction(object, 1);
            if (destructor)
                reinterpret_cast<void (*)(void*, bool)>(destructor)(object, true);
            return false;
        }
'@
Replace-Required $applyAnchor $applyReplacement 'optional native CEcon attributes'

# Keep running the SharedObject backend when only the attribute singleton getter
# is missing. The UI will show VIEW to make it explicit that the independent
# item-view attribute path is active.
$attrDiagAnchor = @'
    if (!g_inventoryEconRuntime.attributeReady)
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_ATTRIBUTES;
        return;
    }

    void* inventory = nullptr;
'@
$attrDiagReplacement = @'
    if (!g_inventoryEconRuntime.attributeReady)
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_ATTRIBUTES;

    void* inventory = nullptr;
'@
Replace-Required $attrDiagAnchor $attrDiagReplacement 'non-fatal attribute diagnostics'

$stateAnchor = '    case INVENTORY_ECON_DIAG_ATTRIBUTES: return L"ATTR";'
$stateReplacement = @'
    case INVENTORY_ECON_DIAG_ATTRIBUTES:
        return g_inventoryEconRuntime.setViewAttributeByName &&
            IsExecutable(g_inventoryEconRuntime.setViewAttributeByName) &&
            g_inventoryEconRuntime.viewReady ? L"VIEW" : L"ATTR";
'@
Replace-Required $stateAnchor $stateReplacement.TrimEnd() 'VIEW fallback status'

# Helpers are injected before the item signature function, after all sticker and
# econ runtime declarations are already available.
$helperAnchor = @'
static unsigned int InventoryEconItemSignature(
    const VirtualInventoryItem& item)
{
'@
$helperBlock = @'
struct InventoryClientItemVector {
    int count;
    int debug;
    void** memory;
};

static BYTE* InventoryEconFindInventoryView(void* inventory,
    unsigned long long itemId)
{
    if (!inventory || !itemId || !g_inventoryEconRuntime.viewReady)
        return nullptr;
    auto* vector = reinterpret_cast<InventoryClientItemVector*>(
        reinterpret_cast<BYTE*>(inventory) + 0x20);
    if (!IsAccessible(vector, sizeof(InventoryClientItemVector), false) ||
        vector->count < 0 || vector->count > 4096 ||
        (vector->count > 0 && (!vector->memory ||
            !IsAccessible(vector->memory,
                static_cast<SIZE_T>(vector->count) * sizeof(void*), false))))
        return nullptr;
    for (int i = 0; i < vector->count; ++i)
    {
        BYTE* view = reinterpret_cast<BYTE*>(vector->memory[i]);
        if (!view || !IsAccessible(view + g_inventoryEconRuntime.viewItemIdOffset,
                sizeof(unsigned long long), false))
            continue;
        if (*reinterpret_cast<unsigned long long*>(view +
                g_inventoryEconRuntime.viewItemIdOffset) == itemId)
            return view;
    }
    return nullptr;
}

static bool InventoryEconSetViewFloat(BYTE* view, const char* name, float value)
{
    if (!view || !name || !*name ||
        !g_inventoryEconRuntime.setViewAttributeByName ||
        !IsExecutable(g_inventoryEconRuntime.setViewAttributeByName))
        return false;
    using Fn = void (*)(void*, const char*, float);
    reinterpret_cast<Fn>(g_inventoryEconRuntime.setViewAttributeByName)(
        view, name, value);
    return true;
}

static bool InventoryEconApplyViewAttributes(BYTE* view,
    const VirtualInventoryItem& item)
{
    if (!view || !g_inventoryEconRuntime.viewReady)
        return false;

    bool ok = true;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
    {
        ok = InventoryEconSetViewFloat(view, "music id",
            static_cast<float>(item.overrideDefinitionIndex)) && ok;
    }
    else if (item.paintKit > 0)
    {
        ok = InventoryEconSetViewFloat(view, "set item texture prefab",
            static_cast<float>(item.paintKit)) && ok;
        ok = InventoryEconSetViewFloat(view, "set item texture seed",
            static_cast<float>(InventoryClampInt(item.seed, 0, 1000))) && ok;
        ok = InventoryEconSetViewFloat(view, "set item texture wear",
            InventoryClampWear(item.wear)) && ok;
    }

    if (item.statTrak >= 0 && item.slotDefinitionIndex != INVENTORY_SLOT_AGENT &&
        item.slotDefinitionIndex != INVENTORY_SLOT_GLOVE)
    {
        ok = InventoryEconSetViewFloat(view, "kill eater",
            static_cast<float>(InventoryClampInt(item.statTrak, 0, 999999))) && ok;
        ok = InventoryEconSetViewFloat(view, "kill eater score type", 0.0f) && ok;
    }

    static const char* stickerId[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 id", "sticker slot 1 id", "sticker slot 2 id",
        "sticker slot 3 id", "sticker slot 4 id"
    };
    static const char* stickerWear[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 wear", "sticker slot 1 wear", "sticker slot 2 wear",
        "sticker slot 3 wear", "sticker slot 4 wear"
    };
    static const char* stickerScale[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 scale", "sticker slot 1 scale", "sticker slot 2 scale",
        "sticker slot 3 scale", "sticker slot 4 scale"
    };
    static const char* stickerRotation[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 rotation", "sticker slot 1 rotation", "sticker slot 2 rotation",
        "sticker slot 3 rotation", "sticker slot 4 rotation"
    };
    static const char* stickerOffsetX[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 offset x", "sticker slot 1 offset x", "sticker slot 2 offset x",
        "sticker slot 3 offset x", "sticker slot 4 offset x"
    };
    static const char* stickerOffsetY[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 offset y", "sticker slot 1 offset y", "sticker slot 2 offset y",
        "sticker slot 3 offset y", "sticker slot 4 offset y"
    };
    static const char* stickerSchema[INVENTORY_STICKER_SLOT_COUNT] = {
        "sticker slot 0 schema", "sticker slot 1 schema", "sticker slot 2 schema",
        "sticker slot 3 schema", "sticker slot 4 schema"
    };

    if (item.slotDefinitionIndex != INVENTORY_SLOT_GLOVE &&
        item.slotDefinitionIndex != INVENTORY_SLOT_MUSIC &&
        item.slotDefinitionIndex != INVENTORY_SLOT_AGENT)
    {
        InventoryStickerRecord stickers;
        InventoryStickerCopyRecord(item.itemId, &stickers);
        for (int i = 0; i < INVENTORY_STICKER_SLOT_COUNT; ++i)
        {
            InventoryStickerSlot slot = stickers.slots[i];
            InventoryStickerSanitize(&slot);
            if (slot.id <= 0)
                continue;
            ok = InventoryEconSetViewFloat(view, stickerId[i],
                static_cast<float>(slot.id)) && ok;
            ok = InventoryEconSetViewFloat(view, stickerWear[i], slot.wear) && ok;
            ok = InventoryEconSetViewFloat(view, stickerScale[i], slot.scale) && ok;
            ok = InventoryEconSetViewFloat(view, stickerRotation[i], slot.rotation) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetX[i], slot.offsetX) && ok;
            ok = InventoryEconSetViewFloat(view, stickerOffsetY[i], slot.offsetY) && ok;
            ok = InventoryEconSetViewFloat(view, stickerSchema[i],
                static_cast<float>(slot.schema)) && ok;
        }
    }

    if (!g_inventoryRuntime.ready)
        ResolveInventoryRuntime(&g_inventoryRuntime);
    if (g_inventoryRuntime.ready)
    {
        if (g_inventoryRuntime.hasQuality)
        {
            int* quality = reinterpret_cast<int*>(view +
                g_inventoryRuntime.entityQualityOffset);
            if (IsAccessible(quality, sizeof(int), true))
                *quality = item.quality;
        }
        if (g_inventoryRuntime.hasCustomName)
        {
            char* destination = reinterpret_cast<char*>(view +
                g_inventoryRuntime.customNameOffset);
            if (IsAccessible(destination, INVENTORY_CUSTOM_NAME_CAPACITY, true))
            {
                int i = 0;
                for (; i < item.customNameLength &&
                    i + 1 < INVENTORY_CUSTOM_NAME_CAPACITY; ++i)
                    destination[i] = item.customName[i];
                destination[i] = 0;
            }
        }
    }

    if (IsAccessible(view + g_inventoryEconRuntime.viewInitializedOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewInitializedOffset) = 1;
    if (IsAccessible(view + g_inventoryEconRuntime.viewDisallowSocOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewDisallowSocOffset) = 0;
    if (g_inventoryEconRuntime.viewRestoreMaterialOffset &&
        IsAccessible(view + g_inventoryEconRuntime.viewRestoreMaterialOffset, 1, true))
        *(view + g_inventoryEconRuntime.viewRestoreMaterialOffset) = 1;
    return ok;
}

static unsigned int InventoryEconItemSignature(
    const VirtualInventoryItem& item)
{
'@
Replace-Required $helperAnchor $helperBlock 'item-view attribute fallback helpers'

# Apply the independent C_EconItemView path every frame after a mirror exists.
# SOCreated may populate the client item vector one or more frames later, so this
# intentionally retries until the view becomes available.
$mirrorAnchor = @'
            if (mirror)
            {
                mirror->seen = true;
                if (mirror->teams != item.equippedTeams)
'@
$mirrorReplacement = @'
            if (mirror)
            {
                mirror->seen = true;
                BYTE* inventoryView = InventoryEconFindInventoryView(
                    inventory, mirror->econItemId);
                if (inventoryView)
                    InventoryEconApplyViewAttributes(inventoryView, item);
                if (mirror->teams != item.equippedTeams)
'@
Replace-Required $mirrorAnchor $mirrorReplacement 'per-frame inventory-view projection'

# Mark this build in the menu so screenshots can prove which DLL is actually
# loaded after replacement.
$econLabelAnchor = '    DrawTextW(hdc, L"Econ", -1, &econLabel, DT_LEFT | DT_SINGLELINE);'
$econLabelReplacement = '    DrawTextW(hdc, L"Econ V2", -1, &econLabel, DT_LEFT | DT_SINGLELINE);'
Replace-Required $econLabelAnchor $econLabelReplacement 'inventory build marker'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Enabled SharedObject inventory with independent item-view attributes: $InputPath"
