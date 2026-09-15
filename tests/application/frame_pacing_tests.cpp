#include "engine/application/lifecycle/frame_pacing.h"
#include "tests/support/test_assertions.h"
#include <limits>
#include <vector>

int main()
{
    using elysia::tests::require;
    using elysia::application::detail::wait_for_frame;
    for (double fps : {60.0,144.0,240.0})
    {
        double elapsed = 0.002;
        bool used_precise = false;
        int calls = 0;
        wait_for_frame(fps,[&] { return elapsed; },[] { return false; },
            [&](std::uint64_t ns,bool precise)
            {
                require(ns > 0 && ns <= (precise ? 1'000'000u : 8'000'000u),"bounded sleeps");
                used_precise |= precise;
                elapsed += ns / 1e9;
                require(++calls < 30,"wait must converge");
            });
        require(used_precise && elapsed >= 1.0 / fps && elapsed < 1.0 / fps + 1e-8,
            "fractional millisecond budget must be preserved");
    }
    int sleeps = 0;
    wait_for_frame(60.0,[] { return 0.1; },[] { return false; },
        [&](auto,auto) { ++sleeps; });
    require(sleeps == 0,"over-budget present must not sleep");
    double elapsed = 0;
    wait_for_frame(60.0,[&] { return elapsed; },[] { return false; },
        [&](auto,auto) { ++sleeps; elapsed += 0.1; });
    require(sleeps == 1,"oversleep must be remeasured");
    elapsed = 0;
    int pumps = 0;
    wait_for_frame(60.0,[&] { return elapsed; },[&] { ++pumps; return false; },
        [&](auto,auto) { elapsed = 0.02; });
    require(pumps == 1,"do not pump events again after the deadline");
    sleeps = 0;
    elapsed = 0;
    wait_for_frame(60.0,[&] { return elapsed; },[&] { elapsed = 0.02; return false; },
        [&](auto,auto) { ++sleeps; });
    require(sleeps == 0,"event pumping time must count toward the budget");
    for (double fps : {0.01,std::numeric_limits<double>::denorm_min()})
    {
        sleeps = 0;
        wait_for_frame(fps,[] { return 0.0; },[&] { return sleeps == 3; },
            [&](std::uint64_t ns,bool precise)
            {
                require(ns <= 8'000'000 && !precise,"tiny fps must use bounded ordinary sleeps");
                ++sleeps;
            });
        require(sleeps == 3,"quit/fault must interrupt a very long frame budget");
    }
    for (double fps : {0.0,-1.0,std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()})
    {
        wait_for_frame(fps,[] { return 0.0; },[] { return false; },
            [&](auto,auto) { require(false,"invalid fps must not wait"); });
    }
}
