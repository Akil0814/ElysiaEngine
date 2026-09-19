# Current behavior and limitations
Implemented: static/kinematic/dynamic bodies, rotating boxes and circles, mass policies, force and impulse APIs, sleeping, CCD, distance springs, revolute limits/motors, shape queries, tile geometry and gameplay event translation.

## Lifecycle
Handles are monotonic and not reused within a world, including across reset. Removal immediately rejects new commands and state queries. Previously accepted commands commit in order before destruction. Body removal also invalidates its joints.

The adapter maps complete Box2D shape IDs, including generation, to engine targets. On destruction, mapping records become tombstones through the following completed event extraction. Gameplay events contain values, not pointers into Box2D or destroyed objects.

Contact invalidation produces exactly one logical End. A rebuilt contact produces End before Begin for the same engine pair. Reset clears silently. Listener storage is borrowed; listeners must remain alive through the current dispatch batch even if they request removal.

## Native behavior
Sensors detect end-of-step overlap; they do not guarantee detection of objects that pass completely through within one step. Use an explicit shape cast for fast gameplay hits and respect the first blocking hit. Bullet bodies improve physical CCD; this is distinct from Sensor events.

One-way decisions use immutable pre-step geometry and velocity. Box2D pre-solve has limitations for high-speed continuous contacts; one-way platforms should be validated at intended gameplay speeds.

Only boxes and circles are exposed. Rotation is supported, but capsules, arbitrary convex polygons and ragdolls are outside this migration. Existing character controllers lock rotation.

Tile Block out-of-bounds builds perimeter barriers around a finite map; it is not an infinite solid plane available at arbitrary remote coordinates. The map adapter currently limits installation to one million cells.

Box2D tolerances still require sensible physical sizes after conversion. A configurable EU scale is not a guarantee of numerical stability at arbitrary magnitudes.
