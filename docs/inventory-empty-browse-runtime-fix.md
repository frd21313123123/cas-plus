# Empty Inventory Browse runtime fix

The redesigned Browse page already gets rewritten at build time to enumerate `cas_plus_game_catalog_v1.bin`. A completely empty Pistols/Rifles page therefore means the game-backed sidecar is absent, rejected, or contains no matching records rather than a card-layout problem.

This fix makes the loader catalog pipeline mandatory and observable:

- Source 2 prefab inheritance is resolved first.
- `pak01_dir.vpk` is read by streaming only its header/directory tree instead of loading the whole directory VPK behind a file-size ceiling.
- `alternate_icons2` generated icon paths accept logical econ paths as well as Panorama/compiled texture wrappers while still validating an exact real item + real paint-kit pair.
- payload catalog loading is retryable rather than permanently caching the first failed read.
- entering a game-backed Browse category retries a missing catalog once.
- an empty category now shows total records, load status, failure count, and a `Reload catalog` action directly in the menu.

No fallback Cartesian product of weapons and paint kits is generated; impossible combinations remain fail-closed.
