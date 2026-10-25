# Physics Units, Commands, and State Snapshots

This document describes current Box2D backend contracts. Games modify state through engine commands and read committed results through snapshots.

## 1. Spatial Units and Direction

Each world has immutable `units_per_meter`, default 100. EU means engine world units; time seconds, angles radians, mass kilograms. `SurfaceDensity` is kg/m² without separate scaling.

For `S = units_per_meter`, inputs convert to Box2D as follows:

| Quantity | Public units | Input conversion |
| --- | --- | --- |
| Position, length | EU | Divide by S |
| Velocity | EU/s | Divide by S |
| Acceleration, gravity | EU/s² | Divide by S |
| Force | kg·EU/s² | Divide by S |
| Linear impulse | kg·EU/s | Divide by S |
| Torque | kg·EU²/s² | Divide by S² |
| Angular impulse | kg·EU²/s | Divide by S² |
| Center-of-mass inertia | kg·EU² | Divide by S² |
| Angle/angular velocity | rad, rad/s | Unchanged |
| Time/mass/density | s, kg, kg/m² | Unchanged |

Outputs invert conversion. Anchors, reaction forces, motor torques use the same table. Spring frequency is Hz; damping ratios/friction/restitution dimensionless. Linear/angular damping coefficients use inverse seconds.

X points right, Y down; positive rotation/angular velocity/torque appears clockwise. The adapter does not flip Y. Degree-based rendering converts at drawing boundaries only.

## 2. Creation, Modification, and Reading

`PhysicsWorld::register_object(owner, body_definition, collider_definitions)` copies definitions and returns stable engine handles. `collider_id(handle, index)` queries shape identity; creation definitions contain no live IDs.

`PhysicsParticipant` supplies descriptions and receives world/handle binding without exposing writable state. `PhysicsStepParticipant::fixed_update` supplies control each step.

Use `set_velocity`, `set_angular_velocity`, `apply_force`, `apply_impulse`, `apply_torque`, `apply_angular_impulse`, `set_transform`, `teleport_object`. Optional force/impulse points are world coordinates, default center of mass. Direct registered `GameObject` position writes are not commands.

`body_state`/`render_pose`/`joint_state` return snapshots, empty `optional` for invalid handles. Rules use committed state; drawing interpolated poses. `update_collider` replaces copied descriptions; `set_collider_enabled` only toggles. Identical descriptions do not rebuild.

## 3. Mass and Joints

`BodyDefinition::mass_properties` selects one of three exclusive configurations: default `DensityMass{}` computes mass data from square-meter area and density; `ExplicitMass{mass}` preserves the density-derived center and scales inertia with total mass; `BodyMassData{mass, local_center, rotational_inertia}` independently specifies mass, local center, and inertia about the center of mass. Sensors contribute to density calculations only when `sensor_contributes_mass` is enabled.

`PhysicsWorld::set_mass_properties(handle, properties)` replaces the configuration; passing `DensityMass{}` restores density computation. `mass_properties(handle)` returns the committed configuration, while `body_state(handle)` returns actual mass, effective inertia, and `local_center` / `world_center`. Both queries return empty before native creation completes or after identity invalidation. `PhysicsParticipant` provides matching configuration accessors.

Only Dynamic bodies accept overrides; Static / Kinematic accept only `DensityMass`. Override mass must be finite and positive, center finite, and inertia finite and nonnegative; converted values and required reciprocals must be representable. Zero inertia has no torque response. Fixed rotation always has zero effective inertia while retaining the requested configuration. Collider replacement, density or mass-contribution changes, and enable changes reapply the current configuration; shape computation cannot overwrite complete custom data.

Changing the center at runtime preserves body-origin pose, angular velocity, and instantaneous origin velocity. The engine adjusts center velocity by `v_new = v_old + angular_velocity × (world_center_new - world_center_old)`; momentum and energy conservation are not promised. Joint local anchors remain unchanged. Enabled bodies wake; disabled bodies stay disabled. Callback payloads are copied into the FIFO. `true` means validated and accepted; execution rechecks identity and then-current geometry, skipping invalidated or no-longer-valid requests. Queue allocation exceptions follow the existing exception contract and do not commit configuration early.

Inertia uses kg·EU² about the center of mass, differing from the local-origin inertia in some older Box2D shape mass data. See the [independent mass properties receipt](../../integration/006-explicit-inertia-receipt.md) for migration and the seesaw example.

Distance joints offer rigid rest length or frequency/damping springs. Revolute joints offer local anchors, reference angle, limits, motor speed/max torque; connected bodies default noncolliding. Native constraints have finite stiffness; extreme torques can exceed nominal limits.

Friction/contact/sleep/CCD follow Box2D without preserving old numeric trajectories.

## 4. Debug Drawing

Default outlines share ordinary drawing's interpolated positions/shortest-angle rotation. Green means awake, blue sleeping/static, purple sensors; disabled bodies are hidden. Joint anchors interpolate too.

Inspector `Native AABB (physics step)` separately shows yellow axis-aligned bounds, neither rotated outlines nor CCD trajectories. `Previous / current physics poses` shows previous gray/latest white, with independent category switches.

Contacts/normals/velocity represent the latest completed step intentionally without interpolation. Deferred commits/debug mode changes refresh capture even during pause. Render-only frames still update collider interpolation factors.

## Terrain and Target Commands

Chain vertices/ghosts and Segment endpoints also use EU. The local front `(dy,-dx)` rotates with the body. Chain children cannot be edited as Colliders; resolve identity with `chain_collider` and `chain_segment_info`. `set_target_transform(handle, pose, dt)` accepts Kinematic bodies only and is submitted each fixed step. Without submissions current velocity persists; identical or natively sleepy targets clear old velocity.

Eight `JointState.details` alternatives expose typed settings/measurements and common `first`/`second` endpoint identities. Filter has no settings or reaction force/torque. Motor position targets/errors use body A local coordinates; Mouse targets/errors use world coordinates.

Filter and `collide_connected` follow the bundled Box2D solid-contact rules. They do not suppress native Sensor overlaps or change stateless queries. Use Collider filters or `CollisionQueryFilter` to exclude sensor/query targets.

## Typed Physics Debug Snapshots

`PhysicsDebugCapture` adds `Chains` and `Targets`. `PhysicsDebugJoint` owns `JointState`, endpoint pose history, local anchors, local axis and reference angle. `PhysicsDebugChain` owns actual vertices, child ColliderIds, ghosts, owner identity and pose history. `PhysicsDebugTarget` contains command-time pose, requested/effective target, step duration, pending flag and target contours. `PhysicsDebugSnapshot` adds `chains`, `targets` and `units_per_meter`, borrowing no world/native data; existing snapshots can still be drawn after world destruction.

Capture follows selected categories; base/detail joints share `Joints`. Disabled details do not build Chain/target contour data. Capture failure preserves the previous complete snapshot and mode, propagates the exception and leaves native motion unchanged. Target commands only update fixed-size diagnostic records. See [debug drawing](../../user-guide/tools/debug-draw.md) for toggles, interpolation and record lifetime.

`PhysicsDebugSnapshot::bodies` is captured with Shapes or BroadPhase and contains committed mass configuration and `BodyState` for each visible body, including colliderless bodies. Inspector consumes these owned snapshots. Values describe physical state at capture time without interpolation.
