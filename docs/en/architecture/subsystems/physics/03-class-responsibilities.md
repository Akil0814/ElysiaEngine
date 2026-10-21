# Physics Component Responsibilities

Worlds, participants, definitions, and events separate runtime state from game integration. Use this table to locate the layer a change belongs in.

| Component | Responsibility |
| --- | --- |
| `PhysicsWorld` | Own world, handles, commands, steps, queries, and events |
| `PhysicsParticipant` | Supply creation definitions to scenes and receive world/handle bindings |
| `PhysicsStepParticipant` | Game control logic in each simulation step |
| `BodyDefinition` / `BodyState` | Creation input / read-only committed simulation output |
| `Collider` | Copyable shape, filter, material, and density description |
| `PhysicsUnits` | Shared spatial-unit conversion boundary |
| `ContactCache` | Engine Begin/Stay/End state, including logical invalidation |
| `GameplayCollisionRuntime` | Team/actor routing and attack-instance deduplication |
| `ITileCollisionWorld` | Borrowed map description and explicit dirty-region refresh |
| `JointHandle` | Engine identity for a distance or revolute joint |

To change character movement, modify control logic or submit physics commands. To change how attacks choose targets, examine gameplay roles, filtering, and explicit queries. Removed detection/solver strategies are not extension points.
