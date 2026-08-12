#pragma once

#include "../core/geometry/vector2.h"

#include <cstdint>

namespace elysia::physics
{
struct PhysicsWorldConfig
{
    double fixed_delta_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_advance = 8;
    std::uint32_t solver_iterations = 8;
    std::uint32_t max_ccd_iterations = 4;
    std::uint32_t max_tile_candidates_per_operation = 65536;
    elysia::core::Vector2 gravity{};
    float collision_epsilon = 1.0e-5f;
    float penetration_slop = 0.001f;
    float position_correction_percent = 0.8f;
    float contact_normal_threshold = 0.5f;
    float restitution_velocity_threshold = 1.0f;
};
}
