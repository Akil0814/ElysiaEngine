#pragma once

#include "collider.h"

namespace elysia::physics
{
struct RayCastQuery
{
    elysia::core::Vector2 origin{};
    elysia::core::Vector2 direction{};
    float max_distance = 0.0f;
    CollisionFilter filter{};
};

struct SegmentCastQuery
{
    elysia::core::Vector2 start{};
    elysia::core::Vector2 end{};
    CollisionFilter filter{};
};

struct CollisionQueryHit
{
    ColliderId collider = InvalidColliderId;
    elysia::core::Vector2 point{};
    elysia::core::Vector2 normal{};
    float distance = 0.0f;
    float fraction = 0.0f;
};

}
