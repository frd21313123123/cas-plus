# Inventory changer reliability upgrade (2026-09)

## Outcome

The inventory UI and projection path now fail closed around a process-bound installed-game catalog. A user selects an item first, then one of that item's verified finishes, reviews wear/seed/StatTrak parameters, and explicitly commits with `Add item` or `Save changes`. Adding creates an unequipped local inventory item and opens it in My items; loadout changes are a later, separate action. Invalid legacy records remain visible for repair but are neither relabelled as a plausible skin nor projected into the client.

The design was compared with the durable interaction patterns in [advancedfx/nSkinz](https://github.com/advancedfx/nSkinz) (searchable in-game configuration and explicit overrides) and the local SharedObject lifecycle shown by [sgp729/o2](https://github.com/sgp729/o2/blob/cs2/inventory_changer.cc). Both repositories are archived/outdated for current CS2; no signatures, offsets, or implementation code were copied. The useful pattern retained here is separation between catalog, draft UI, persisted local item, and runtime projection. Arbitrary weapon/finish combinations from older changers were intentionally not copied.

## Reliability changes

- The loader resolves prefab inheritance and `alternate_icons2` pairs from the installed files. It writes catalog v2 transactionally and binds it to the target PID plus process creation time.
- The payload validates header, exact record count/size, checksum, strings, duplicates and process identity before atomically publishing an immutable catalog.
- Catalog file I/O runs on the worker. The menu queues reloads; frame and menu callbacks only consume the published snapshot.
- Runtime appearance signatures exclude item ID, equip flags and padding, but include meaningful cosmetic and attachment state. Lock contention causes a retry, not a fictitious empty inventory.
- Attribute/material work is change-driven and retry-budgeted instead of being restarted every frame.
- Runtime team masks are intersected with catalog compatibility without rewriting the user's persisted record.
- Shutdown coordinates the inventory frame callback and worker. If the bounded drain cannot prove safety, cleanup leaves game-owned client objects alone instead of risking a use-after-free.
- The build uses one explicit ordered payload manifest and writes only an intermediate/generated translation unit. It no longer runs compatibility stages that mutate other generator scripts.

## Selector behavior

- Four catalog domains: weapons, knives, gloves and agents.
- Independent item and compatible-finish searches, keyboard focus/navigation, mouse wheel paging, stable ID-based selection, and 32-34 px controls.
- A real changer on/off control backed by `g_inventoryStore.enabled` is visible in both selector and My items.
- A compatible team mask is enforced during runtime projection; the item editor controls equip state after creation.
- Invalid numeric input remains a draft error. Fields that do not apply to a default finish or agent cannot block that selection.
- No hash-derived rarity, invented artwork, Cartesian-product fallback, or raw paint-ID increment/decrement path.
- Separate no-results, inventory-busy/full, and catalog-unavailable states.

## Verification performed

- Loader Release x64 build: passed.
- Payload Release x64 build: passed.
- Catalog parser/validation regression: 56 checks passed, including malformed/truncated/stale snapshots and false pair prevention.
- Selector regression: 61 assertions passed, including compatibility, search, numeric bounds, lock contention, explicit submit without implicit equip, edit-draft preservation, full inventory, keyboard repeat protection and catalog failure.
- Runtime policy suite: passed, including semantic signatures, attachment lock contention, snapshot busy behavior, compatible-team projection, retry wraparound and mirror removal.
- Generated payload was produced twice with the same SHA-256, and the test verified that no maintained source/generator file was rewritten.
- The production GDI selector was rendered offscreen for ready, no-results and catalog-error states and inspected visually.

The harness never starts CS2, calls `DllMain`, injects the payload, or opens the user's real inventory files. A current-game session is still required to verify entity/material behavior and the best-effort restoration of original engine-owned dynamic attributes after a CS2 update.
