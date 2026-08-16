# cas+ Inventory Changer

The inventory changer is intentionally split into two concepts:

1. **Virtual inventory state** — local item instances, fake item IDs, cosmetics and T/CT equip state.
2. **Game adapter** — schema-resolved writes which project the equipped virtual item onto locally-owned CS2 econ entities.

It does **not** modify Steam Inventory or Game Coordinator state.

## Current architecture

```text
VirtualInventoryItem[]
        |
        +--> local persistence (versioned + checksummed)
        |
        +--> per-team loadout resolver
                     |
                     v
            equipped virtual item
                     |
                     v
            Schema-backed adapter
                     |
                     v
       locally-owned C_EconEntity
```

### Implemented

- Fixed-size virtual inventory with up to 64 independent item instances.
- Stable locally-generated fake item IDs.
- Per-item paint kit, seed, wear and StatTrak values.
- Item definition override field for knife variants.
- Per-team equip masks: none, T, CT, or both.
- Generic knife slot: all knife definitions resolve to one loadout slot.
- Capture-current-weapon flow to create a virtual item from the active weapon.
- Automatic projection to every locally-owned econ entity, not only the active weapon.
- Original-value capture and restoration on disable/unequip/entity replacement/map transition/shutdown.
- Runtime offsets resolved through Source 2 SchemaSystem; mandatory fields are bounds checked before writes.
- Optional quality/item-ID/initialization/attachment-dirty fields are used only when present.
- Local persistence using a versioned binary image and checksum.
- Inventory UI with paging, selection, editing, deletion and T/CT loadout assignment.
- Runtime diagnostics in the UI: active definition, number of applied overrides and locally-owned econ entities discovered.

## Update-safety rules

The game adapter must fail closed:

- never use a stale hard-coded `C_EconEntity` offset;
- resolve fields by schema name at runtime;
- validate every resolved offset against the reflected class size;
- verify entity handles and object identity before restoring captured state;
- perform game memory writes only from the existing frame-stage game-thread bridge;
- keep persistence/file I/O on the payload worker thread, never in the render callback.

## Roadmap

### Phase 1 — virtual inventory / weapon cosmetics

Status: **implemented in PR #9**.

- item instances;
- fake IDs;
- paint/seed/wear/StatTrak;
- T/CT equip;
- local persistence;
- whole-owned-loadout application;
- safe restore.

### Phase 2 — item schema/catalog

Build a real `ItemSchema` layer instead of exposing raw numeric IDs:

- weapon definitions;
- paint kits and compatibility;
- knife definitions;
- localized/display names where safely obtainable;
- rarity/quality metadata;
- searchable/selectable UI.

The catalog should be read-only and separate from `VirtualInventoryItem` so game updates only affect the schema adapter.

### Phase 3 — knife/viewmodel refresh

Definition-index override is implemented, but a CS2 model/viewmodel refresh path must be resolved and validated before knife model swapping is considered complete.

Required work:

- identify the current schema-backed/model-system relationship for world model and viewmodel;
- use an engine-supported refresh/invalidation path rather than a raw guessed model pointer;
- capture/restore original model state;
- validate on respawn, weapon switch and map transition.

Until this exists, knife definition presets in the UI should be treated as **experimental**.

### Phase 4 — stickers and custom names

Add a structured cosmetic sub-object to each virtual item:

```text
VirtualInventoryItem
  +-- StickerSlot[5]
      +-- kit
      +-- wear
      +-- scale
      +-- rotation
```

Do not write guessed sticker offsets. Resolve the current CS2 attribute-list representation first and isolate it behind the game adapter. Custom-name writes should likewise use a validated string representation instead of assuming a raw character buffer.

### Phase 5 — gloves / agents / music kits

These are distinct equip domains and should not be forced through weapon `C_EconEntity` logic.

Add separate adapters:

- `GloveAdapter`
- `AgentAdapter`
- `MusicKitAdapter`

Each adapter owns its own schema validation, lifecycle and restore behavior while sharing the same virtual inventory/store layer.

### Phase 6 — inventory actions

Once item types are represented safely in the virtual backend, local-only actions can be added:

- duplicate/delete;
- name-tag operation;
- sticker apply/scrape simulation;
- StatTrak counter editing/swap simulation;
- local case/capsule opening simulation;
- collections/storage grouping.

These operations mutate only the local virtual inventory. They must not impersonate or modify server-owned Steam/GC inventory state.

## Testing checklist

- Build Debug x64 and Release x64.
- Launch with inventory disabled and verify no econ writes occur.
- Add the active weapon and verify a new independent virtual item appears.
- Equip different virtual items for T and CT and switch teams.
- Switch between primary/secondary/knife and verify the matching slot is applied.
- Drop/pick up weapons and verify stale captured entities are not restored into reused handles.
- Disable changer and verify all captured values are restored.
- Change map and verify no previous-map entity snapshot survives.
- Restart payload and verify persistence checksum/version loading.
- Break a mandatory schema field name in a test build and verify the adapter fails closed without writes.
