# Tile maps
`set_tile_world` builds persistent static shapes from a borrowed map description. Each occupied cell retains its tile coordinate, filter, material and optional one-way rule. Map lifetime must cover the binding.

After modifying cells, call `update_tiles(begin, end)` with an inclusive dirty rectangle. Only that region is rebuilt; the world does not scan the map every step. Replacing or clearing a map invalidates old contacts and retires native IDs safely.

Block out-of-bounds installs four perimeter barriers. Empty out-of-bounds installs none. Negative map origins and non-square tiles are supported.

Adjacent solid cells with matching filters suppress their shared internal contact faces. Exposed faces remain blocking, and dirty updates refresh adjacency on the next step. Cell identities and materials remain separate.

One-way decisions use frozen current and preceding shape bounds, the contact normal, and previously accepted support contacts. The preceding bounds retain the approach side when a discrete contact arrives after crossing the surface. Accepted supports remain enabled while penetration settles; tiny solver velocity reversals do not disable them.

`request_pass_through` accepts one-way supports only, wakes the actor, and suppresses each requested pair until the shapes separate beyond the resting-contact margin. Request every supporting pair when the actor spans multiple cells. The demo Gameplay helper does this automatically.

This adapter exposes rectangular cells; slopes, half tiles, chunk streaming and merged contours are not included.
