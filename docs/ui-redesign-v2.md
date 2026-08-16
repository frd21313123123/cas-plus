# cas+ UI Redesign V2

The second UI pass removes the legacy shifted Inventory editor from the active UI path.

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

## Input ownership

When the Inventory tab is active, redesigned Inventory screens consume their own clicks. Unknown detail-screen clicks are intentionally consumed rather than translated into coordinates of the old hidden editor. This prevents invisible legacy controls from firing underneath the redesigned UI. Modal input has even higher priority than the page.

## Live skin status

The editor exposes the runtime state of the current live weapon material path (`MATERIAL`, `SET`, `ATTR`) and a material-rebuild counter. The underlying runtime rebuilds current CS2 composite materials rather than relying only on the previous UpdateSkin-only path.
