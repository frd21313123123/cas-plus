param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Inventory runtime diagnostic anchor '$Name' expected once, found $count. Refusing to patch blindly."
    }
    $script:source = $source.Replace($Needle, $Replacement)
}

$globalsAnchor = @'
static unsigned int g_inventoryEconCreates = 0;
static unsigned int g_inventoryEconRemoves = 0;
static unsigned int g_inventoryEconFailures = 0;
'@
$globalsReplacement = @'
static unsigned int g_inventoryEconCreates = 0;
static unsigned int g_inventoryEconRemoves = 0;
static unsigned int g_inventoryEconFailures = 0;

// Runtime-only diagnostics. These values are written on the FrameStage game
// thread and are intentionally not persisted. They distinguish live-entity
// projection from the local SharedObject/CEcon mirror backend.
static unsigned int g_inventoryEconDiagPatternMask = 0;
static unsigned int g_inventoryEconDiagFailureStage = 0;
static bool g_inventoryEconDiagInventoryReady = false;
static bool g_inventoryEconDiagTypeCacheReady = false;

enum InventoryEconDiagStage : unsigned int {
    INVENTORY_ECON_DIAG_READY = 0,
    INVENTORY_ECON_DIAG_RUNTIME = 1,
    INVENTORY_ECON_DIAG_SHARED = 2,
    INVENTORY_ECON_DIAG_ATTRIBUTES = 3,
    INVENTORY_ECON_DIAG_LOCAL_INVENTORY = 4,
    INVENTORY_ECON_DIAG_TYPE_CACHE = 5,
    INVENTORY_ECON_DIAG_CREATE = 6
};

static const wchar_t* InventoryEconDiagStateText()
{
    if (!g_inventoryEconRuntime.resolved)
        return L"WAIT";
    switch (g_inventoryEconDiagFailureStage)
    {
    case INVENTORY_ECON_DIAG_RUNTIME: return L"SIG";
    case INVENTORY_ECON_DIAG_SHARED: return L"SO";
    case INVENTORY_ECON_DIAG_ATTRIBUTES: return L"ATTR";
    case INVENTORY_ECON_DIAG_LOCAL_INVENTORY: return L"NO-INV";
    case INVENTORY_ECON_DIAG_TYPE_CACHE: return L"CACHE";
    case INVENTORY_ECON_DIAG_CREATE: return L"CREATE";
    default: return g_inventoryEconDiagInventoryReady &&
        g_inventoryEconDiagTypeCacheReady ? L"READY" : L"WAIT";
    }
}
'@
Replace-Required $globalsAnchor $globalsReplacement 'econ diagnostic globals'

$patternAnchor = @'
    BYTE* viewAttributeCall = FindUniquePattern(client, viewAttributeCallPattern,
        sizeof(viewAttributeCallPattern) / sizeof(viewAttributeCallPattern[0]));

    if (manager && IsAccessible(manager + 3, sizeof(LONG), false))
'@
$patternReplacement = @'
    BYTE* viewAttributeCall = FindUniquePattern(client, viewAttributeCallPattern,
        sizeof(viewAttributeCallPattern) / sizeof(viewAttributeCallPattern[0]));

    // Bits 0..10 correspond to the eleven exact pattern gates above. A full
    // current-client match is 2047. Showing the mask in the UI makes a client
    // update mismatch immediately actionable without dereferencing guesses.
    g_inventoryEconDiagPatternMask = 0;
    if (manager) g_inventoryEconDiagPatternMask |= 1u << 0;
    if (inventoryAccessor) g_inventoryEconDiagPatternMask |= 1u << 1;
    if (sharedCache) g_inventoryEconDiagPatternMask |= 1u << 2;
    if (createTypeCacheCall) g_inventoryEconDiagPatternMask |= 1u << 3;
    if (createEcon) g_inventoryEconDiagPatternMask |= 1u << 4;
    if (getSystem) g_inventoryEconDiagPatternMask |= 1u << 5;
    if (attributeDefinitionCall) g_inventoryEconDiagPatternMask |= 1u << 6;
    if (dynamicThunk) g_inventoryEconDiagPatternMask |= 1u << 7;
    if (equip) g_inventoryEconDiagPatternMask |= 1u << 8;
    if (getLoadout) g_inventoryEconDiagPatternMask |= 1u << 9;
    if (viewAttributeCall) g_inventoryEconDiagPatternMask |= 1u << 10;

    if (manager && IsAccessible(manager + 3, sizeof(LONG), false))
'@
Replace-Required $patternAnchor $patternReplacement 'pattern mask'

