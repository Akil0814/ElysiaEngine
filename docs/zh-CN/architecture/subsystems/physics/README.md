# Physics
Elysia uses Box2D 3.1.1 for simulation and spatial queries. Gameplay uses Elysia handles, definitions, commands and events; Box2D headers are private implementation details.

## Guide
- [Units and public API](09-v1-implementation-reference.md)
- [功能与接口总览](10-physics-features-and-api-guide.md)
- [Architecture and ownership](02-target-architecture.md)
- [Lifecycle and current limitations](01-current-state-audit.md)
- [Responsibilities](03-class-responsibilities.md)
- [Fixed-step execution](04-function-responsibilities.md)
- [Collision and queries](05-collision-algorithms.md)
- [Tile maps](06-tile-map-collision.md)
- [Gameplay events](07-gameplay-collision-runtime.md)
- [Validation and performance](08-implementation-roadmap-and-tests.md)

The Physics menu contains the existing combat demos and **Box2D Physics Lab**. The lab demonstrates rotating stacks, sleeping bodies, a distance spring, a limited pendulum, a motor and a fast circle. Space applies an impulse; R recreates the scene; Escape returns to the menu.

The old integrator, broad phase, detection strategies and impulse solver have been removed. There is no alternative backend or strategy injection API.
