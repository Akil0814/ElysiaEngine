# Common Utilities

This document introduces vectors, rectangles, `colors`, random numbers, logging, and JSON reads. Examples are local call fragments; callers remain responsible for validation, lifetime, and failure handling.

These values/utilities do not register themselves with scenes. See [time and timers](../systems/time-and-timers.md) for timer advancement, callbacks, and pause.

## Vector2: Direction and Distance

Use this function in ordinary game logic; speed is world units/second and delta is seconds.

```cpp
#include "engine/core/geometry/vector2.h"

elysia::core::Vector2 move_toward(elysia::core::Vector2 from,
    elysia::core::Vector2 to, float speed, double delta)
{
    const elysia::core::Vector2 offset = to - from;
    const float distance = offset.length();
    if (speed <= 0.0f || delta <= 0.0)
        return from;
    const float step = speed * static_cast<float>(delta);
    // Snap when less than one step remains to avoid oscillation from overshooting.
    if (distance <= step)
        return to;
    return from + offset.normalized() * step;
}
```

`Vector2` stores float x/y and provides length, length_squared, distance_to, distance_squared_to, direction_to, dot, and cross. Vector multiplication is a dot product, not component-wise multiplication. Both `normalized()` and `normalize()` return new vectors; `normalize_in_place()` mutates, zeroing near-zero vectors and returning false. Use `nearly_equals()` for approximate comparison, not exact float equality to detect motion completion. Callers ensure finite inputs.

## Rect: Bounds and Intersection

```cpp
#include "engine/core/geometry/rect.h"

// Place in a region-test function; arguments must share a coordinate space.
bool within_area(elysia::core::Vector2 point)
{
    const elysia::core::Rect area{0, 0, 100, 80};
    // Exclude right/bottom so adjacent cells share boundaries without duplicate hits.
    return area.contains_half_open(point);
}
```

`Rect` construction order is x, y, width, height. Negative sizes clamp to zero rather than reversing the rectangle. Use `from_points` for endpoints and `from_center` for center/size. Getters expose position, size, center, and edges.

`contains(point)` includes boundaries; `contains_half_open(point)` excludes right/bottom. For normal nonempty rectangles, `intersects` requires positive overlapping area; `touches_or_intersects` includes edge contact. Check `is_empty()` to exclude degeneracy. `intersection` returns overlap, `merged` enclosing bounds, and `translated` / `expanded` new rectangles. These are geometry only, without physics filters, events, or body response.

## Color and Predefined Colors

```cpp
#include "engine/core/render/color.h"
#include "engine/core/render/colors.h"

// Use in a drawing-color configuration function.
constexpr elysia::core::Color kTranslucentRed{255, 0, 0, 128};
constexpr elysia::core::Color kForeground = elysia::core::colors::white;
```

`Color` has 0–255 RGBA channels, defaulting to opaque white; omitted alpha is 255. `colors` supplies white, black, transparent, and other constants, not theme management. Do not directly use 0–1 floats as these integer channels.

## RandomGenerator

Random generators are ordinary caller-owned objects. Keep one in the responsible game flow/object rather than reconstructing a fixed-seed generator on each sample.

```cpp
#include "engine/tools/random_generator.h"

#include <array>

// Create and retain rng during level initialization; fixed seeds help reproduce issues.
elysia::tools::RandomGenerator rng{12345};
int count = rng.int_inclusive<int>(1, 3);
double offset = rng.real(-10.0, 10.0);
bool rare = rng.chance(0.1);
std::array<int, 3> rewards{10, 20, 30};
int reward = rng.pick(rewards); // Copy the sampled value.
```

| Method | Bounds and errors |
| --- | --- |
| `int_inclusive(min, max)` | Both inclusive; throws `std::invalid_argument` if min exceeds max |
| `real(min, max)` | Inclusive lower/exclusive upper; finite endpoints with min less than max |
| `chance(p)` | Finite [0,1]; 0 always false, 1 always true; out-of-range throws |
| `pick(values)` | Sample a nonempty sized random-access range; empty ranges throw |

`pick()` returns an element reference; the example copies it. Retained references depend on container lifetime/reallocation. Validate player/configuration ranges and probabilities rather than using exceptions as ordinary per-frame flow.

Default construction uses an entropy seed. Explicit seeds help reproduce the same call flow, without guaranteeing network determinism. `set_seed()` resets the engine and clears fixed-value injection. Test `set_fixed_values()` injects raw random integers, not final high-level results. After exhaustion the engine resumes; rejection sampling can consume multiple values.

## Logging

```cpp
#include "engine/tools/logger.h"

// Place in a function with business context:
ELYSIA_LOG_INFO("gameplay", "Spawned enemies: " << 3);
ELYSIA_LOG_WARN("inventory", "Reward skipped because inventory is full.");
```

The first argument is a category; the second supports stream expressions, not printf placeholders. Use `ELYSIA_LOG_DEBUG`, `ELYSIA_LOG_INFO`, `ELYSIA_LOG_WARN`, or `ELYSIA_LOG_ERROR`; `ELYSIA_LOG` is Info convenience. Keep categories stable, include identifying objects/operations/reasons, and avoid unconditional high-volume per-frame logs.

`ELYSIA_LOG_TERMINATING` records Terminating level without requesting exit. Applications own logging lifetime; ordinary objects should not repeatedly initialize/shut down `Logger`.

Logging Error does not recover, switch scenes, or terminate automatically. Check service results and decide whether to stop, notify, or enter [application failure flow](../builtin-scenes/application-failure.md). Logging does not replace handling errors from interfaces such as `std::expected`.

## JSON Files and Field Validation

Prefer existing [loading](../content/resources.md) and [configuration](../content/configuration.md) services for resources/game settings. Use `JsonLoader` directly only for game-managed JSON files.

`open_file(path)` returns `expected<void, JsonFileFailure>`. On success `root()` exposes read-only JSON, `is_loaded()` queries state, and `reset()` clears it. Root references remain valid only while the loader retains that document; do not use old references after reload/reset.

Strict parsing checks syntax and duplicate properties; callers still validate required fields, types, and game rules. `JsonLoader` neither supplies business defaults nor converts arbitrary JSON to game objects. For new domain loaders, consult structured failure interfaces in [content registry loader](../../../../engine/io/loaders/content_registry_loader.h).

## References

- [Random API](../../../../engine/tools/random_generator.h), [logging API](../../../../engine/tools/logger.h)
- [JSON loading API](../../../../engine/io/json/json_loader.h)
- [Back to user guide](../README.md)
