# Validation and performance
The migration replaces old algorithm-specific tests with public behavior tests:
- unit conversions, scale invariance, density, explicit mass and inertia;
- fixed-step callbacks, creation/removal/teleport ordering and silent reset;
- Sensor End lifecycle and Gameplay routing after unbinding;
- rotated queries, circle CCD, sleep/wake, one-way and dirty Tile updates;
- distance springs, revolute motor direction/limits and joint invalidation;
- actual Scene registration, input latching, damage and render interpolation.

The Debug build passes all 84 CTest targets. Tile regressions cover walking both ways across seams, exposed faces after dirty updates, sleeping across two one-way supports, dropping through every support, and landing again. The Gameplay test also exercises the multi-support drop helper.

The Box2D lab passes an SDL software-renderer smoke test and its generated geometry preview has been inspected. Interactive window testing and movement feel remain unverified.

## Timing probe
Configure Release with `ELYSIA_BUILD_PHYSICS_BENCHMARK=ON` and run `physics_benchmark`. It measures 100 and 500 separated moving boxes, 200 warm-up steps and 1000 measured 60 Hz steps. Debug capture and sleep are disabled. Report median and P95 of the entire Elysia advance call.

The pre-migration standalone probe used `/O2 /DNDEBUG /MD` and measured:
| Bodies | Median µs | P95 µs |
|---|---:|---:|
| 100 | 22.0 | 34.7 |
| 500 | 185.1 | 225.6 |

The Box2D-backed Release probe on the same host measured:
| Bodies | Median µs | P95 µs |
|---|---:|---:|
| 100 | 29.0 | 42.2 |
| 500 | 190.9 | 205.9 |

These are sequential measurements of the final adapter and the archived native implementation on the same host. The 100-body median is higher; the 500-body median is similar and its P95 is lower. Both include Elysia event extraction and state synchronization. This run does not demonstrate a general speedup.

The new-capability probe measures 50 rotating boxes and five distance springs separately:
| Sleep enabled | Median µs | P95 µs |
|---|---:|---:|
| No | 97.3 | 164.2 |
| Yes | 108.5 | 136.1 |

Sleep enabled is a world setting, not an assertion that every body is asleep during measurement. These scenes have no equivalent native-backend baseline.

This is a sparse-body microbenchmark, not a stacking benchmark or a claim of general engine performance. Runtime load and machine conditions affect results.
