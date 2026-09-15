# Velocity -> cas-plus port

Branch: `feature/velocity-port`

This branch is a clean-room reimplementation of Velocity's gameplay feature surface on top of the existing cas-plus runtime. The public Velocity repository is used as a behavioral and architectural reference only. Existing cas-plus systems are preferred whenever they already cover the same feature.

## Integration policy

- Keep the cas-plus manual-map loader and payload lifecycle.
- Keep cas-plus visuals/chams/glow when they are already more complete.
- Keep the cas-plus inventory backend and catalog instead of replacing it with Velocity's changer.
- Reimplement combat, command processing, prediction-facing adapters, movement and gameplay helpers as native cas-plus modules.
- Fail closed when a current CS2 address/schema/usercmd layout cannot be validated.
- Do not import Velocity's VAC/protection/anti-analysis implementation.

## Feature matrix

| Area | Velocity surface | cas-plus port status |
| --- | --- | --- |
| Rage | target selection | core implemented, CS2 adapter pending |
| Rage | per-weapon groups | implemented |
| Rage | FOV | implemented |
| Rage | hitchance gate | implemented through adapter estimates |
| Rage | minimum damage | implemented through adapter estimates |
| Rage | min-damage override | implemented |
| Rage | hitchance override | implemented |
| Rage | hitbox selection | config implemented, scanner adapter pending |
| Rage | silent aim | config implemented, usercmd bridge pending |
| Rage | force body aim | config implemented, hitbox adapter pending |
| Rage | no spread | config implemented, spread/seed adapter pending |
| Rage | doubletap | state/charge scaffold implemented, tickbase/subtick bridge pending |
| Rage | backtrack | max tick policy implemented, lag record storage pending |
| Rage | extrapolation | math/core implemented, lag record integration pending |
| Legit | aimbot | implemented in normalized command core |
| Legit | smoothing | implemented |
| Legit | RCS | settings implemented, punch adapter pending |
| Legit | triggerbot | implemented with adapter hitchance |
| Legit | autowall | policy implemented, penetration adapter pending |
| Anti-Aim | pitch none/down/up | implemented |
| Anti-Aim | at targets | implemented |
| Anti-Aim | autoyaw crosshair/distance/health | implemented |
| Anti-Aim | manual left/right | implemented |
| Anti-Aim | hide shots | settings implemented, subtick bridge pending |
| Anti-Aim | avoid backstab | settings implemented, knife-threat query pending |
| Anti-Aim | movement correction | implemented |
| Movement | bunnyhop | implemented |
| Movement | airstrafe | implemented |
| Movement | slow walk | implemented |
| Movement | edge jump | implemented |
| Movement | edge stop | implemented |
| Movement | jump bug | implemented baseline |
| Movement | edge bug | implemented baseline |
| Movement | fast ladder | config present, ladder command adapter pending |
| Assistance | quick peek | implemented baseline |
| Assistance | duck peek | config present, subtick adapter pending |
| Assistance | auto revolver | implemented baseline |
| Assistance | auto scope | implemented baseline |
| Assistance | zeus bot | config present, weapon/target adapter pending |
| Assistance | knife bot | config present, weapon/target adapter pending |
| Misc | projectile trajectory | config present, render/trace adapter pending |
| Misc | bullet impacts | mapped to existing cas-plus visual pipeline where possible |
| Misc | scoreboard weapons | config present, HUD adapter pending |
| World | smoke removal | config present, render hook adapter pending |
| World | weather | config present, frame-stage adapter pending |
| World | scene modulation | mapped to cas-plus material/scene work where possible |
| Visuals | player/item chams | reuse cas-plus renderer |
| Visuals | glow | reuse cas-plus renderer |
| Visuals | overlays | reuse/extend cas-plus ESP instead of replacing it |
| Changer | guns | reuse cas-plus inventory backend |
| Changer | knives | reuse cas-plus inventory backend |
| Changer | gloves | reuse cas-plus inventory backend |
| Changer | agents | reuse cas-plus inventory backend |
| Changer | econ item system | reuse cas-plus inventory backend/catalog |

## Runtime order

The normalized command pipeline follows the same high-level dependency order required by Velocity-style features:

1. collect local/player state and targets
2. capture pre-movement state
3. Anti-Aim/autostop pre-pass where required
4. movement features
5. prediction-facing combat pass
6. Rage/Legit
7. duck/peek helpers and airstrafe post-pass where required
8. quick-peek return logic
9. apply normalized command back to the validated CS2 usercmd/protobuf representation

The core is deliberately independent from raw CS2 offsets. The CS2 bridge must validate every address and schema before it exposes data to the core.

## Source layout

- `payload/src/velocity_port/velocity_port.hpp`: stable feature model and adapter contract
- `payload/src/velocity_port/velocity_port.cpp`: no-CRT gameplay core
- `payload/src/velocity_port/velocity_bridge.hpp`: bridge contract for the host payload
- `payload/src/velocity_port/velocity_bridge.cpp`: frame/command dispatch and fail-closed runtime state

## Next integration gates

The branch is not considered gameplay-complete until these gates are satisfied:

1. current CS2 CreateMove/input hook resolved and validated
2. current usercmd/base-usercmd protobuf layout validated
3. local player, weapon, movement services and view angles adapted from schema data
4. entity/target snapshots adapted from the cas-plus entity lifecycle
5. prediction, tracing, penetration and lag records connected
6. menu controls wired to `cas_velocity::settings`
7. existing cas-plus inventory/visual modules exposed through the bridge
8. Debug and Release x64 builds pass without CRT regressions
