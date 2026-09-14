# Tile maps
`set_tile_world` builds persistent static shapes from a borrowed map description. Each occupied cell retains its tile coordinate, filter, material and optional one-way rule. Map lifetime must cover the binding.

After modifying cells, call `update_tiles(begin, end)` with an inclusive dirty rectangle. Only that region is rebuilt; the world does not scan the map every step. Replacing or clearing a map invalidates old contacts and retires native IDs safely.

Block out-of-bounds installs four perimeter barriers. Empty out-of-bounds installs none. Negative map origins and non-square tiles are supported.

One-way decisions compare exact pre-step shape bounds and relative velocity. A small native tolerance is retained for resting contacts. `request_pass_through` suppresses a pair until its frozen shape bounds no longer overlap.

This adapter exposes rectangular cells; slopes, half tiles, chunk streaming and merged contours are not included.
