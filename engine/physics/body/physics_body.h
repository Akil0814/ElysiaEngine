#pragma once

#include "../../core/geometry/vector2.h"

#include <cstdint>

namespace elysia::physics
{
enum class BodyType : std::uint8_t
{
    Static,
    Kinematic,
    Dynamic
};

struct PhysicsBody
{
    elysia::core::Vector2 velocity{};
    elysia::core::Vector2 accumulated_force{};
    elysia::core::Vector2 max_speed{};

    float gravity_scale = 1.0f;
    float linear_damping = 0.0f;
    float mass = 1.0f;

    bool enabled = true;
    BodyType type = BodyType::Dynamic;
};
}
