# Collision Processing and Spatial Queries

Box2D handles broad-phase candidates, precise collision, continuous collision, and constraint solving. Elysia adds filtering, results, and gameplay integration without maintaining a second solver.

## Filtering and Shape Responses

Engine category/mask/group filtering maps to native shapes. Overlap shapes become sensors; Ignore shapes create no native collision shape. Materials supply friction and restitution.

## Query Results

Ray and segment casts return nearest or all hits. AABB/circle overlap queries test precise shape overlap rather than accepting broad-phase bounds alone. AABB sweeps support circles and rotated rectangles. Initial overlap explicitly returns fraction zero, with no unique collision normal.

Results use engine IDs and world units (EU). All-hit results sort by fraction and engine target and deduplicate shape hits. Filters can exclude the caller's collision category. Overlap results identify shapes without extra penetration manifolds.

## Fast Attacks

Cast the attack shape, process hits in travel order, and stop at blocking geometry. Sensor `Begin` represents discrete sampled contact and cannot replace fast casts. Box2D native CCD/contact semantics are in its [simulation documentation](https://box2d.org/documentation/md_simulation.html).
