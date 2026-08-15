# CAS+ Feature Matrix

This branch turns the existing single-page visuals UI into a functional multi-page offline/training client while preserving the current Win32/no-CRT/manual-map payload constraints.

## Implemented

### Aim Lab
- Aim-guide overlay preview with three guide radii.
- Recoil-path visual guide.
- Click-target trainer with deterministic target relocation and hit/miss counters.
- Aim Assist Simulator: a preview-only crosshair smoothly interpolates toward the local trainer target.
- Three smoothing presets for the simulator (fast / medium / slow).
- Simulator settings persist in profile slots.
- Independent enable/disable controls for each helper.
- No game view-angle writes, mouse automation, target selection against players, or fire automation.

### Movement Training
- Jump timing cue.
- Left/right strafe cadence visualizer driven by the existing frame-stage cadence.
- Optional landing/reset cue.
- No movement or input automation.

### Visuals
- Existing ESP configuration page.
- Skeleton, history skeleton and aim-history skeleton configuration.
- Footstep visualization configuration.
- Glow configuration.
- Chams visible/occluded/glow-pass controls.
- Off-screen indicator configuration.
- ESP status flags and interactive preview.
- Existing skybox and model-highlight runtime remain intact.

### Misc & Diagnostics
- Entity-runtime health indicator.
- Skybox-runtime health indicator.
- Frame-stage hook health indicator.
- Game-window health indicator.
- Diagnostics panel toggle.
- Trainer-counter reset action.

### Configs
- Three independent local profile slots.
- Save, load and reset actions.
- Versioned profile format (`CASP`, version 2).
- Profile validation before data is applied.
- Existing visuals plus Aim Lab / Movement / Diagnostics settings are persisted.
- Aim Assist Simulator enable state and smoothing are persisted.
- Runtime model effects are resynchronized after profile load/reset.

## Build pipeline

Generated payload source is transformed in this order:

1. `apply-feature-foundation.ps1`
2. `apply-feature-runtime-sync.ps1`
3. `apply-safe-feature-suite.ps1`
4. `apply-aim-assist-simulator.ps1`
5. `apply-ui-scope-fix.ps1`
6. `apply-chams-render-pipeline.ps1`
7. MSVC compilation of the generated `dllmain.chams.cpp`

All transforms are fail-closed: if an expected source anchor is missing, the build stops instead of patching an unknown source layout.

## Intentionally not implemented

CAS+ does not add online competitive automation or anti-cheat-evasion functionality. In particular this branch does not implement automatic aiming/firing against players, resolver/backtrack targeting against live players, anti-aim/fakelag, network manipulation, anti-cheat bypasses, or hidden-information ESP intended for online competitive play.
