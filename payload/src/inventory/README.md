# cas+ Inventory Changer

The inventory subsystem is a local client-side virtual inventory with guarded CS2 integration. It does **not** create Steam/Game Coordinator ownership and does not send GC inventory messages.

## Architecture

```text
items_game.txt -> offline catalog generator -> read-only catalog metadata
                                           |
VirtualInventoryItem[] --------------------+----> Inventory UI
        |                                  |
        |                                  +----> local operations
        |
        +--> v2 checksummed persistence
        +--> sticker/group sidecars
        |
        +--> LocalEconBackend
        |       +--> CEconItem factory
        |       +--> SharedObject type cache
        |       +--> SOCreated/SOUpdated/SODestroyed
        |       +--> dynamic econ attributes
        |       +--> EquipItemInLoadout
        |
        +--> WeaponAdapter -> locally-owned C_EconEntity / C_EconItemView
        +--> GloveAdapter  -> pawn.m_EconGloves
        +--> Agent loadout -> clothing custom-player slot
        +--> MusicAdapter  -> controller music fields
        +--> guarded visual refresh
```

All game-facing offsets that are available through Source 2 SchemaSystem are resolved by schema name and bounds-checked. Non-schema client functions are accepted only through exact unique executable-pattern resolution; an unresolved capability disables that path instead of falling through to guessed writes.

## Implemented functionality

### Virtual inventory and persistence

- Up to 64 independent item instances.
- Stable local virtual item IDs.
- Weapon/knife/glove/agent/music/container domains.
- Paint kit, seed, wear, StatTrak, quality and definition metadata.
- T / CT / both / none equip masks.
- Add current weapon and catalog-driven `+ Weapon` creation.
- Duplicate/delete with fresh virtual IDs.
- Selected-item editor and filtered paging.
- Existing `VirtualInventoryItem` v2 on-disk ABI is preserved.
- Versioned + checksummed base persistence.
- Stickers and collection/storage metadata use separate versioned sidecars keyed by virtual item ID so old v2 inventories remain readable.

### Local CEconItem / SharedObject backend

When the current client contract validates, virtual items are mirrored into real **local client** `CEconItem` objects:

1. resolve local `CCSPlayerInventory` and shared-object cache;
2. create the item through `CreateSharedObjectSubclassEconItem`;
3. preserve factory-owned custom-data state;
4. assign local item/account/inventory/definition metadata;
5. apply dynamic attributes through `CEconItemSchema` + `SetDynamicAttributeValue`;
6. add to the item type cache;
7. issue `SOCreated`;
8. equip through `EquipItemInLoadout` where applicable;
9. issue `SOUpdated` when local state changes.

Removal follows the reverse lifecycle: `SODestroyed` -> type-cache `RemoveObject` -> item destructor. Partially-created objects are destroyed on every checked failure path.

The backend never sends a GC message. If a resolver is missing or ambiguous, it stays disabled and the older schema-backed live-entity projection remains available where safe.

### Weapons and knives

- Runtime SchemaSystem projection onto locally-owned econ entities.
- All owned weapon entities are scanned, not only the active weapon.
- Original cosmetic/definition/item-view state is captured and restored on disable, unequip, entity replacement, map transition and payload shutdown.
- Full handle + address + identity checks prevent restoring into a reused entity handle.
- Generic knife loadout slot and full baseline knife definition selector.
- Weapon definition cycling with team compatibility.
- Local CEconItem item-ID binding when the shared-object backend is ready.
- Paint, seed, wear, StatTrak and quality editing.
- Empty custom names now clear previously projected names instead of leaving stale bytes.

### Guarded visual refresh

The visual layer has guarded client refresh functions for weapon subclass/composite-material state. Refreshes are state-cached and budgeted so material rebuild work is not restarted every frame.

No guessed `CModelState::m_hModel` pointer is written.

### Gloves

- Separate persistent glove domain and `+ Gloves` factory.
- Broken Fang, Bloodhound, Sport, Driver, Hand Wraps, Moto, Specialist and Hydra definition selector.
- T/CT loadout masks.
- `m_EconGloves` item-view projection.
- Client CEconItem binding supplies paint kit / seed / wear through validated econ attributes.
- Both halves of the wearable lifecycle handshake are used when exposed by the current schema: `m_bNeedToReApplyGloves` plus `m_nEconGlovesChanged`.
- Pawn identity capture/restore remains in the dedicated glove adapter.

### Stickers

Sticker state is stored independently from the v2 base item record. Five UI slots are supported, with:

- sticker ID;
- wear / scrape simulation;
- scale;
- rotation;
- X/Y placement offsets;
- physical sticker-schema anchor;
- stack reordering;
- remove;
- duplicate/delete persistence synchronization.

The client adapter uses engine-owned dynamic econ attributes instead of manually replacing `C_UtlVectorEmbeddedNetworkVar` storage. Current CS2 sticker attribute blocks, placement offsets and schema anchors are represented by the backend.

