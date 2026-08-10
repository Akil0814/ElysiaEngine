#pragma once

#include "collider.h"

#include "../../core/geometry/vector2.h"

#include <array>
#include <cstdint>

namespace elysia::physics
{
struct CollisionPair
{
    ColliderId first = InvalidColliderId;
    ColliderId second = InvalidColliderId;
};

struct CollisionManifold
{
    // Detection strategies orient the normal from CollisionPair::first to second.
    elysia::core::Vector2 normal{};
    float penetration = 0.0f;
    std::array<elysia::core::Vector2, 2> contact_points{};
    std::uint8_t contact_point_count = 0;
};

struct CollisionHit
{
    CollisionManifold manifold{};
    float time_of_impact = 1.0f;
};

struct CollisionOverlap
{
    CollisionPair pair{};
    CollisionManifold manifold{};
};

struct CollisionContact
{
    CollisionPair pair{};
    CollisionManifold manifold{};
    CollisionResponse response = CollisionResponse::Ignore;
};
}
