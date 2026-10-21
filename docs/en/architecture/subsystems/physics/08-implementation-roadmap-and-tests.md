# Historical Physics Migration Validation and Performance

This page preserves acceptance and microbenchmark results from Box2D migration, not current working-tree test status. The original record omitted measurement dates; retain its environment, scope, and values without inventing dates. Current procedures are in the [testing guide](../../../contributing/testing.md).

## Historical Acceptance Coverage

Public-behavior tests replaced algorithm-specific tests, covering:

- Units, scale consistency, density, explicit mass, and inertia.
- Fixed callbacks, creation/removal, teleport ordering, and silent reset.
- Sensor End lifecycle and Gameplay routing after unbinding.
- Rotated queries, circular CCD, sleeping/waking, one-way platforms, and map dirty regions.
- Distance springs, revolute motor direction/limits, and joint invalidation.
- Real scene registration, input latching, damage, and rendering interpolation.

The historical Debug build passed all 84 CTest targets. Map regressions covered seams in both directions, exposed faces after dirty updates, sleeping across two one-way supports, passing through all supports, and landing again. Gameplay tests covered multi-support pass-through helpers.

Box2D Lab passed an SDL software-rendering smoke test and generated geometry previews were inspected. Actual window interaction and movement feel were not accepted in that record.

## Reproduce the Timing Probe

Configure Release with `ELYSIA_BUILD_PHYSICS_BENCHMARK=ON` and run `physics_benchmark`. Measure 100 and 500 separated moving rectangles, warm up 200 steps, then measure 1000 fixed steps at 60 Hz. Disable debug capture and sleeping; record median and P95 of the full Elysia advance call.

## Historical Benchmark Results

The pre-migration standalone probe used `/O2 /DNDEBUG /MD`:

| Bodies | Median µs | P95 µs |
| --- | ---: | ---: |
| 100 | 22.0 | 34.7 |
| 500 | 185.1 | 225.6 |

On the same host, the Box2D Release probe measured:

| Bodies | Median µs | P95 µs |
| --- | ---: | ---: |
| 100 | 29.0 | 42.2 |
| 500 | 190.9 | 205.9 |

These sequential measurements of the final adapter and archived native implementation both include event extraction and state synchronization. Median time increased at 100 bodies; at 500 it was similar and P95 lower. This does not establish general engine acceleration.

A separate new-capability probe measured 50 rotating rectangles and five distance springs:

| Sleeping enabled | Median µs | P95 µs |
| --- | ---: | ---: |
| No | 97.3 | 164.2 |
| Yes | 108.5 | 136.1 |

Enabling sleeping is a world setting, not proof that every body sleeps. No old-backend baseline exists for this case. This is a sparse-body microbenchmark, not a stack benchmark or whole-engine performance measurement. Workload and machine state affect results.
