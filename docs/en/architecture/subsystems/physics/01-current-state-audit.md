# Physics Lifecycle and Current Limits

This page describes current Box2D adapter lifecycles and usage boundaries. Implemented capabilities include static/kinematic/dynamic bodies, boxes, circles, capsules and convex polygons, mass policies, forces/impulses, sleeping, CCD, distance springs, revolute limits/motors, shape queries, map geometry, and gameplay event conversion.

## When Handles Become Invalid

Handles increase monotonically within a world and are not reused even after reset. Removal requests immediately reject new commands and state queries. Previously accepted commands are handled at safe points; joint updates revalidate both bodies and skip invalid endpoints. Body removal invalidates its joints too.

The adapter maps complete generation-bearing Box2D shape IDs to engine targets. After destruction, invalid mappings remain until the next event-extraction round completes. Gameplay events own values rather than borrowing Box2D or destroyed-object memory.

## Contacts and Listeners

Invalidating a contact produces one logical `End`. Rebuilding contacts for the same engine object pair produces `End` before `Begin`; reset clears contacts silently. Listeners are borrowed and must survive the current dispatch batch even after requesting removal.

## Discrete and Continuous Detection

Sensors detect overlap at step end and may miss an object that crosses entirely during one fixed step. Fast attacks should explicitly cast shapes and stop at the first blocking hit. Bullet bodies improve physical CCD but do not turn sensor events into continuous detection.

One-way platforms decide from geometry and velocity frozen before simulation. Box2D pre-solve callbacks have limitations for fast continuous contacts; validate platforms at actual gameplay speeds.

## Shapes, Maps, and Scale

Public shapes include boxes, circles, capsules and convex polygons with up to eight vertices, supporting local offsets and body rotation. Static/Kinematic Segment colliders and independent Chain terrain are also supported. Concave decomposition and ragdolls are outside this update. Existing character controllers still lock rotation.

Map `Block` out-of-bounds policy builds walls around a finite map, not infinite solid space arbitrarily far away. Installation is limited to one million cells.

Configurable world scale controls conversion, not stability at every scale. Converted dimensions must still meet Box2D precision requirements.

Chains own copied vertices/tags and a ColliderId per actual edge. Children are immutable; owner removal and Reset reclaim the group, and unfinished creation rolls back on exceptions. Open chains receive endpoint ghosts so every actual input edge participates; closed chains preserve winding.
