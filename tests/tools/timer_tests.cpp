#include "engine/tools/timer.h"
#include "tests/support/test_assertions.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace
{
using elysia::tests::require;
using elysia::tools::Timer;

static_assert(noexcept(std::declval<const Timer&>().get_wait_time()));
static_assert(noexcept(std::declval<const Timer&>().get_elapsed_time()));
static_assert(noexcept(std::declval<const Timer&>().get_remaining_time()));
static_assert(noexcept(std::declval<const Timer&>().is_one_shot()));
static_assert(noexcept(std::declval<const Timer&>().is_finished()));
static_assert(noexcept(std::declval<const Timer&>().is_paused()));

void require_time(double actual, double expected, const char* message)
{
    require(std::abs(actual - expected) < 1e-9, message);
}

void test_defaults_and_progress()
{
    Timer timer;
    const Timer& state = timer;
    require_time(state.get_wait_time(), 0.0, "default wait time must be zero");
    require_time(state.get_elapsed_time(), 0.0, "default elapsed time must be zero");
    require_time(state.get_remaining_time(), 0.0, "default remaining time must be zero");
    require(!state.is_one_shot() && !state.is_finished() && !state.is_paused(),
        "default timer must be an unfinished, unpaused looping timer");

    int callbacks = 0;
    timer.set_on_timeout([&] { ++callbacks; });
    timer.set_wait_time(2.0);
    timer.update(0.75);
    require_time(state.get_wait_time(), 2.0, "query must return configured wait time");
    require_time(state.get_elapsed_time(), 0.75, "query must return accumulated time");
    require_time(state.get_remaining_time(), 1.25, "remaining time must subtract progress");
    timer.update(0.0);
    timer.update(-1.0);
    require_time(state.get_elapsed_time(), 0.75, "nonpositive delta must not advance progress");
    timer.update(1.25);
    require(callbacks == 1, "queries must not disturb timeout delivery");
    require_time(state.get_elapsed_time(), 0.0, "looping timer must consume full intervals");
    require_time(state.get_remaining_time(), 2.0, "next loop must start with full remaining time");
    require(!state.is_finished(), "looping timer must never report completion");
}

void test_disabled_intervals_and_interval_changes()
{
    Timer timer;
    timer.set_wait_time(2.0);
    timer.update(0.75);
    timer.set_wait_time(-1.0);
    require_time(timer.get_wait_time(), 0.0, "negative wait time must normalize to zero");
    require_time(timer.get_remaining_time(), 0.0, "disabled timer must have zero remaining time");
    timer.update(10.0);
    require_time(timer.get_elapsed_time(), 0.75, "disabled timer must preserve accumulated time");
    require(!timer.is_finished(), "zero remaining time must not imply completion");
    timer.set_wait_time(0.0);
    timer.update(10.0);
    require_time(timer.get_elapsed_time(), 0.75, "zero wait time must not advance progress");
    timer.set_wait_time(0.5);
    require_time(timer.get_elapsed_time(), 0.75, "changing wait time must not restart progress");
    require_time(timer.get_remaining_time(), 0.0, "overdue remaining time must clamp to zero");
    timer.set_one_shot(true);
    require(timer.is_one_shot() && !timer.is_finished(), "selecting one-shot must not mark completion");
    timer.update(0.0);
    require(!timer.is_finished(), "querying an overdue timer must not trigger it");
}

void test_pause_resume_and_restart()
{
    Timer timer;
    timer.set_wait_time(2.0);
    timer.set_one_shot(true);
    timer.update(0.75);
    timer.pause();
    timer.update(10.0);
    require(timer.is_paused() && !timer.is_finished(), "manual pause must not imply completion");
    require_time(timer.get_elapsed_time(), 0.75, "pause must preserve elapsed time");
    require_time(timer.get_remaining_time(), 1.25, "pause must preserve remaining time");
    timer.resume();
    timer.update(0.25);
    require(!timer.is_paused(), "resume must clear pause");
    require_time(timer.get_elapsed_time(), 1.0, "resume must continue existing progress");
    timer.pause();
    timer.restart();
    require(!timer.is_paused() && !timer.is_finished() && timer.is_one_shot(),
        "restart must clear pause and completion while preserving mode");
    require_time(timer.get_elapsed_time(), 0.0, "restart must clear progress");
    require_time(timer.get_remaining_time(), 2.0, "restart must restore the full interval");
}

void test_one_shot_completion()
{
    Timer timer;
    timer.set_wait_time(2.0);
    timer.set_one_shot(true);
    int callbacks = 0;
    timer.set_on_timeout([&]
    {
        ++callbacks;
        require(timer.is_finished() && timer.is_paused(), "completion must be visible inside callback");
        require_time(timer.get_elapsed_time(), 2.0, "completed elapsed time must exclude overshoot");
        require_time(timer.get_remaining_time(), 0.0, "completed callback must see zero remaining time");
    });
    timer.update(1.5);
    require(!timer.is_finished(), "one-shot must be unfinished before timeout");
    require_time(timer.get_remaining_time(), 0.5, "one-shot must expose remaining countdown");
    timer.update(5.0);
    require(callbacks == 1 && timer.is_finished(), "one-shot must complete once even with overshoot");
    timer.update(5.0);
    timer.resume();
    require(timer.is_finished() && !timer.is_paused(), "resume must preserve completion");
    timer.update(0.5);
    require_time(timer.get_elapsed_time(), 2.0, "finished reads must remain complete after resume");
    timer.update(2.0);
    require(callbacks == 1 && timer.is_paused(), "resuming a completed timer must not fire again");
    timer.set_wait_time(3.0);
    require_time(timer.get_elapsed_time(), 3.0, "completed elapsed time must follow current wait time");
    require_time(timer.get_remaining_time(), 0.0, "changing completed interval must not restart it");
    timer.set_wait_time(0.0);
    require(timer.is_finished(), "disabling a completed timer must preserve completion");
    require_time(timer.get_elapsed_time(), 0.0, "completed disabled timer must use current zero interval");
    timer.set_wait_time(2.0);
    timer.restart();
    require(!timer.is_finished() && !timer.is_paused(), "restart must begin another one-shot");
    require_time(timer.get_elapsed_time(), 0.0, "restart after completion must clear elapsed time");
    require_time(timer.get_remaining_time(), 2.0, "restart after completion must restore countdown");
    timer.update(2.0);
    require(callbacks == 2, "restart must allow another timeout");

    timer.set_one_shot(false);
    require(!timer.is_one_shot() && !timer.is_finished(), "looping mode must hide one-shot completion");
    require_time(timer.get_elapsed_time(), 0.0, "looping mode must expose stored accumulation");
    require_time(timer.get_remaining_time(), 2.0, "looping mode must expose next interval");
    timer.set_one_shot(true);
    require(timer.is_finished(), "switching modes must not clear the stored shot flag");
}

void test_looping_catch_up()
{
    Timer timer;
    timer.set_wait_time(1.0);
    int callbacks = 0;
    timer.set_on_timeout([&]
    {
        ++callbacks;
        require(!timer.is_finished(), "loop callbacks must remain unfinished");
    });
    timer.update(3.25);
    require(callbacks == 3, "state queries must preserve all loop catch-up callbacks");
    require_time(timer.get_elapsed_time(), 0.25, "loop must retain fractional accumulation");
    require_time(timer.get_remaining_time(), 0.75, "loop must report time to next timeout");
}

void test_callback_mutations()
{
    Timer paused;
    paused.set_wait_time(1.0);
    int callbacks = 0;
    paused.set_on_timeout([&]
    {
        ++callbacks;
        paused.pause();
        require_time(paused.get_elapsed_time(), 2.25, "callback pause must preserve pending accumulation");
        require_time(paused.get_remaining_time(), 0.0, "pending accumulation must clamp remaining time");
    });
    paused.update(3.25);
    require(callbacks == 1 && paused.is_paused() && !paused.is_finished(),
        "callback pause must interrupt catch-up without marking completion");
    require_time(paused.get_elapsed_time(), 2.25, "queries must not consume pending intervals");

    Timer restarted;
    restarted.set_wait_time(1.0);
    restarted.set_one_shot(true);
    callbacks = 0;
    restarted.set_on_timeout([&]
    {
        ++callbacks;
        require(restarted.is_finished(), "one-shot callback must initially see completion");
        restarted.restart();
        require(!restarted.is_finished() && !restarted.is_paused(), "callback restart must clear completion");
        require_time(restarted.get_elapsed_time(), 0.0, "callback restart must reset elapsed time");
        require_time(restarted.get_remaining_time(), 1.0, "callback restart must restore interval");
    });
    restarted.update(3.25);
    require(callbacks == 1, "callback restart must interrupt current update");
    restarted.update(0.25);
    require_time(restarted.get_elapsed_time(), 0.25, "restarted timer must advance from zero");

    Timer changed;
    changed.set_wait_time(1.0);
    callbacks = 0;
    changed.set_on_timeout([&]
    {
        ++callbacks;
        changed.set_wait_time(0.5);
        require_time(changed.get_elapsed_time(), 2.25, "callback interval change must preserve accumulation");
        require_time(changed.get_remaining_time(), 0.0, "overdue changed interval must report zero remaining");
    });
    changed.update(3.25);
    require(callbacks == 1, "callback interval change must interrupt catch-up");
    require_time(changed.get_wait_time(), 0.5, "changed interval must be immediately visible");
}
}

int main()
{
    test_defaults_and_progress();
    test_disabled_intervals_and_interval_changes();
    test_pause_resume_and_restart();
    test_one_shot_completion();
    test_looping_catch_up();
    test_callback_mutations();
    std::cout << "Timer tests passed\n";
    return EXIT_SUCCESS;
}
