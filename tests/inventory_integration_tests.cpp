// Compile the actual generated payload as an offline test translation unit.
// The renamed entry point is never invoked: no game, injection, worker thread,
// process scanning, or user inventory persistence is involved in this harness.
#include <cstdio>

#define DllMain InventoryRegressionUnusedEntry
#define _fltused InventoryRegressionFloatMarker
#include "dllmain.chams.cpp"
#undef _fltused
#undef DllMain

#include "../payload/tests/inventory_runtime_policy_tests.inc"
#include "../payload/tests/inventory_selector_tests.inc"
#include "gdi_snapshot.h"

static_assert(sizeof(VirtualInventoryItem) == 64,
    "The existing v2 user inventory record must remain compatible.");
static_assert(sizeof(InventoryGameCatalogHeader) == 32);
static_assert(sizeof(InventoryGameCatalogRecord) == 904);

static void InventoryTestDrawMenu(HDC dc, int width, int height)
{
    CasUiDrawRect(dc, 0, 0, width, height, CAS_UI_BG);
    CasUiDrawSidebarChrome(dc, height);
    // The same sidebar geometry and shared production components as WM_PAINT.
    for (int tab = 0; tab < 5; ++tab) {
        const bool selected = tab == g_espConfig.selectedTab;
        DrawRoundedCard(dc, 14, 116 + tab * 50, 130, 38,
            selected ? CAS_UI_ACCENT_SOFT : CAS_UI_SIDEBAR,
            selected ? CAS_UI_BORDER_HI : CAS_UI_SIDEBAR, 6);
        if (selected)
            CasUiDrawRect(dc, 14, 124 + tab * 50, 17, 146 + tab * 50,
                CAS_UI_ACCENT);
        CasUiDrawLabel(dc, CasUiTabTitle(tab), 28, 116 + tab * 50, 108, 38,
            selected ? CAS_UI_TEXT : CAS_UI_MUTED, 14, 500);
    }
    CasUiDrawInventoryScreen(dc, width, height);
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::printf("Usage: inventory-integration-tests <snapshot-output-directory>\n");
        return 2;
    }
    int failures = InventoryRunRuntimePolicyTests();
    failures += InventoryRunSelectorTests();
    const char* snapshots[] = {
        "selector-ready.png", "selector-no-results.png", "selector-catalog-error.png"
    };
    for (int scenario = 0; scenario < 3; ++scenario) {
        InventoryPrepareSelectorSnapshot(scenario);
        char path[2048]{};
        const int length = std::snprintf(path, sizeof(path), "%s/%s", argv[1], snapshots[scenario]);
        if (length < 0 || static_cast<std::size_t>(length) >= sizeof(path) ||
            !InventoryTestRenderPng(path, 980, 620, InventoryTestDrawMenu)) {
            std::printf("FAIL: offscreen production render, scenario %d\n", scenario);
            ++failures;
        } else {
            std::printf("PASS: offscreen production render: %s\n", path);
        }
    }
    std::printf("Inventory offline integration: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
