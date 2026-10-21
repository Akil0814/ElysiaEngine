# Physics System

Physics uses Box2D 3.1.1 for simulation and spatial queries. Games use Elysia handles, definitions, commands, and events; Box2D identifiers and headers are implementation details.

This directory is for physics maintainers. Integration examples are in the [physics guide](../../../user-guide/systems/physics.md); public capabilities are collected in the [interface overview](../../../reference/physics/README.md).

## Read by Question

- [Units and Public Interfaces](../../../reference/physics/units-and-state.md): world units, commands, snapshots, and debug drawing.
- [Architecture and Ownership](02-target-architecture.md), [Component Responsibilities](03-class-responsibilities.md): ownership of simulation state and scene/adapter responsibilities.
- [Lifecycle and Limits](01-current-state-audit.md), [Fixed-Step Flow](04-function-responsibilities.md): handle invalidation, deferred commands, event extraction, and callbacks.
- [Collision and Queries](05-collision-algorithms.md), [Map Collision](06-tile-map-collision.md): filtering, continuous queries, one-way platforms, and map changes.
- [Gameplay Collision Events](07-gameplay-collision-runtime.md): converting physical contact into gameplay events.
- [Historical Validation and Performance](08-implementation-roadmap-and-tests.md): migration acceptance, microbenchmarks, and reproduction.

## Implementation Boundaries

The old integrator, broad phase, detection strategies, and impulse solver were removed. There is no selectable backend or solver-strategy injection. Extend Gameplay through control, filtering, one-way rules, and explicit queries.

Box2D Physics Lab demonstrates rotating stacks, sleeping, springs, limited pendulums, motors, and fast circles. Space applies an impulse, R rebuilds, Esc returns. Demos aid observation and do not replace automated tests or movement-feel acceptance.
