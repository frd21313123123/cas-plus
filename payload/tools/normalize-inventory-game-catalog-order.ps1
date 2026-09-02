param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

$typeBlock = @'
#pragma pack(push, 1)
struct InventoryGameCatalogRecord {
    unsigned short definitionIndex;
    BYTE category;
    BYTE teamMask;
    int paintKit;
    wchar_t displayName[80];
    wchar_t weaponName[48];
    wchar_t finishName[64];
    char rarity[32];
    char iconResource[160];
    char modelPlayer[160];
    char modelWorld[160];
};

struct InventoryGameCatalogHeader {
    unsigned int magic;
    unsigned short version;
    unsigned short recordSize;
    unsigned int count;
    unsigned int checksum;
    unsigned int processId;
    unsigned long long processCreated;
    unsigned int schemaChecksum;
};
#pragma pack(pop)
'@
# The staging scripts deliberately normalize generated files to CRLF, while
# text-only fixtures may arrive with LF.  Compare a canonical line ending form
# so a harmless transport newline cannot make the ABI-order guard fail.
$source = $source.Replace("`r`n", "`n")
$typeBlock = $typeBlock.Replace("`r`n", "`n")
$typeCount = ([regex]::Matches($source, [regex]::Escape($typeBlock))).Count
if ($typeCount -ne 1) {
    throw "Game-catalog type block expected exactly once, found $typeCount. Refusing to reorder blindly."
}
$source = $source.Replace($typeBlock, '')

$prelude = @'
#pragma pack(push, 1)
struct InventoryGameCatalogRecord {
    unsigned short definitionIndex;
    BYTE category;
    BYTE teamMask;
    int paintKit;
    wchar_t displayName[80];
    wchar_t weaponName[48];
    wchar_t finishName[64];
    char rarity[32];
    char iconResource[160];
    char modelPlayer[160];
    char modelWorld[160];
};
struct InventoryGameCatalogHeader {
    unsigned int magic;
    unsigned short version;
    unsigned short recordSize;
    unsigned int count;
    unsigned int checksum;
    unsigned int processId;
    unsigned long long processCreated;
    unsigned int schemaChecksum;
};
#pragma pack(pop)

struct VirtualInventoryItem;
static bool LoadInventoryGameCatalog();
static void ShutdownInventoryGameCatalog();
static bool InventoryGameCatalogReady();
static bool InventoryGameCatalogValidatePair(unsigned short definitionIndex, int paintKit);
static bool InventoryGameCatalogValidateItem(const VirtualInventoryItem& item);
static void ProcessInventoryGameCatalogLoadRequests();
static const InventoryGameCatalogRecord* InventoryGameCatalogAt(unsigned int index);
static const InventoryGameCatalogRecord* InventoryGameCatalogFindPair(
    unsigned short definitionIndex, int paintKit);
static const InventoryGameCatalogRecord* InventoryGameCatalogFindFirstDefinition(
    unsigned short definitionIndex);
static int InventoryGameCatalogCountCategory(int category);
static const InventoryGameCatalogRecord* InventoryGameCatalogCategoryOrdinal(
    int category, int ordinal, int* absoluteIndex);
static int InventoryGameCatalogCountForDefinition(unsigned short definitionIndex);
static const InventoryGameCatalogRecord* InventoryGameCatalogOrdinalForDefinition(
    unsigned short definitionIndex, int ordinal, int* absoluteIndex);
static const wchar_t* InventoryGameCatalogFinishName(
    const InventoryGameCatalogRecord* record);
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
    int currentPaint, int direction);
static const InventoryGameCatalogRecord* InventoryGameCatalogForItem(
    const VirtualInventoryItem& item);
static const wchar_t* InventoryGameCatalogDisplayName(
    const VirtualInventoryItem& item);
static const char* InventoryGameCatalogModelPlayer(
    const VirtualInventoryItem& item);
static const char* InventoryGameCatalogModelWorld(
    const VirtualInventoryItem& item);
static const char* InventoryGameCatalogIconResource(
    const VirtualInventoryItem& item);
static const char* InventoryGameCatalogRarity(
    const InventoryGameCatalogRecord* record);
static void InventoryGameCatalogSanitizeLoadedStore();
static bool InventoryGameCatalogApplyLiveView(BYTE* itemView,
    const VirtualInventoryItem& item);

'@
$anchor = 'struct RGBVal {'
$anchorCount = ([regex]::Matches($source, [regex]::Escape($anchor))).Count
if ($anchorCount -ne 1) {
    throw "Game-catalog early declaration anchor expected exactly once, found $anchorCount."
}
$source = $source.Replace($anchor, $prelude + $anchor)

# Later staging scripts intentionally use CRLF literal anchors.  Restore that
# canonical output convention after doing the newline-tolerant ABI match.
$source = $source.Replace("`n", "`r`n")

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Normalized game-catalog type/function declaration order: $InputPath"
