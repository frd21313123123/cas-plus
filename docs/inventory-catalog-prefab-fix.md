# Inventory catalog prefab fix

Current CS2 base item records such as weapon_deagle reference named prefabs for item_name, model_player, model_world, image_inventory and inherited class metadata. The loader catalog generator now resolves direct values first, then walks whitespace-separated prefab chains (including parent prefabs) with cycle/depth guards.

Ordinary weapon paint records are no longer discarded merely because a model path is unavailable: an existing live weapon entity already owns its view/world model. Definition-changing knife/glove/agent records remain fail-closed when no real game model can be resolved.

This fixes empty Browse categories such as Inventory -> Pistols when the installed items_game uses prefab-backed weapon metadata.
