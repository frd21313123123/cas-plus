# Inventory Attachments V2

This layer keeps stickers, agent patches and weapon keychains aligned with the exact installed CS2 build.

## Data source

`loader.exe` parses `scripts/items/items_game.txt` plus the current localization and VPK resources before injection and writes `%TEMP%/cas_plus_attachment_catalog_v1.bin`.

The attachment catalog contains only definitions discovered in the game files:

- sticker kits with `sticker_material`;
- agent patches with `patch_material`;
- `keychain_definitions` with localized names, inventory image resources and pedestal models.

Records are deduplicated by `(kind, id)` and the binary snapshot is versioned and checksummed.

## Projection

The payload loads the snapshot fail-closed. The UI cannot cycle to arbitrary raw IDs when the game-backed attachment catalog is unavailable.

- weapons expose the current game's sticker slot count and a single Charm/Keychain editor;
- agents expose the current game's patch slot count (currently three in `game_info`);
- keychain state persists in a separate versioned sidecar keyed by the virtual item ID;
- duplicate/delete/reset operations keep attachment sidecars synchronized.

The `C_EconItemView` fallback uses Source 2 attribute names. Integer-valued sticker/keychain IDs and schemas are passed as their exact 32-bit float bit representation, while wear/placement values remain numeric floats. Empty sticker slots are explicitly zeroed so removing an attachment cannot leave stale attributes in an existing view.

Keychain projection currently uses the schema-supported fields mirrored by the open-source CounterStrikeSharp inventory simulator: ID, seed/pattern and X/Y/Z offsets.

## Boundaries

This remains local cosmetic/loadout state. It does not create Steam inventory ownership and does not send GC mutations.
