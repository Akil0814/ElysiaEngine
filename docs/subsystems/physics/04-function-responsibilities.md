# Fixed-step execution
1. Snapshot registered participants and invoke active, live participants' fixed updates.
2. Commit deferred commands in order. A pending reset takes precedence.
3. Remove destroyed objects, synchronize activation and snapshot previous poses.
4. Freeze shape geometry, one-way rules, velocities and pass-through pairs.
5. Advance Box2D.
6. Copy contact/Sensor information into engine values while ID tombstones remain available.
7. Update engine contact state and write back owner transforms.
8. Dispatch events using a listener snapshot, then commit callback commands.

New objects participate in simulation after creation commits; their control callbacks begin on the following step. Render frames without a fixed step retain latched input.

Pre-solve reads only the frozen snapshot and the supplied manifold. It does not query/mutate the native world, dereference gameplay objects, dispatch gameplay callbacks or execute commands. A return value controls contact acceptance.

Use explicit teleport operations to reset position and interpolation. Reset is silent and stops remaining catch-up steps. Exceptions propagate; a completed simulation step is not rolled back.
