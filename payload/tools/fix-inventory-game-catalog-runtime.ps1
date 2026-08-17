param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Game-catalog runtime fix anchor '$Name' expected exactly once, found $count."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

$forwardAnchor = @'
static bool InventoryGameCatalogReady();
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
'@
$forwardReplacement = @'
static bool InventoryGameCatalogReady();
static bool ReloadInventoryGameCatalog();
static unsigned int InventoryGameCatalogTotalCount();
static bool InventoryGameCatalogLoadWasAttempted();
static unsigned int InventoryGameCatalogLoadFailureCount();
static unsigned int InventoryGameCatalogLastLoadStatus();
static int InventoryGameCatalogCyclePaint(unsigned short definitionIndex,
'@
Replace-Required $forwardAnchor $forwardReplacement 'catalog diagnostic forward declarations'

# A failed startup read must not leave Browse permanently blank. Entering a
# real game-backed category performs one explicit retry if the sidecar is not
# currently loaded.
$categorySelectAnchor = @'
                g_casUiBrowseCategory = i + 1;
                g_casUiCatalogPage = 0;
                g_casUiInventoryView = 2;
'@
$categorySelectReplacement = @'
                g_casUiBrowseCategory = i + 1;
                if (g_casUiBrowseCategory >= 1 &&
                    g_casUiBrowseCategory <= 9 &&
                    !InventoryGameCatalogReady())
                    ReloadInventoryGameCatalog();
                g_casUiCatalogPage = 0;
                g_casUiInventoryView = 2;
'@
Replace-Required $categorySelectAnchor $categorySelectReplacement 'category-entry reload'

# Empty game-backed categories are no longer a silent black panel. Show the
# sidecar state and a manual reload action directly in the redesigned page.
$drawAnchor = @'
    CasUiDrawLabel(hdc, L"ITEMS", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);
    for (int i = 0; i < visible; ++i)
'@
$drawReplacement = @'
    CasUiDrawLabel(hdc, L"ITEMS", CAS_UI_CONTENT_X, 104,
        180, 18, CAS_UI_MUTED_2, 10, 650, DT_LEFT);

    if (g_casUiBrowseCategory >= 1 && g_casUiBrowseCategory <= 9 &&
        total == 0)
    {
        DrawRoundedCard(hdc, CAS_UI_CONTENT_X, 138, 762, 192,
            CAS_UI_SURFACE, CAS_UI_BORDER, 8);
        CasUiDrawLabel(hdc,
            InventoryGameCatalogReady() ?
                L"Game catalog loaded, but this category has no records" :
                L"Game catalog is not loaded",
            CAS_UI_CONTENT_X + 24, 158, 650, 26,
            InventoryGameCatalogReady() ? CAS_UI_TEXT : CAS_UI_DANGER,
            15, 650, DT_LEFT);

        CasUiDrawLabel(hdc, L"Total records", CAS_UI_CONTENT_X + 24, 194,
            110, 20, CAS_UI_MUTED, 11, 500, DT_LEFT);
        RECT catalogTotalRc = { CAS_UI_CONTENT_X + 140, 194,
            CAS_UI_CONTENT_X + 220, 214 };
        DrawInventoryNumber(hdc, InventoryGameCatalogTotalCount(),
            &catalogTotalRc, DT_LEFT | DT_SINGLELINE);

        CasUiDrawLabel(hdc, L"Load status", CAS_UI_CONTENT_X + 250, 194,
            90, 20, CAS_UI_MUTED, 11, 500, DT_LEFT);
        RECT catalogStatusRc = { CAS_UI_CONTENT_X + 344, 194,
            CAS_UI_CONTENT_X + 410, 214 };
        DrawInventoryNumber(hdc, InventoryGameCatalogLastLoadStatus(),
            &catalogStatusRc, DT_LEFT | DT_SINGLELINE);

        CasUiDrawLabel(hdc, L"Failures", CAS_UI_CONTENT_X + 440, 194,
            70, 20, CAS_UI_MUTED, 11, 500, DT_LEFT);
        RECT catalogFailureRc = { CAS_UI_CONTENT_X + 514, 194,
            CAS_UI_CONTENT_X + 580, 214 };
        DrawInventoryNumber(hdc, InventoryGameCatalogLoadFailureCount(),
            &catalogFailureRc, DT_LEFT | DT_SINGLELINE);

        CasUiDrawLabel(hdc,
            InventoryGameCatalogLoadWasAttempted() ?
                L"The payload tried to load %TEMP%\\cas_plus_game_catalog_v1.bin." :
                L"The payload has not attempted to read the game catalog yet.",
            CAS_UI_CONTENT_X + 24, 222, 690, 22,
            CAS_UI_MUTED_2, 11, 400, DT_LEFT);
        CasUiDrawButton(hdc, CAS_UI_CONTENT_X + 24, 266,
            148, 34, L"Reload catalog", true);
        return;
    }

    for (int i = 0; i < visible; ++i)
'@
Replace-Required $drawAnchor $drawReplacement 'empty category diagnostics'

$clickAnchor = @'
        const int total = CasUiCatalogCount(g_casUiBrowseCategory);
        const int first = g_casUiCatalogPage * CAS_UI_GRID_PAGE;
'@
$clickReplacement = @'
        if (g_casUiBrowseCategory >= 1 && g_casUiBrowseCategory <= 9 &&
            CasUiCatalogCount(g_casUiBrowseCategory) == 0 &&
            CasUiPointInRect(mouseX, mouseY,
                CAS_UI_CONTENT_X + 24, 266, 148, 34))
        {
            ReloadInventoryGameCatalog();
            g_casUiCatalogPage = 0;
            return true;
        }
        const int total = CasUiCatalogCount(g_casUiBrowseCategory);
        const int first = g_casUiCatalogPage * CAS_UI_GRID_PAGE;
'@
Replace-Required $clickAnchor $clickReplacement 'manual catalog reload click'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8 -NoNewline
Write-Host "Added retryable game-catalog Browse diagnostics: $InputPath"
