# cas+ Inventory Changer

The inventory changer is split into three layers:

1. **Virtual inventory state** — local item instances, fake item IDs, cosmetics and T/CT equip state.
2. **Catalog** — read-only weapon/knife/paint metadata used by the UI and compatibility selection.
3. **Game adapter** — schema-resolved writes which project the equipped virtual item onto locally-owned CS2 econ entities.

It does **not** modify Steam Inventory or Game Coordinator state.

## Current architecture

```text
items_game.txt ----> catalog generator ----> read-only catalog
                                             |
VirtualInventoryItem[] ----------------------+----> UI / compatibility
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
- Per-item paint kit, seed, wear, editable StatTrak and quality values.
- Item definition override field for knife variants.
- Per-team equip masks: none, T, CT, or both.
- Generic knife slot: all knife definitions resolve to one loadout slot.
- Capture-current-weapon flow to create a virtual item from the active weapon.
- Automatic projection to every locally-owned econ entity, not only the active weapon.
- Original-value capture and restoration on disable/unequip/entity replacement/map transition/shutdown.
- Runtime offsets resolved through Source 2 SchemaSystem; mandatory fields are bounds checked before writes.
- Optional quality/item-ID/initialization/attachment-dirty fields are used only when present.
- Local persistence using a versioned binary image and checksum.
- Inventory UI with paging, selection, editing, duplication, deletion and T/CT loadout assignment.
- Named weapon/knife catalog and knife cycling in the UI.
- Baseline knife finish catalog plus raw paint-ID fallback.
- `generate-inventory-catalog.ps1` can parse a current `items_game.txt` and derive weapon definitions plus weapon/paint compatibility from generated inventory icon paths.
- Runtime diagnostics in the UI: active definition, number of applied overrides and locally-owned econ entities discovered.

## Update-safety rules

The game adapter must fail closed:

- never use a stale hard-coded `C_EconEntity` offset;
- resolve fields by schema name at runtime;
- validate every resolved offset against the reflected class size;
- verify entity handles and object identity before restoring captured state;
- perform game memory writes only from the existing frame-stage game-thread bridge;
- keep persistence/file I/O on the payload worker thread, never in the render callback;
- keep catalog parsing/generation outside the injected runtime;
- never mutate an engine container whose layout/mutation API has not been validated.

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

Status: **baseline implemented; automatic catalog generation added**.

Implemented:

- weapon definition metadata;
- full knife definition selector;
- readable names in the inventory UI;
- baseline knife finish selector;
- raw ID fallback;
- offline generator for `items_game.txt`;
- automatic derivation of weapon/paint compatibility using `alternate_icons2` generated icon paths.

Remaining:

- check in a generated full paint compatibility table after generator validation;
- resolve localization tokens from the current CS2 localization resources;
- add category/search/filter UI without stealing keyboard focus from the game;
- expose rarity metadata in the catalog/UI.

### Phase 3 — knife/viewmodel refresh

Status: **under investigation**.

The current client schema confirms that `C_EconEntity` contains `m_hViewmodelAttachment`, `m_bAttachmentDirty` and `m_nUnloadedModelIndex`. `m_bAttachmentDirty` is already resolved optionally and marked after virtual item projection.

Definition-index override is implemented, but a CS2 world/viewmodel refresh path must be validated before knife model swapping is considered complete.

Required work:

- resolve the current client-side model refresh function or another engine-owned invalidation path;
- validate the relationship between econ entity, viewmodel attachment and scene/model state;
- capture/restore any additional model state touched by the adapter;
- validate on respawn, weapon switch and map transition.

No raw `CModelState::m_hModel` pointer write should be added as a shortcut.

### Phase 4 — stickers and custom names

Status: **schema/attribute research complete; mutation adapter pending**.

Current CS2 schema exposes:

- `C_EconItemView::m_AttributeList`;
- `C_EconItemView::m_NetworkedDynamicAttributes`;
- `CAttributeList::m_Attributes` as `C_UtlVectorEmbeddedNetworkVar<CEconItemAttribute>`;
- `CEconItemAttribute::{m_iAttributeDefinitionIndex,m_flValue,m_flInitialValue,...}`;
- five sticker slots in `items_game.txt`.

Current sticker attribute definitions follow the verified four-value blocks:

```text
slot 0: 113 id, 114 wear, 115 scale, 116 rotation
slot 1: starts at 117
...
slot 4: ends at 132 rotation
```

Do not directly reinterpret/mutate `C_UtlVectorEmbeddedNetworkVar` until its current client layout or an engine setter is validated. The server-side `CAttributeList::SetOrAddAttributeValueByName` signature is known from existing tooling, but it is not a valid substitute for a client-side adapter in this payload.

Custom-name storage is already part of `VirtualInventoryItem`; a dedicated non-focus-stealing text editor remains to be added.

### Phase 5 — gloves / agents / music kits

These are distinct equip domains and should not be forced through weapon `C_EconEntity` logic.

Add separate adapters:

- `GloveAdapter`
- `AgentAdapter`
- `MusicKitAdapter`

Each adapter owns its own schema validation, lifecycle and restore behavior while sharing the same virtual inventory/store layer.

### Phase 6 — inventory actions

Status: **partially implemented**.

Implemented local-only operations:

- duplicate/delete;
- editable StatTrak counter;
- quality cycling for a small validated quality allow-list;
- reset helper in the operations layer.

Remaining:

- name-tag editor;
- sticker apply/scrape simulation after the sticker attribute adapter is safe;
- StatTrak swap between virtual items;
- local case/capsule opening simulation;
- collections/storage grouping.

These operations mutate only the local virtual inventory. They must not impersonate or modify server-owned Steam/GC inventory state.

## Testing checklist

- Build Debug x64 and Release x64.
- Launch with inventory disabled and verify no econ writes occur.
- Add the active weapon and verify a new independent virtual item appears.
- Duplicate an item and verify the copy receives a new fake ID and starts unequipped.
- Equip different virtual items for T and CT and switch teams.
- Switch between primary/secondary/knife and verify the matching slot is applied.
- Cycle knife definitions and verify definition state changes without stale restore data.
- Edit StatTrak/quality and verify persistence across payload restart.
- Drop/pick up weapons and verify stale captured entities are not restored into reused handles.
- Disable changer and verify all captured values are restored.
- Change map and verify no previous-map entity snapshot survives.
- Restart payload and verify persistence checksum/version loading.
- Break a mandatory schema field name in a test build and verify the adapter fails closed without writes.
- Run the catalog generator against the current `items_game.txt` and review the generated weapon/paint compatibility counts before replacing the fallback catalog.
