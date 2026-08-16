param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Expected exactly one inventory native-econ anchor '$Name', found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# GetEconItemSystem is the one signature that changed in the current client build.
# Resolve it from the stable function prologue and validate the RIP-relative
# global used by the function instead of depending on bytes after the branch.
$runtimeAnchor = @'
static bool ResolveInventoryEconRuntime()
{
'@
$runtimeHelper = @'
static BYTE* InventoryResolveEconItemSystemFunction(HMODULE module)
{
    if (!module)
        return nullptr;
    BYTE* base = reinterpret_cast<BYTE*>(module);
    const SIZE_T imageSize = ModuleImageSize(module);
    static const int prefix[] = {
        0x48,0x83,0xEC,0x28,0x48,0x8B,0x05,-1,-1,-1,-1,
        0x48,0x85,0xC0,0x0F,0x85
    };
    const SIZE_T prefixLength = sizeof(prefix) / sizeof(prefix[0]);
    if (!base || imageSize < prefixLength)
        return nullptr;

    BYTE* resolved = nullptr;
    for (SIZE_T offset = 0; offset + prefixLength <= imageSize; ++offset)
    {
        BYTE* candidate = base + offset;
        bool matches = true;
        for (SIZE_T i = 0; i < prefixLength; ++i)
        {
            if (prefix[i] >= 0 && candidate[i] != static_cast<BYTE>(prefix[i]))
            {
                matches = false;
                break;
            }
        }
        if (!matches || !IsExecutable(candidate) ||
            !IsAccessible(candidate + 7, sizeof(LONG), false))
            continue;

        const LONG displacement = *reinterpret_cast<LONG*>(candidate + 7);
        BYTE* globalAddress = candidate + 11 + displacement;
        if (!IsAccessible(globalAddress, sizeof(void*), false))
            continue;
        void* currentSystem = *reinterpret_cast<void**>(globalAddress);
        if (currentSystem && !IsAccessible(currentSystem, sizeof(void*) * 2, false))
            continue;

        // More than one structurally valid entry is ambiguous: fail closed.
        if (resolved && resolved != candidate)
            return nullptr;
        resolved = candidate;
    }
    return resolved;
}

static bool ResolveInventoryEconRuntime()
{
'@
Replace-Required $runtimeAnchor $runtimeHelper 'GetEconItemSystem structural helper'

$getSystemAnchor = @'
    BYTE* getSystem = FindUniquePattern(client, getSystemPattern,
        sizeof(getSystemPattern) / sizeof(getSystemPattern[0]));
'@
$getSystemReplacement = @'
    BYTE* getSystem = InventoryResolveEconItemSystemFunction(client);
'@
Replace-Required $getSystemAnchor $getSystemReplacement 'GetEconItemSystem resolver'

# Music kits are real econ items too. The virtual model stores the music-kit ID
# in overrideDefinitionIndex; the client inventory item itself uses definition
# 1314 and dynamic attribute 166 (music id).
$attributesAnchor = @'
    bool ok = true;
    if (item.paintKit > 0)
'@
$attributesReplacement = @'
    bool ok = true;
    if (item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC)
    {
        const int musicId = static_cast<int>(item.overrideDefinitionIndex);
        return InventoryEconSetDynamic(object, 166, &musicId);
    }
    if (item.paintKit > 0)
'@
Replace-Required $attributesAnchor $attributesReplacement 'music econ attribute'

$definitionWriteAnchor = @'
        *reinterpret_cast<unsigned short*>(
            reinterpret_cast<BYTE*>(object) + 0x30) = item.overrideDefinitionIndex;

        if (!InventoryEconApplyAttributes(object, item))
'@
$definitionWriteReplacement = @'
        const unsigned short econDefinition =
            item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ?
                static_cast<unsigned short>(1314) : item.overrideDefinitionIndex;
        *reinterpret_cast<unsigned short*>(
            reinterpret_cast<BYTE*>(object) + 0x30) = econDefinition;

        // CEconItem packs origin/quality/level/rarity into the word at 0x32.
        // Keep origin/level/rarity at their factory defaults and project only
        // the user-selected 4-bit quality. Knives/gloves/agents use Unusual
        // when the virtual item remains at Default quality, matching client
        // inventory expectations for those domains.
        unsigned short* packedQuality = reinterpret_cast<unsigned short*>(
            reinterpret_cast<BYTE*>(object) + 0x32);
        if (IsAccessible(packedQuality, sizeof(unsigned short), true))
        {
            unsigned short quality = static_cast<unsigned short>(
                InventoryClampInt(item.quality, 0, 14));
            if (quality == 0 &&
                (item.slotDefinitionIndex == INVENTORY_SLOT_KNIFE ||
                 item.slotDefinitionIndex == INVENTORY_SLOT_GLOVE ||
                 item.slotDefinitionIndex == INVENTORY_SLOT_AGENT))
                quality = 3;
            *packedQuality = static_cast<unsigned short>(
                (*packedQuality & ~static_cast<unsigned short>(0x01E0u)) |
                static_cast<unsigned short>((quality & 0x0Fu) << 5));
        }

        if (!InventoryEconApplyAttributes(object, item))
'@
Replace-Required $definitionWriteAnchor $definitionWriteReplacement 'effective econ definition and quality'

$mirrorDefinitionAnchor = '        mirror.definitionIndex = item.overrideDefinitionIndex;'
$mirrorDefinitionReplacement = '        mirror.definitionIndex = econDefinition;'
Replace-Required $mirrorDefinitionAnchor $mirrorDefinitionReplacement 'mirror econ definition'

# Mirror every valid virtual item into the local client inventory. Containers
# remain local-only (no GC messages) but a valid client item definition can
# still be represented in the local SOCache just like weapons/gloves/agents.
$skipAnchor = @'
            if (!item.itemId ||
                item.slotDefinitionIndex == INVENTORY_SLOT_MUSIC ||
                item.slotDefinitionIndex == INVENTORY_SLOT_CONTAINER)
                continue;
'@
$skipReplacement = @'
            if (!item.itemId)
                continue;
'@
Replace-Required $skipAnchor $skipReplacement 'mirror all virtual item domains'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Completed native client inventory econ mirror: $InputPath"
