#include "engine/tools/random_generator.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdint>
#include <exception>
#include <vector>

namespace
{
using elysia::tools::RandomGenerator;
using elysia::tests::require;

template <typename Callback>
void require_invalid_argument(Callback&& callback,const char* message)
{
    try
    {
        callback();
    }
    catch (const std::invalid_argument&)
    {
        return;
    }
    catch (...)
    {
    }

    require(false,message);
}

void require_same_seed_sequence()
{
    RandomGenerator first(RandomGenerator::TestSeedOne);
    RandomGenerator second(RandomGenerator::TestSeedOne);
    const std::array<int,4> choices{ 10,20,30,40 };

    for (int index = 0;index < 8;++index)
    {
        require(
            first.int_inclusive(-1000,1000) == second.int_inclusive(-1000,1000),
            "same seed must reproduce integer values"
        );
        require(first.real(-2.0,3.0) == second.real(-2.0,3.0), "same seed must reproduce real values");
        require(first.chance(0.35) == second.chance(0.35), "same seed must reproduce chance values");
        require(first.pick(choices) == second.pick(choices), "same seed must reproduce container picks");
    }
}

void require_fixed_values_take_priority()
{
    RandomGenerator generator(RandomGenerator::TestSeedTwo);
    constexpr std::array<std::uint64_t,3> fixed_values{ 3,1,2 };
    generator.set_fixed_values(fixed_values);

    require(generator.int_inclusive(10,12) == 10, "first fixed value must produce first bounded result");
    require(generator.int_inclusive(10,12) == 11, "second fixed value must produce second bounded result");
    require(generator.int_inclusive(10,12) == 12, "third fixed value must produce third bounded result");

    RandomGenerator expected_after_fallback(RandomGenerator::TestSeedTwo);
    require(
        generator.int_inclusive(0,1000000) == expected_after_fallback.int_inclusive(0,1000000),
        "fixed queue exhaustion must fall back to the unchanged engine state"
    );

    generator.set_fixed_values(std::array<std::uint64_t,1>{ 0 });
    generator.set_seed(RandomGenerator::TestSeedThree);
    RandomGenerator expected_after_seed(RandomGenerator::TestSeedThree);
    require(
        generator.int_inclusive(0,1000000) == expected_after_seed.int_inclusive(0,1000000),
        "setting a seed must clear fixed values"
    );
}

void require_ranges_and_boundaries()
{
    RandomGenerator generator(RandomGenerator::TestSeedThree);

    for (int index = 0;index < 100;++index)
    {
        const int integer = generator.int_inclusive(-4,7);
        require(integer >= -4 && integer <= 7, "integer result must stay in its inclusive range");

        const double real = generator.real(-2.5,4.5);
        require(real >= -2.5 && real < 4.5, "real result must stay in its half-open range");
    }

    require(!generator.chance(0.0), "zero chance must be false");
    require(generator.chance(1.0), "one chance must be true");

    const std::array<int,1> one_value{ 42 };
    require(generator.pick(one_value) == 42, "picking a single element must return that element");
}

void require_invalid_inputs_fail()
{
    RandomGenerator generator(RandomGenerator::TestSeedOne);
    std::vector<int> empty;

    require_invalid_argument([&] { static_cast<void>(generator.int_inclusive(2,1)); }, "reversed integer range must fail");
    require_invalid_argument([&] { static_cast<void>(generator.real(1.0,1.0)); }, "empty real range must fail");
    require_invalid_argument([&] { static_cast<void>(generator.real(2.0,1.0)); }, "reversed real range must fail");
    require_invalid_argument([&] { static_cast<void>(generator.chance(-0.1)); }, "negative probability must fail");
    require_invalid_argument([&] { static_cast<void>(generator.chance(1.1)); }, "probability above one must fail");
    require_invalid_argument([&] { static_cast<void>(generator.pick(empty)); }, "picking an empty container must fail");
}
}

int main()
{
    require_same_seed_sequence();
    require_fixed_values_take_priority();
    require_ranges_and_boundaries();
    require_invalid_inputs_fail();
    return 0;
}