Sticker sidecar persistence file: `cas_plus_inventory_stickers_v1.bin`.

### Agents

- Dedicated agent virtual domain.
- T and CT baseline agent catalog.
- Agent items are mirrored into local CEconItem state and equipped through the clothing custom-player loadout slot.
- Agent sanitizer accepts the current high definition-index range rather than accidentally normalizing it as a weapon.
- Paint/seed/StatTrak are disabled for the agent domain.

The adapter deliberately relies on the engine loadout/character lifecycle rather than writing raw model handles. Whether an already-spawned pawn visually hot-swaps immediately is treated as runtime behavior to verify after CS2 updates; respawn/loadout application does not depend on a guessed model path.

### Music kits

- Separate music domain and `+ Music` factory.
- Named baseline music-kit selector with raw numeric ID as the stored source of truth.
- Schema-backed `m_iMusicKitID` projection.
- Optional MVP/StatTrak mapping through the controller field when present.
- Capture/restore of original controller values.

Music intentionally keeps its proven controller adapter: its virtual `overrideDefinitionIndex` stores a **music-kit ID**, not an econ item definition, so the generic CEconItem mirror does not fabricate a wrong item definition for this domain.

### Search and custom names

- Inventory search filters definition/custom-name/domain and drives filtered paging.
- Custom-name editor supports a local CS2-safe short name.
- The owned overlay remains `WS_EX_NOACTIVATE`.
- Keyboard input uses a reversible game-window WndProc subclass only while Search/Name editing is active.
- All other messages immediately delegate to the original WndProc.
- Original WndProc is restored during shutdown.

### StatTrak operations

- Editable StatTrak counter.
- Local StatTrak swap flow: arm a source item and click the target.
- Both items must have StatTrak enabled.
- Weapon swaps require the same item definition; music-to-music is permitted as its separate domain.
- Gloves/agents are rejected.

### Local containers / collection / storage groups

`Local Ops` provides intentionally local sandbox inventory operations:

- create a `Local Sandbox Case` virtual item;
- consume/open it into another local virtual inventory item;
- local randomized weapon/knife/seed/wear/StatTrak reward generation;
- persistent collection ID grouping;
- persistent storage ID grouping;
- grouping metadata follows duplicate/delete lifecycle.

These sandbox rolls **do not claim Valve/GC drop probabilities** and do not create server-owned items. Exact current case/capsule pools remain catalog data that should be generated from the current `items_game.txt` rather than guessed inside the injected runtime.

Group sidecar persistence file: `cas_plus_inventory_groups_v1.bin`.

### Catalog tooling

The checked-in runtime contains a stable baseline weapon/knife/paint/music catalog. `payload/tools/generate-inventory-catalog.ps1` can parse a supplied current `items_game.txt` and emit:

- weapon definitions;
- weapon/paint compatibility derived from generated icon paths;
- music-kit definitions.

This keeps large, frequently-changing game data outside the memory adapter. Updating the generated catalog is a data-refresh operation rather than a reason to change entity layouts or write paths.

## Build/update safety

- Every patcher has required exact anchors and refuses to patch blindly.
- Game-facing schema fields are bounds-checked before write.
- Pattern-resolved functions require a unique executable match.
- SharedObject object creation/removal has symmetric rollback paths.
- Persistence/file I/O stays on the payload worker thread.
- Game-memory projection stays on the existing frame-stage game-thread bridge.
- The no-CRT payload does not pull in the MSVC SEH runtime solely for optional inventory wrappers; generated wrappers are normalized after exact-count validation and retain pre-call capability guards.
- Direct engine container storage is not manually reallocated/reinterpreted.

## Validation checklist

After every CS2 client update:

1. Build Release x64 and confirm every inventory patch stage reports successful injection.
2. Launch with changer disabled and confirm there are no inventory-domain writes.
3. Verify LocalEconBackend capability resolution; unresolved capabilities must remain disabled.
4. Add/equip a firearm and knife for both teams; confirm disable restores originals.
5. Create gloves with non-zero paint/wear and confirm wearable rebuild + restore.
6. Add/edit/reorder/scrape stickers and confirm material refresh without per-frame rebuild loops.
7. Equip T/CT agents and verify loadout/respawn behavior.
8. Equip a music kit and verify ID/MVP restore.
9. Exercise custom-name/Search editing and verify normal game input resumes immediately when editing ends.
10. Duplicate/delete items and restart the payload; confirm base/sticker/group persistence stays synchronized.
11. Open a Local Sandbox Case and verify only local virtual state changes.
12. Break a mandatory schema/pattern in a diagnostic build and confirm that capability fails closed rather than using a guessed fallback.

## Boundary

This subsystem is a **local virtual inventory / cosmetic loadout implementation**. It intentionally stops at the client boundary: no Steam ownership spoofing, no Game Coordinator item creation, and no promise that local sandbox rewards represent real inventory items.