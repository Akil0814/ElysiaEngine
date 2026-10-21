# Physics Architecture and Ownership

This page explains responsibilities in current Box2D integration. Its filename retains early planning terminology; the body describes implemented architecture.

## Ownership of Simulation State

`PhysicsWorld` is Elysia's public physics entry point. Its private implementation owns the Box2D world, bodies, shapes, joints, engine-handle indexes, deferred commands, and event mapping. Queries and Gameplay receive no Box2D IDs.

Scenes own game objects and unregister physics before destruction. The world borrows registered objects, which must survive removal commit. Creation definitions are copied; games do not maintain a second writable set of simulated velocities or transforms.

## Module Responsibilities

Internals separate world lifecycle, queries, maps, joints, and debug capture. A shared internal unit-conversion type prevents inconsistent scales across features.

Default fixed steps are 1/60 second with four Box2D substeps. Elysia manages accumulation and render interpolation; Box2D owns simulation state. Scene ordering is in the [input architecture](../input/architecture.md#a-frame-and-a-fixed-step).

## Third-Party Dependency

Box2D 3.1.1 is pinned under `thirdparty/box2d`, with the source-archive checksum recorded there. Builds use checked-in source without downloads. This minimal snapshot excludes upstream examples, standalone tests, benchmarks, and documentation.
