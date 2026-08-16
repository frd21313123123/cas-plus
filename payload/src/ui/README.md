# cas+ UI redesign

The redesigned menu keeps the existing no-CRT Win32/GDI rendering model while moving the product toward a reusable application shell.

## Phase 1

- persistent left navigation rail
- unified dark/purple design tokens
- shared page chrome and typography
- 980x620 content canvas
- card-based Inventory home screen
- Add Items category browser
- catalog item cards for weapons, knives, gloves, agents and music kits
- Inventory search integration
- legacy item editor embedded as a detail page so no existing controls are lost during migration
- legacy feature panels rendered through a translated viewport inside the new shell

## Next phases

1. Replace the embedded legacy item editor with redesigned controls.
2. Add thumbnails sourced from current game resources/catalog metadata.
3. Convert Visuals, Ragebot, Anti-Aim and Configs to reusable section/card components.
4. Add hover, pressed and disabled states plus tooltips.
5. Add responsive density rules for smaller game resolutions.

The reference screenshots are used only as layout/UX direction. cas+ keeps its own branding, colors and component implementation.
