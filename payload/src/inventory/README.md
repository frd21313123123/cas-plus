# cas+ Inventory Changer

The inventory subsystem is split into independent layers and equip domains:

1. **Virtual inventory state** — persistent local item instances, fake item IDs and equip masks.
2. **Catalog** — read-only weapon/knife/paint metadata plus an `items_game.txt` generator.
3. **Weapon adapter** — schema-backed projection onto locally-owned `C_EconEntity` objects.
4. **Glove adapter** — separate projection onto `C_CSPlayerPawn::m_EconGloves`.
5. **Music adapter** — separate projection onto `CCSPlayerController::m_iMusicKitID`.
6. **Visual refresh backend** — optional, exact-pattern engine refresh calls for weapon subclass/composite material state.

The subsystem does **not** modify Steam Inventory or Game Coordinator state.

## Architecture

```text
items_game.txt ---> catalog generator ---> read-only catalog
                                            |
VirtualInventoryItem[] ---------------------+----> UI / compatibility
        |
        +--> versioned + checksummed local persistence
        |
        +--> domain/loadout resolver
                  |
                  +--> WeaponAdapter -> owned C_EconEntity
                  +--> GloveAdapter  -> pawn.m_EconGloves
                  +--> MusicAdapter  -> controller.m_iMusicKitID
                  |
                  +--> guarded visual refresh (weapon domain only)
```

## Implemented

### Virtual inventory

- Up to 64 independent persistent item instances.
- Stable local fake item IDs.
- Paint kit, seed, wear, StatTrak, quality and definition metadata.
- T / CT / both / none equip masks.
- Duplicate/delete.
- Paging and selected-item editor.
- Versioned binary persistence with checksum.
- Original-state capture and restore across disable, unequip, entity replacement, map transition and payload shutdown.

### Weapon/loadout domain

- `C_EconEntity`, `C_AttributeContainer` and `C_EconItemView` fields resolved at runtime through Source 2 SchemaSystem.
- Mandatory fields bounds-checked against reflected class sizes before any write.
- Optional quality/item-ID/init/attachment-dirty fields used only when present.
- Projection to every econ entity owned by the local pawn, not only the active weapon.
- Full-handle + address + identity validation before restore.
- Generic knife loadout slot.
- Add-current flow.
- `+ Weapon` catalog factory: a virtual weapon can be created without owning/holding it first.
- Ordinary weapon definition cycling; incompatible carried paint is reset when the weapon definition changes.
- T/CT compatibility from the catalog.

### Catalog

- Named firearm catalog.
- Full knife definition selector currently covering Bayonet, Classic, Flip, Gut, Karambit, M9, Huntsman, Falchion, Bowie, Butterfly, Shadow Daggers, Paracord, Survival, Ursus, Navaja, Nomad, Stiletto, Talon, Skeleton and Kukri.
- Baseline knife finish selector plus raw paint-ID fallback.
- `generate-inventory-catalog.ps1` parses a current `items_game.txt` and emits weapon definitions plus weapon/paint compatibility derived from `alternate_icons2` generated icon paths.

### Inventory operations

- Editable StatTrak counter (`Off`, `-100`, `-1`, `+1`, `+100`).
- Small validated quality allow-list and quality cycling.
- Reset helper in the operations layer.
- Duplicate starts unequipped and receives a fresh fake ID.

### Guarded weapon visual refresh

The fallback fields are not the whole visual pipeline in CS2. An optional backend therefore resolves current client functions by **unique executable patterns**:

- `C_CSWeaponBase::UpdateSubclass(this)`
- `C_CSWeaponBase::UpdateSkin(this, bool)`

Properties:

- fail closed if a pattern is missing or ambiguous;
- only operates on already-captured locally-owned weapon entities;
- requires the projected definition to exist in the local weapon catalog;
- caches definition/paint/seed/wear/StatTrak state;
- calls `UpdateSubclass` on definition changes;
- budgets `UpdateSkin` re-kicks instead of restarting composite-material work every frame;
- never writes a guessed `CModelState::m_hModel` pointer.

This is a materially better refresh path than fallback writes alone, but visual behavior still needs in-game validation after each CS2 update because the backend is pattern-dependent.

### Gloves domain

Current client schema exposes:

- `C_CSPlayerPawn::m_EconGloves`
- `C_CSPlayerPawn::m_bNeedToReApplyGloves`

Implemented:

