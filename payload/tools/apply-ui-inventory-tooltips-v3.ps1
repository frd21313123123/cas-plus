param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath
)

$ErrorActionPreference = 'Stop'
$source = Get-Content -LiteralPath $InputPath -Raw -Encoding UTF8

function Replace-Required([string]$Needle, [string]$Replacement, [string]$Name) {
    $count = ([regex]::Matches($script:source, [regex]::Escape($Needle))).Count
    if ($count -ne 1) {
        throw "Inventory tooltip anchor '$Name' expected exactly once, found $count. Refusing to patch blindly."
    }
    $script:source = $script:source.Replace($Needle, $Replacement)
}

# Register tooltip hitboxes at the end of the native Item Editor draw. The
# tooltip renderer itself is shared with Visuals and runs after the backbuffer.
$itemTail = @'
    CasUiDrawButton(hdc, 582, 552, 106, 32, L"Swap ST");
    CasUiDrawButton(hdc, 694, 552, 104, 32, L"Reset");
}
'@
$itemReplacement = @'
    CasUiDrawButton(hdc, 582, 552, 106, 32, L"Swap ST");
    CasUiDrawButton(hdc, 694, 552, 104, 32, L"Reset");

    CasUiTooltipCandidate(438, 110, 498, 78,
        L"Item definition controls. For knives, gloves, agents and music kits the arrows cycle their matching catalog domain.", 1);
    CasUiTooltipCandidate(438, 198, 498, 54,
        L"Paint kit selects the finish. The catalog arrows move through compatible known paint kits; +/-1 changes the raw paint-kit ID.", 1);
    CasUiTooltipCandidate(438, 252, 300, 40,
        L"Seed changes the pattern seed used by paint kits that support pattern variation. Valid inventory values are clamped by the existing store sanitizer.", 2);
    CasUiTooltipCandidate(438, 294, 498, 72,
        L"Wear controls finish condition. FN, MW, FT, WW and BS are convenient condition presets over the stored float wear value.", 2);
    CasUiTooltipCandidate(438, 386, 412, 46,
        L"StatTrak stores the local counter used by the virtual item. Off disables it; the numeric buttons adjust the counter without changing the weapon definition.", 2);
    CasUiTooltipCandidate(438, 432, 498, 34,
        L"Quality is the econ entity-quality value for the virtual item. Cycle it only when you need a different quality classification.", 2);
    CasUiTooltipCandidate(438, 476, 498, 64,
        L"Loadout assignment chooses which local team loadout receives this virtual item: neither team, T, CT or both.", 2);
    CasUiTooltipCandidate(174, 552, 104, 32,
        L"Duplicate creates another virtual item with the same cosmetic state. The copy starts as a separate inventory record.", 3);
    CasUiTooltipCandidate(284, 552, 92, 32,
        L"Delete removes this virtual item and its associated local sticker state. It does not modify Steam inventory.", 3);
    CasUiTooltipCandidate(382, 552, 98, 32,
        L"Open the five-slot sticker editor for this weapon. Gloves, agents and music kits do not use weapon sticker slots.", 3);
    CasUiTooltipCandidate(486, 552, 90, 32,
        L"Edit the local custom-name buffer for this virtual item. Text capture is temporary and releases the game window procedure when editing ends.", 3);
    CasUiTooltipCandidate(582, 552, 106, 32,
        L"Arm a local StatTrak swap. Select a compatible target item to exchange counters using the existing inventory compatibility rules.", 3);
    CasUiTooltipCandidate(694, 552, 104, 32,
        L"Reset cosmetic fields on the selected virtual item back to their local defaults while keeping the inventory record.", 3);
    CasUiTooltipCandidate(CAS_UI_CONTENT_X + 16, 444, 218, 76,
        L"Runtime diagnostics for the in-match skin path. MATERIAL/SET/ATTR indicate which current client material and item-view backends resolved; rebuilds counts material kicks.", 4);
}
'@
Replace-Required $itemTail $itemReplacement.TrimEnd() 'Item Editor tooltip registration'

# Sticker tooltips describe engine/econ concepts rather than just repeating the
# button labels. Modal input already has higher priority than page controls.
$stickerTail = @'
    CasUiDrawLabel(hdc,
        L"Sticker changes rebuild the local econ item; no raw attribute-vector writes.",
        530, 480, 308, 32, CAS_UI_MUTED_2, 10, 400, DT_RIGHT);
}
'@
$stickerReplacement = @'
    CasUiDrawLabel(hdc,
        L"Sticker changes rebuild the local econ item; no raw attribute-vector writes.",
        530, 480, 308, 32, CAS_UI_MUTED_2, 10, 400, DT_RIGHT);

    CasUiTooltipCandidate(280, 202, 310, 28,
        L"Choose which sticker stack slot you are editing. The virtual inventory exposes five weapon sticker slots.", 4);
    CasUiTooltipCandidate(280, 246, 574, 96,
        L"Sticker ID selects the sticker definition. Scrape increases wear; Remove clears the selected slot from the local item state.", 3);
    CasUiTooltipCandidate(280, 352, 574, 54,
        L"Rotation and scale modify the sticker placement transform. Anchor chooses the physical sticker schema anchor on the weapon model.", 3);
    CasUiTooltipCandidate(280, 406, 574, 58,
        L"Offset X/Y moves the sticker relative to its selected model anchor. Small 0.01 steps keep placement changes controllable.", 3);
    CasUiTooltipCandidate(280, 480, 224, 32,
        L"Move left/right reorders the sticker stack while preserving the sticker's stored wear and placement fields.", 4);
    CasUiTooltipCandidate(786, 112, 68, 30,
        L"Close the sticker editor. Changes are already stored in the local virtual inventory sidecar.", 5);
}
'@
Replace-Required $stickerTail $stickerReplacement.TrimEnd() 'Sticker Editor tooltip registration'

# apply-ui-redesign changes the menu creation size to 980x620. The original
# PositionMenuOverGame helper still has a separate 780x500 pair used only for
# centering calculations; fix that remaining exact pair so the larger menu is
# actually centered over the CS2 client instead of shifted down/right.
$positionAnchor = @'
    constexpr int kWidth = 780;
    constexpr int kHeight = 500;
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
'@
$positionReplacement = @'
    constexpr int kWidth = 980;
    constexpr int kHeight = 620;
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
'@
Replace-Required $positionAnchor $positionReplacement.TrimEnd() '980x620 menu centering dimensions'

Set-Content -LiteralPath $InputPath -Value $source -Encoding UTF8
Write-Host "Applied Inventory/Sticker V3 tooltips and corrected menu centering: $InputPath"
