# Validation and performance
The migration replaces old algorithm-specific tests with public behavior tests:
- unit conversions, scale invariance, density, explicit mass and inertia;
- fixed-step callbacks, creation/removal/teleport ordering and silent reset;
- Sensor End lifecycle and Gameplay routing after unbinding;
- rotated queries, circle CCD, sleep/wake, one-way and dirty Tile updates;
- distance springs, revolute motor direction/limits and joint invalidation;
- actual Scene registration, input latching, damage and render interpolation.

Run CTest after building all targets. A passing suite is not a substitute for inspecting the interactive demos.

## Timing probe
Configure Release with `ELYSIA_BUILD_PHYSICS_BENCHMARK=ON` and run `physics_benchmark`. It measures 100 and 500 separated moving boxes, 200 warm-up steps and 1000 measured 60 Hz steps. Debug capture and sleep are disabled. Report median and P95 of the entire Elysia advance call.

The pre-migration optimized standalone probe measured:
| Bodies | Median µs | P95 µs |
|---|---:|---:|
| 100 | 21.3 | 23.9 |
| 500 | 196.0 | 291.0 |

The Box2D-backed Release probe on the same host measured:
| Bodies | Median µs | P95 µs |
|---|---:|---:|
| 100 | 20.9 | 23.4 |
| 500 | 136.8 | 152.5 |

The 100-body results are similar; the 500-body median was about 30% lower in this run. Both include Elysia's event extraction and state synchronization. These measurements do not establish performance for dense contact scenes.

This is a sparse-body microbenchmark, not a stacking benchmark or a claim of general engine performance. Runtime load and machine conditions affect results.
