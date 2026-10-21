# Map Collision Implementation

`PhysicsWorld` borrows map descriptions and creates persistent static shapes for occupied cells, retaining coordinates, filters, materials, and optional one-way rules per cell. See the [map collision guide](../../../user-guide/systems/tilemap-collision.md) for binding, dirty refresh, and pass-through.

## Adjacency and One-Way Support

Adjacent solid cells with matching filters suppress shared internal contacts while exposed faces block. Dirty updates refresh adjacency next step; cell identity and materials remain independent.

One-way decisions use frozen current/previous bounds, contact normals, and accepted supports. Previous bounds retain approach direction, including when discrete contact appears after crossing a surface. Existing support survives penetration correction; slight solver velocity reversal does not immediately disable it.

Replacing/clearing maps invalidates old contacts and retires native IDs. Dirty updates rebuild only the selected region, without scanning the entire map every step.
