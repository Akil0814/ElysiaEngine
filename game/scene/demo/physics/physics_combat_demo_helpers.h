#pragma once

#include "../../../demo/physics/block_actor.h"
#include "../../../demo/physics/demo_obstacle.h"
#include "../../../../engine/gameplay/control/control_command.h"

namespace example::scene::detail
{
[[nodiscard]] elysia::physics::PhysicsWorldConfig make_gravity_config(
    float gravity) noexcept;

[[nodiscard]] example::demo::physics::ObstacleConfig make_aabb_obstacle(
    const elysia::core::Rect& rect,
    elysia::core::Color color) noexcept;

class QueryProbe final : public elysia::core::GameObject
{
public:
    QueryProbe(
        elysia::physics::PhysicsWorld& world,
        example::demo::physics::BlockCombatActor& player) noexcept;

    void execute();
    void target_removed(const elysia::core::SceneObject& object) { if (_player == &object) _player = nullptr; }

  private:
    elysia::physics::PhysicsWorld* _world = nullptr;
    example::demo::physics::BlockCombatActor* _player = nullptr;
};
}
