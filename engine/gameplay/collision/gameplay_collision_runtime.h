#pragma once

#include "actor_collision_rig.h"
#include "gameplay_collision_types.h"

namespace elysia::gameplay::collision
{
class IGameplayCollisionRuntime
{
public:
    virtual ~IGameplayCollisionRuntime() = default;

    [[nodiscard]] virtual bool bind_actor(const ActorCollisionRig& rig) = 0;
    [[nodiscard]] virtual bool bind_collider(const ColliderBinding& binding) = 0;
    [[nodiscard]] virtual bool bind_hit_box(const HitBoxBinding& binding) = 0;
    [[nodiscard]] virtual bool unbind_collider(elysia::physics::ColliderId collider) = 0;
    [[nodiscard]] virtual bool request_drop_through(const DropThroughRequest& request) = 0;
};
}