- separate glove slot in the same persistent virtual inventory;
- `+ Gloves` factory;
- definition catalog for Broken Fang, Bloodhound, Sport, Driver, Hand Wraps, Moto, Specialist and Hydra gloves;
- T/CT equip masks;
- schema bounds checks;
- pawn address + identity capture/restore;
- projected definition, quality and fake item ID;
- `m_bNeedToReApplyGloves` lifecycle invalidation;
- no repeated dirty write every frame.

**Glove paint/wear is deliberately not projected yet.** Those values remain stored in the virtual item until a validated client-side econ-attribute mutation API exists.

### Music Kit domain

Current client schema exposes plain controller fields:

- `CCSPlayerController::m_iMusicKitID`
- `CCSPlayerController::m_iMusicKitMVPs`

Implemented:

- separate music slot in the persistent virtual inventory;
- `+Music` factory;
- compact named baseline catalog with raw numeric ID as the source of truth;
- `< Music` / `Music >` cycling;
- schema bounds checks;
- controller address + identity capture/restore;
- per-team equip masks using the shared loadout model;
- Music Kit ID projection;
- virtual item's StatTrak value maps to the local Music Kit MVP counter when that schema field is present;
- StatTrak `Off` restores the captured original MVP count.

## Update-safety rules

The runtime must fail closed:

- never use stale hard-coded `C_EconEntity`/pawn/controller field offsets;
- resolve game-facing fields by schema name;
- bounds-check every reflected field before write;
- validate full entity/object identity before restore;
- make game-memory writes only from the existing frame-stage game-thread bridge;
- keep persistence/file I/O on the payload worker thread;
- keep catalog parsing/generation outside the injected runtime;
- do not mutate engine containers whose current client layout/mutation semantics are not validated;
- optional pattern backends must require a unique executable match and may disable themselves independently.

## Remaining work

### Full generated item/paint catalog

Status: **generator implemented, generated table still to be promoted into the runtime**.

Remaining:

- run/review the generator against the current `items_game.txt` whenever Valve updates inventory data;
- check in a generated full weapon/paint compatibility table;
- resolve localization tokens from current localization resources;
- add category/search/filter UI without stealing keyboard focus from CS2;
- expose rarity metadata.

### Stickers and client econ attributes

Status: **schema + attribute IDs understood; safe client mutation adapter pending**.

Current schema exposes:

- `C_EconItemView::m_AttributeList`;
- `C_EconItemView::m_NetworkedDynamicAttributes`;
- `CAttributeList::m_Attributes` as `C_UtlVectorEmbeddedNetworkVar<CEconItemAttribute>`;
- `CEconItemAttribute` definition/value fields.

Current sticker attribute blocks are verified as:

```text
slot 0: 113 id, 114 wear, 115 scale, 116 rotation
slot 1: starts at 117
...
slot 4: ends at 132 rotation
```

A known `CAttributeList::SetOrAddAttributeValueByName` signature exists for **server.dll** plugin tooling. That does not justify calling it from this client payload. The runtime therefore does not reinterpret/replace `C_UtlVectorEmbeddedNetworkVar` or free/replace its storage based on guessed layouts.

This same boundary currently blocks safe glove paint/wear projection and sticker mutation.

### Custom names

`VirtualInventoryItem` already stores a 31-character local custom name. Remaining work is a non-focus-stealing editor and a fully validated client string/invalidation path.

### Agents

Agent support remains a separate future adapter. It should use the current pawn/controller model/character-definition lifecycle rather than weapon or glove logic.

### Additional local-only inventory operations

Remaining candidates:

- StatTrak swap between virtual items;
- sticker apply/scrape simulation after the attribute adapter is safe;
- local case/capsule simulation;
- collection/storage grouping;
- richer search/filter UI.

## Testing checklist

- Build Debug x64 and Release x64.
- Verify every build-time patch stage reports successful injection.
- Launch with changer disabled and verify no inventory-domain writes occur.
- Create a weapon through `+ Weapon` without holding it first.
- Add current weapon and verify it produces an independent virtual item.
- Duplicate an item and verify a fresh fake ID + unequipped copy.
- Equip different items for T and CT and switch teams.
- Switch weapon/knife definitions and verify safe restore on unequip/disable.
- Verify guarded visual refresh does not run when its pattern backend is unresolved.
- Create/equip gloves and verify definition restore after disable/respawn/map lifecycle.
- Create/equip a Music Kit and verify ID/MVP restoration.
- Drop/pick up weapons and verify stale captures are not restored into reused handles.
- Restart payload and verify persistence checksum/version loading.
- Break a mandatory schema name in a test build and verify that domain fails closed.
- Run catalog generator against the current `items_game.txt` and review generated compatibility counts before promoting the output.
