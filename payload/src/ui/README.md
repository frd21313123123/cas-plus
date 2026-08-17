# cas+ UI redesign

The redesigned menu keeps the existing no-CRT Win32/GDI rendering model while moving the product toward a reusable application shell.

## Implemented

- persistent left navigation rail
- unified dark/purple design tokens
- shared page chrome and typography
- 980x620 content canvas, now centered using its real dimensions
- shared menu-local hover and pressed-state tracking
- contextual tooltip renderer shared across redesigned pages
- card-based Inventory home screen
- Add Items category browser
- catalog item cards for weapons, knives, gloves, agents and music kits
- Inventory search integration
- native Inventory Item Editor V2
- native Sticker Editor V2
- Inventory/Sticker parameter tooltips for paint, seed, wear, StatTrak, quality, loadout and sticker placement
- native Visuals V2 page backed directly by the existing `ESPConfig`
- RGB color-picker reuse from redesigned Visuals controls
- runtime skin-backend status in the Inventory editor
- legacy feature panels preserved only for tabs that do not yet have a real native backing-state migration

## Interaction ownership

Redesigned pages consume their own pointer coordinates and never translate unknown clicks into hidden legacy controls. Modal input has priority over page input. Mouse hover/pressed state belongs only to the owned Win32 menu window and does not poll or modify game input.

## Remaining work

1. Add thumbnails sourced from current game resources/catalog metadata.
2. Migrate additional feature tabs only after a real backing state exists for those controls; do not create decorative settings that cannot affect runtime behavior.
3. Add responsive density rules for smaller game resolutions.
4. Continue consolidating old Visuals-only modal styling into the shared component system.

The reference screenshots are used only as layout/UX direction. cas+ keeps its own branding, colors and component implementation.
