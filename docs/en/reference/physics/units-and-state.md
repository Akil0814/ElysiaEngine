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

`FromDensity` computes mass from square-meter area. `ExplicitMass` preserves computed center of mass and scales inertia to requested mass. Sensors contribute only when explicitly enabled. Existing actors use explicit mass/fixed rotation.

Distance joints offer rigid rest length or frequency/damping springs. Revolute joints offer local anchors, reference angle, limits, motor speed/max torque; connected bodies default noncolliding. Native constraints have finite stiffness; extreme torques can exceed nominal limits.

Friction/contact/sleep/CCD follow Box2D without preserving old numeric trajectories.

## 4. Debug Drawing

Default outlines share ordinary drawing's interpolated positions/shortest-angle rotation. Green means awake, blue sleeping/static, purple sensors; disabled bodies are hidden. Joint anchors interpolate too.

Inspector `Native AABB (physics step)` separately shows yellow axis-aligned bounds, neither rotated outlines nor CCD trajectories. `Previous / current physics poses` shows previous gray/latest white, both off by default.

Contacts/normals/velocity represent the latest completed step intentionally without interpolation. Deferred commits/debug mode changes refresh capture even during pause. Render-only frames still update collider interpolation factors.

## Terrain and Target Commands

Chain vertices/ghosts and Segment endpoints also use EU. The local front `(dy,-dx)` rotates with the body. Chain children cannot be edited as Colliders; resolve identity with `chain_collider` and `chain_segment_info`. `set_target_transform(handle, pose, dt)` accepts Kinematic bodies only and is submitted each fixed step. Without submissions current velocity persists; identical or natively sleepy targets clear old velocity.

Eight `JointState.details` alternatives expose typed settings/measurements and common `first`/`second` endpoint identities. Filter has no settings or reaction force/torque. Motor position targets/errors use body A local coordinates; Mouse targets/errors use world coordinates.

Filter and `collide_connected` follow the bundled Box2D solid-contact rules. They do not suppress native Sensor overlaps or change stateless queries. Use Collider filters or `CollisionQueryFilter` to exclude sensor/query targets.
