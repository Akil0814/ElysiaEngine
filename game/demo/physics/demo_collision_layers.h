#pragma once

#include "../../../engine/physics/collision/collider.h"

namespace example::demo::physics::collision_layers
{
inline constexpr elysia::physics::CollisionBits World = 1u << 0;
inline constexpr elysia::physics::CollisionBits Body = 1u << 1;
inline constexpr elysia::physics::CollisionBits HurtBox = 1u << 2;
inline constexpr elysia::physics::CollisionBits HitBox = 1u << 3;
inline constexpr elysia::physics::CollisionBits Trigger = 1u << 4;
}
