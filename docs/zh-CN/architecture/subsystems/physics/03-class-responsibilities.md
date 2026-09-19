# Responsibilities
| Component | Responsibility |
|---|---|
| PhysicsWorld | World ownership, handles, commands, fixed stepping, queries, events |
| PhysicsParticipant | Scene creation descriptions and world/handle binding |
| PhysicsStepParticipant | Per-step gameplay controls |
| BodyDefinition / BodyState | Creation input / read-only committed output |
| Collider | Copied shape, filtering, material and density description |
| PhysicsUnits | The single spatial conversion boundary |
| ContactCache | Engine Begin/Stay/End state, including logical invalidations |
| GameplayCollisionRuntime | Team/role routing and attack-instance deduplication |
| ITileCollisionWorld | Borrowed map description; explicit dirty-region refresh |
| JointHandle | Engine identity for a distance or revolute joint |

The removed detection/solver strategies are not extension points. Gameplay customization belongs in controls, filters, one-way rules or explicit queries.
