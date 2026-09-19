# Units and public API
Despite this file's historical name, this page describes the Box2D-backed API.

## Units
A world has one immutable spatial scale, `units_per_meter` (default 100). Time is seconds, angles are radians and mass is kilograms. Density uses the Elysia `SurfaceDensity` wrapper in kg/m². There are no independent unit multipliers.

| Quantity | Public unit | Input conversion, S = units_per_meter |
|---|---|---|
| Position, length | EU | divide by S |
| Velocity | EU/s | divide by S |
| Acceleration, gravity | EU/s² | divide by S |
| Force | kg·EU/s² | divide by S |
| Linear impulse | kg·EU/s | divide by S |
| Torque | kg·EU²/s² | divide by S² |
| Angular impulse | kg·EU²/s | divide by S² |
| Rotational inertia about center of mass | kg·EU² | divide by S² |
| Angle, angular velocity | rad, rad/s | unchanged |
| Time, mass, density | s, kg, kg/m² | unchanged |

Outputs use the inverse conversions. Joint anchors, reaction forces and motor torque follow the same table. Spring frequency is Hz; damping ratio, friction and restitution are dimensionless. Linear/angular damping coefficients are inverse seconds.

Coordinates remain X-right, Y-down. Positive rotation, angular velocity and torque appear clockwise on screen. No Y reflection occurs in the adapter. Render APIs requiring degrees receive a conversion only at the rendering boundary.

## Creation and commands
`PhysicsWorld::register_object(owner, body_definition, collider_definitions)` copies definitions and returns a stable engine handle. `collider_id(handle, index)` returns a shape's engine identity. Shape definitions do not contain live IDs.

`PhysicsParticipant` supplies creation descriptions to Scene and receives a world/handle binding. It does not expose mutable simulation state. `PhysicsStepParticipant::fixed_update` supplies control once per simulated step.

Use `set_velocity`, `set_angular_velocity`, `apply_force`, `apply_impulse`, `apply_torque`, `apply_angular_impulse`, `set_transform` and `teleport_object`. Optional force/impulse points are world coordinates; omission applies at the center of mass. Position changes on a registered GameObject are not physics commands.

Use `body_state`, `render_pose` and `joint_state` for snapshots. Invalid handles return an empty optional. Rendering reads interpolated poses; simulation and gameplay read committed state.

`update_collider` replaces a copied description. `set_collider_enabled` changes only enabled state. Identical descriptions do not recreate shapes.

## Mass and joints
`FromDensity` derives mass from shape area in meters. `ExplicitMass` preserves shape-derived center of mass and scales rotational inertia with the requested total mass. Sensors contribute no mass unless explicitly enabled. Existing characters use explicit mass and fixed rotation.

Distance joints offer a rigid rest length or a spring with frequency and damping ratio. Revolute joints offer local anchors, reference/limit angles, motor speed and maximum torque. Connected bodies do not collide by default. Native constraints have finite stiffness: extremely large motor torques can push beyond nominal angle limits.

The backend uses Box2D's native friction, contact, sleep and CCD behavior; old numerical trajectories are not compatibility guarantees.
# Debug drawing

The default collider overlay follows the same interpolated position and shortest-angle rotation as rendering. Green outlines indicate awake bodies, blue outlines indicate sleeping/static bodies, and purple outlines indicate sensors. Disabled bodies are omitted. Joint anchors use interpolated body poses too.

The inspector exposes `Native AABB (physics step)` separately. These yellow axis-aligned boxes are Box2D bounds, not rotated collider outlines or swept CCD paths. `Previous / current physics poses` draws the preceding pose in gray and the latest simulated pose in white. Both overlays are off by default.

Contact points, contact normals and velocity vectors describe the latest completed physics step. They are deliberately not interpolated. Capture refreshes after deferred mutations and on debug mode changes, including while paused. Rendering frames without a physics step still update the collider interpolation factor.
