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
    std::uint32_t max_ccd_iterations = 4;
    elysia::core::Vector2 gravity{};
    float collision_epsilon = 1.0e-5f;
    float penetration_slop = 0.001f;
    float position_correction_percent = 1.0f;
    float contact_normal_threshold = 0.5f;
};
}