$updateAnchor = @'
static void UpdateInventoryEconBackend()
{
    if (!ResolveInventoryEconRuntime() || !g_inventoryEconRuntime.sharedReady ||
        !g_inventoryEconRuntime.attributeReady)
        return;
    void* inventory = nullptr;
    InventorySOID owner{};
    if (!InventoryEconResolveInventory(&inventory, &owner))
        return;
'@
$updateReplacement = @'
static void UpdateInventoryEconBackend()
{
    g_inventoryEconDiagInventoryReady = false;
    g_inventoryEconDiagTypeCacheReady = false;
    g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_READY;

    if (!ResolveInventoryEconRuntime())
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_RUNTIME;
        return;
    }
    if (!g_inventoryEconRuntime.sharedReady)
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_SHARED;
        return;
    }
    if (!g_inventoryEconRuntime.attributeReady)
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_ATTRIBUTES;
        return;
    }

    void* inventory = nullptr;
    InventorySOID owner{};
    if (!InventoryEconResolveInventory(&inventory, &owner))
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_LOCAL_INVENTORY;
        return;
    }
    g_inventoryEconDiagInventoryReady = true;

    // Probe the item type-cache before trying to create any mirror. This call
    // is already used by the creation path and is fail-closed behind the same
    // runtime validation; exposing it separately identifies SOCache failures.
    if (!InventoryEconTypeCache(inventory))
    {
        g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_TYPE_CACHE;
        return;
    }
    g_inventoryEconDiagTypeCacheReady = true;
'@
Replace-Required $updateAnchor $updateReplacement 'backend stage diagnostics'

$createFailureAnchor = @'
                if (!InventoryEconCreateMirror(item, inventory, owner, signature))
                {
                    ++g_inventoryEconFailures;
                    continue;
                }
'@
$createFailureReplacement = @'
                if (!InventoryEconCreateMirror(item, inventory, owner, signature))
                {
                    ++g_inventoryEconFailures;
                    g_inventoryEconDiagFailureStage = INVENTORY_ECON_DIAG_CREATE;
                    continue;
                }
'@
Replace-Required $createFailureAnchor $createFailureReplacement 'mirror create failure stage'

$uiAnchor = @'
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT runtime = { 360, 136, 720, 156 };
    DrawTextW(hdc, L"Active def / applied / owned:", -1, &runtime,
        DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT activeDef = { 555, 136, 605, 156 };
    DrawInventoryNumber(hdc, snapshot.activeDefinitionIndex, &activeDef,
        DT_LEFT | DT_SINGLELINE);
    RECT applied = { 610, 136, 650, 156 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.appliedCount), &applied,
        DT_LEFT | DT_SINGLELINE);
    RECT owned = { 660, 136, 705, 156 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.ownedEconCount), &owned,
        DT_LEFT | DT_SINGLELINE);
'@
$uiReplacement = @'
    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT runtime = { 360, 136, 440, 151 };
    DrawTextW(hdc, L"Live A/P/O", -1, &runtime,
        DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT activeDef = { 438, 136, 482, 151 };
    DrawInventoryNumber(hdc, snapshot.activeDefinitionIndex, &activeDef,
        DT_LEFT | DT_SINGLELINE);
    RECT applied = { 486, 136, 524, 151 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.appliedCount), &applied,
        DT_LEFT | DT_SINGLELINE);
    RECT owned = { 528, 136, 566, 151 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(snapshot.ownedEconCount), &owned,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT econLabel = { 360, 151, 395, 166 };
    DrawTextW(hdc, L"Econ", -1, &econLabel, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc,
        g_inventoryEconDiagFailureStage == INVENTORY_ECON_DIAG_READY &&
        g_inventoryEconDiagInventoryReady && g_inventoryEconDiagTypeCacheReady ?
        RGB_COLOR(132, 204, 22) : RGB_COLOR(249, 115, 22));
    RECT econState = { 395, 151, 465, 166 };
    DrawTextW(hdc, InventoryEconDiagStateText(), -1, &econState,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT maskLabel = { 468, 151, 505, 166 };
    DrawTextW(hdc, L"mask", -1, &maskLabel, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT maskValue = { 505, 151, 552, 166 };
    DrawInventoryNumber(hdc, g_inventoryEconDiagPatternMask, &maskValue,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT mirrorLabel = { 556, 151, 578, 166 };
    DrawTextW(hdc, L"m", -1, &mirrorLabel, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT mirrorValue = { 570, 151, 610, 166 };
    DrawInventoryNumber(hdc,
        static_cast<unsigned int>(g_inventoryEconMirrorCount), &mirrorValue,
        DT_LEFT | DT_SINGLELINE);

    SetTextColor(hdc, RGB_COLOR(113, 113, 122));
    RECT failLabel = { 612, 151, 634, 166 };
    DrawTextW(hdc, L"f", -1, &failLabel, DT_LEFT | DT_SINGLELINE);
    SetTextColor(hdc, RGB_COLOR(212, 212, 216));
    RECT failValue = { 628, 151, 690, 166 };
    DrawInventoryNumber(hdc, g_inventoryEconFailures, &failValue,
        DT_LEFT | DT_SINGLELINE);
'@
Replace-Required $uiAnchor $uiReplacement 'inventory UI diagnostics'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Injected Inventory Econ runtime diagnostics: $InputPath"
