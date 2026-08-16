# cas+ UI Redesign V2

The second UI pass removes the legacy shifted Inventory editor from the active UI path and starts moving feature pages onto reusable native controls.

## Inventory detail screen

The native detail screen is implemented in `payload/src/ui/ui_inventory_editor_v2.inc` and owns rendering and hit-testing for:

- item definition/catalog cycling;
- paint-kit catalog and raw paint adjustment;
- seed adjustment;
- wear presets;
- StatTrak controls;
- quality cycling;
- T/CT loadout assignment;
- duplicate/delete/sticker/name/StatTrak-swap/reset actions;
- live-skin backend status.

It calls the existing InventoryStore mutation helpers. No second inventory state model is introduced.

## Sticker editor

`payload/src/ui/ui_inventory_sticker_v2.inc` replaces the active legacy sticker overlay with the same V2 visual system. It owns slot selection, sticker ID, scrape/remove, rotation, scale, anchor, X/Y offsets and stack movement. Modal pointer input is routed before sidebar and page hit-testing, so controls underneath the overlay cannot fire accidentally.

## Shared interaction layer

`payload/src/ui/ui_interaction_v3_prelude.inc` adds one menu-local pointer state shared by every redesigned page. Buttons, value pills, inventory cards and sidebar items now have hover/pressed feedback. Tooltip candidates are selected while controls are drawn, then one tooltip is rendered over the final backbuffer. The interaction state comes only from the owned Win32 menu window; it does not poll or modify game input.

## Visuals V2

`payload/src/ui/ui_visuals_v2.inc` is the first non-inventory page migrated off the translated legacy panel. It reads and mutates the existing `ESPConfig` directly, so runtime render logic is unchanged. The page includes:

- master Visuals enable;
- skeleton, history skeleton and aim-history skeleton with RGB controls;
- footsteps and off-screen indicators with RGB controls;
- history-depth adjustment;
- glow and chams controls;
- chams target/style and visible/occluded colors;
- player-state flag toggles;
- contextual tooltips for the migrated controls.

Hands chams remains visibly unavailable until the existing runtime has a validated schema-backed local viewmodel/arms handle.

## Input ownership

When the Inventory tab is active, redesigned Inventory screens consume their own clicks. Unknown detail-screen clicks are intentionally consumed rather than translated into coordinates of the old hidden editor. This prevents invisible legacy controls from firing underneath the redesigned UI. Modal input has even higher priority than the page.

Visuals V2 also owns its page coordinates. Ragebot, Anti-Aim and Configs still use the translated legacy hit-testing until their native pages are migrated.

## Live skin status

The editor exposes the runtime state of the current live weapon material path (`MATERIAL`, `SET`, `ATTR`) and a material-rebuild counter. The underlying runtime rebuilds current CS2 composite materials rather than relying only on the previous UpdateSkin-only path.
