# Empty Inventory Browse runtime fix

The selector enumerates `cas_plus_game_catalog_v2.bin`. An empty item/finish list therefore means the process-bound game catalog is unavailable, rejected, or has no matching records rather than falling back to hand-written combinations.

This fix makes the loader catalog pipeline mandatory and observable:

- Source 2 prefab inheritance is resolved first.
- `pak01_dir.vpk` is read by streaming only its header/directory tree instead of loading the whole directory VPK behind a file-size ceiling.
- `alternate_icons2` generated icon paths accept logical econ paths as well as Panorama/compiled texture wrappers while still validating an exact real item + real paint-kit pair.
- payload catalog loading is requested by the UI and performed asynchronously by the worker rather than doing file I/O on the menu or frame callback.
- a rejected snapshot stays fail-closed and exposes its status plus an explicit `Retry catalog` action.
- no-results and catalog-error states are distinct: a search can be cleared without losing the draft, while an unavailable catalog reports its status and offers a retry.

No fallback Cartesian product of weapons and paint kits is generated; impossible combinations remain fail-closed.
