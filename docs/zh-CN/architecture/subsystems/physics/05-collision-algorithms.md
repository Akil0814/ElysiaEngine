# Collision and queries
Box2D owns broad phase, narrow phase, continuous collision and constraint solving. Elysia contains no duplicate solver.

Collision filtering maps engine category/mask/group rules to native shapes. Overlap shapes become sensors; Ignore shapes have no native collision shape. Material uses friction and restitution.

Ray and segment casts return nearest or all hits. AABB/circle overlap uses exact shape overlap rather than accepting broad-phase bounds alone. AABB sweep can hit circles and rotated boxes. Initial overlap is included explicitly with fraction zero and no unique impact normal.

Results use engine IDs and EU distances. All-hits are ordered by fraction and engine target, with duplicate shape hits removed. The query filter can exclude the casting object's collision category. Overlap results identify shapes; they do not synthesize a penetration manifold.

For high-speed attacks, cast the attack shape, process hits in travel order and stop at a blocking obstacle. Do not substitute a Sensor Begin event for this query.

See the [Box2D simulation documentation](https://box2d.org/documentation/md_simulation.html) for native CCD and contact semantics.
