#pragma once

#include "../../core/geometry/vector2.h"

namespace elysia::physics
{
struct PhysicsBody
{
    elysia::core::Vector2 velocity{};
    elysia::core::Vector2 accumulated_force{};
    elysia::core::Vector2 max_speed{};

    float gravity_scale = 1.0f;
    float linear_damping = 0.0f;
    float mass = 1.0f;

    bool enabled = true;
    bool is_static = false;
    bool is_kinematic = false;
};
}
