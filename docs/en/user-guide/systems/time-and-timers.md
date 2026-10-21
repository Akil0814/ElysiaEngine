# Time and Timers

This document separates application time, scene fixed steps, and manual timers. Choose a clock, then its update phase, keeping slow motion, pause, and stopped timers distinct.

Time interfaces use seconds. `ELYSIA_TIME` supplies application time; `Timer` is caller-driven. Their coexistence does not connect them automatically.

## Choosing a Time Source

| Interface | Meaning |
| --- | --- |
| `raw_delta()` | Frame `delta` before global scaling |
| `delta()` | Raw frame `delta` multiplied by global `time_scale()` |
| `total_time()` | Accumulated raw time |
| `scaled_total_time()` | Accumulated scaled time |
| `frame_count()` | Frames started, not physics steps |

The application calls `begin_frame()` each frame; games should not call it again or arbitrarily `reset()` global time. Normal scene updates receive scaled `delta`. Features unaffected by slow motion can choose `raw_delta()`, but raw time cannot restore scheduling if pause already skipped that update.

```cpp
#include "engine/core/time.h"

// The game flow calls these at effect start and end.
void begin_slow_motion()
{
    ELYSIA_TIME->set_time_scale(0.5);
}
void end_slow_motion()
{
    ELYSIA_TIME->set_time_scale(1.0);
}
```

This assumes normal scale 1 and one slow-motion owner. Games must coordinate concurrent effects rather than letting any ending effect overwrite the rest.

Scale participates in `begin_frame()`, so mid-frame changes apply next frame. Zero scale stops scaled time while `raw_delta()`, `total_time()`, and frame counts continue. Input, rendering, and zero-`delta` updates may still run; zero `delta` is not an explicit input-disable policy.

## Physics, Pause, and Object Scale

Fixed-step scenes accumulate scaled `delta` in independent `FixedStepRuntime`. Actual steps call scene hooks before optional `PhysicsWorld`. Global slow motion reduces steps per real second without changing fixed step size; frames can have zero/multiple steps. Do not multiply global scale again in fixed callbacks. Advancement has a maximum-step limit and drops excessive backlog; huge scales cannot promise infinitely accurate fast-forward.

`Scene::pause()` stops scene physics and filters ordinary updates/input by object policies, without stopping the global clock. `GameObject::set_time_scale()` is only object-level scaling; ordinary scheduling does not apply it automatically. Use `scaled_delta(delta)` explicitly; it does not change body simulation speed automatically.

`on_before_update()` / `on_after_update()` still run during pause. Timers in those hooks need an `is_paused()` check or explicit timer `pause()` to stop. Member timers advance only when their owners receive updates.

## Manually Driving Timer

This class can be an object member; call `update(delta)` from its owner's update. It does not register automatically. Member timers die with their owners, avoiding independent long-lived callbacks accessing expired objects.

```cpp
#include "engine/tools/timer.h"

class Cooldown
{
public:
    Cooldown()
    {
        _timer.set_wait_time(2.0);
        _timer.set_one_shot(true);
        // The callback borrows this; the timer is a member and its capturing owner cannot be copied.
        _timer.set_on_timeout([this]
        {
            _ready = true;
        });
        _timer.pause(); // start() explicitly begins the first round.
    }

    Cooldown(const Cooldown&) = delete;
    Cooldown& operator=(const Cooldown&) = delete;

    // Restart the full cooldown after each skill use.
    void start()
    {
        _ready = false;
        _timer.restart();
    }

    void update(double delta)
    {
        // Callers choose time; Timer does not subscribe to global updates.
        _timer.update(delta);
    }

    bool ready() const
    {
        return _ready;
    }

    double remaining_seconds() const
    {
        return _timer.get_remaining_time();
    }

private:
    bool _ready = false;
    elysia::tools::Timer _timer;
};
```

`set_wait_time()` changes the interval without restarting. `restart()` clears elapsed time/trigger flags and resumes. `pause()` preserves progress; `resume()` continues. One-shot timers pause on firing; restart for a new round.

`get_wait_time()` returns configured interval, `get_elapsed_time()` accumulated time, and `get_remaining_time()` remaining time, all seconds. After one-shot completion, elapsed equals the currently configured interval and remaining is `0`; later interval changes also change reported elapsed. A zero interval reports remaining `0` without becoming finished.

`is_one_shot()` queries mode; `is_finished()` is `true` only after firing in one-shot mode. Manual pause does not mark completion; `resume()` after completion does not clear it, while `restart()` does. Repeating mode never reports finished. Interval changes or interruption of catch-up callbacks can leave unfinished elapsed time above the interval, with remaining still `0`.

Repeating timers (`set_one_shot(false)`) can fire multiple times for a large `delta`. Non-positive wait or `delta` does not advance. Restarting, pausing, or changing wait time inside callbacks stops further catch-up in that update. Never directly destroy the currently executing `Timer` owner inside its callback.

## References

- [Time](../../../../engine/core/time.h), [Timer](../../../../engine/tools/timer.h)
- [Core update concepts](../core_concepts.md), [back to user guide](../README.md)
