#pragma once

#include "gameplay_collision_types.h"

#include "../../physics/collision/collision_contact.h"

namespace elysia::gameplay::collision
{
struct BodyContactEvent
{
    ColliderBinding body{};
    elysia::physics::ColliderId other = elysia::physics::InvalidColliderId;
    elysia::physics::CollisionContact contact{};
};

struct PushBoxOverlapEvent
{
    ColliderBinding first{};
    ColliderBinding second{};
    elysia::physics::CollisionOverlap overlap{};
};

struct HitOverlapEvent
{
    HitBoxBinding hit_box{};
    ColliderBinding hurt_box{};
    elysia::physics::CollisionOverlap overlap{};
};
}
