#pragma once

#include "../core/geometry/vector2.h"

#include <cstdint>

namespace elysia::physics
{
struct PhysicsWorldConfig
{
    double fixed_delta_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_advance = 8;
    std::uint32_t solver_iterations = 4;
    elysia::core::Vector2 gravity{};
};
}
