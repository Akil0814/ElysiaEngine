# Architecture and ownership
`PhysicsWorld` is the Elysia boundary. Its private implementation owns the Box2D world, bodies, shapes, joints, engine-handle indexes, deferred commands and event mappings. Queries and gameplay never receive Box2D IDs.

Scene owns GameObjects and unregisters them before destruction. Registered owners are borrowed and must remain alive until removal commits. Definitions are copied; there is no second writable velocity/transform structure.

The core is split into world/lifecycle, queries, tiles, joints, debug capture and an internal unit-conversion type. The same conversion is used throughout.

Box2D is pinned through CMake FetchContent with a SHA-256 checksum. Applications may use CMake's standard FetchContent source override for an already verified local source checkout. No Box2D samples or standalone tests are built as part of Elysia.

Fixed steps default to 1/60 second with four Box2D substeps. The accumulator and render interpolation belong to Elysia. Simulation state belongs to Box2D.
