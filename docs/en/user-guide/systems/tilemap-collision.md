# Tile Map Collision Integration

Maps describe collisions through `ITileCollisionWorld`; games choose drawing, level data, and editing. Enable scene physics following the [physics guide](physics.md), then bind a long-lived map.

## Binding and Release

Implement dimensions, world origin, cell size, bounds policy, and contents. Rectangular cells, negative origins, and nonsquare sizes work; slopes, partial cells, chunk streaming, and merged contours are absent.

Call this complete helper during integration while physics is not advancing. It demonstrates binding only. Keep `tiles` alive throughout binding, call/check `clear_tile_world(tiles)` before destruction. Unbinding requested during simulation callbacks may defer; release only after actual commit.

```cpp
#include "engine/physics/physics_world.h"
#include "engine/tools/logger.h"

bool attach_collision_map(elysia::physics::PhysicsWorld& world,
                          const elysia::physics::ITileCollisionWorld& tiles)
{
    // The world borrows the map; true does not transfer ownership.
    if (!world.set_tile_world(tiles))
    {
        ELYSIA_LOG_ERROR("physics", "Invalid collision map dimensions or origin.");
        return false;
    }
    return true;
}

bool refresh_collision_cell(elysia::physics::PhysicsWorld& world,
                            elysia::physics::TileCoordinate coordinate)
{
    // Edit game map data first, then notify physics. Both endpoints are in the dirty region.
    if (!world.update_tiles(coordinate, coordinate))
    {
        ELYSIA_LOG_ERROR("physics", "No bound map or invalid dirty coordinates.");
        return false;
    }
    return true;
}
```

Binding validates finite origin, finite positive sizes, nonnegative dimensions, and at most one million cells. `Block` creates four boundary walls; `Empty` none. Binding success does not validate every custom cell parameter; invalid shape descriptions generate no collider.

## Modifying Maps

After data changes, call `update_tiles(begin, end)` with in-map, ascending inclusive coordinates. It refreshes only that region. Rebind when sizes/dimensions/origin/bounds policy change. Simulation-time changes may defer; edit/refresh at safe phases and always notify the world.

## Dropping Through One-Way Platforms

From current contacts, obtain actor `ColliderId` and support `CollisionTarget`, call/check `request_pass_through(actor, support)`. Invalid actors, missing targets, absent rules, or prohibited supports return false.

Success wakes the actor and suppresses that pair until sufficient separation. Request every support pair when spanning cells. This neither disables the whole platform nor sets downward velocity; games own drop input/movement rules.

See [physics reference](../../reference/physics/README.md) for APIs and [tile map architecture](../../architecture/subsystems/physics/06-tile-map-collision.md) for adjacent faces/one-way decisions.
