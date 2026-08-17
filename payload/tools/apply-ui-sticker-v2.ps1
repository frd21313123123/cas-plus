param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8
$modulePath = Join-Path $PSScriptRoot '..\src\ui\ui_inventory_sticker_v2.inc'
if (-not (Test-Path -LiteralPath $modulePath)) {
    throw "Sticker V2 module was not found: $modulePath"
}
$module = Get-Content -LiteralPath $modulePath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Sticker V2 anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# ui_redesign.inc/editor V2 have already been injected by apply-ui-redesign.ps1.
# Put the sticker overlay after them and before MenuWindowProc so it can reuse
# the shared cas+ UI primitives while its functions are visible to the proc.
$menuAnchor = 'static LRESULT CALLBACK MenuWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)'
$menuCount = ([regex]::Matches($source, [regex]::Escape($menuAnchor))).Count
if ($menuCount -ne 1) {
    throw "Sticker V2 MenuWindowProc anchor expected once, found $menuCount."
}
$menuIndex = $source.IndexOf($menuAnchor)
$source = $source.Substring(0, $menuIndex) + $module + "`r`n`r`n" +
    $source.Substring($menuIndex)

# The extended inventory stage still emits its old sticker draw helper. Keep it
# compiled as a rollback aid, but route the active overlay to the redesigned one.
Replace-Required `
    '            DrawInventoryStickerModal(memDC, inventoryModalSnapshot);' `
    '            CasUiDrawStickerModalV2(memDC);' `
    'sticker modal draw route'

# Modal input must win before sidebar/navigation and before the Inventory V2
# page consumes unknown clicks. This also makes clicks outside the dialog inert.
$routerAnchor = @'
        if (CasUiPrepareMenuClick(wnd, &mouseX, &mouseY,
            casUiClient.bottom - casUiClient.top))
            return 0;

        // RGB Color Picker Modal click handling
'@
$routerReplacement = @'
        if (g_inventoryStickerModal)
        {
            CasUiHandleStickerModalV2Click(mouseX, mouseY);
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        }
        if (CasUiPrepareMenuClick(wnd, &mouseX, &mouseY,
            casUiClient.bottom - casUiClient.top))
            return 0;

        // RGB Color Picker Modal click handling
'@
Replace-Required $routerAnchor $routerReplacement 'sticker modal input ownership'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Applied redesigned Inventory sticker modal: $InputPath"
