# Execution of a Physics Fixed Step

This page explains one internal physics step. Scenes first advance controllers and game fixed-step extensions, then simulation. The full scene flow is in the [input architecture](../input/architecture.md#a-frame-and-a-fixed-step).

## Simulation and Event Order

1. Validate step size and reject re-entry, then commit earlier deferred commands. A pending reset clears the world and ends this step.
2. Snapshot registered participants and call fixed updates for still-live, active participants.
3. Commit deferred commands in order; pending reset takes precedence.
4. Remove destroyed objects and synchronize activity.
5. Freeze current/previous geometry, map adjacency, one-way rules, supporting contacts, and pass-through pairs, then advance the previous-pose storage used for interpolation.
6. Advance Box2D simulation.
7. Copy contact/sensor information into engine values while invalid-ID mappings remain available.
8. Update engine contacts and write transforms to owning objects.
9. Dispatch through listener snapshots, then commit physics commands produced by callbacks.

Objects registered during participant fixed-update callbacks are absent from this step's participant snapshot. Once creation commits, they can join simulation this step, but their own fixed update begins next step. Objects registered outside a step participate on the next entered step. Render frames without a step preserve latched input for later consumption.

## Pre-Solve Callback Boundary

Pre-solve reads only frozen snapshots and the supplied contact manifold and decides acceptance through its return value. It does not query/mutate the native world, dereference game objects, dispatch gameplay callbacks, or execute commands.

Use explicit teleport operations for discontinuous position changes, resetting position and interpolation together. World reset silently clears state; reset requested from a participant skips the pending simulation. It does not reset or stop remaining independent `FixedStepRuntime` scheduling. Exceptions propagate; completed steps are not rolled back.

Target poses, Chains and joint creation/update/destruction use the same FIFO. Targets read the execution-time native pose and fixed rotation ignores target angles. Before simulation, Motor targets are resolved from body A local offsets. Updates immediately following creation execute in order. Chains reserve child identities before committing native edges/mappings; partial failures clean unfinished resources without rolling back completed simulation.
