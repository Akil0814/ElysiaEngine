#pragma once

#include "game/showcase/gameplay/runtime/block_actor.h"
#include "game/showcase/gameplay/runtime/demo_obstacle.h"
#include "engine/gameplay/control/control_command.h"

namespace example::scene::detail
{
[[nodiscard]] elysia::physics::PhysicsWorldConfig make_gravity_config(
    float gravity) noexcept;

[[nodiscard]] example::showcase::gameplay::ObstacleConfig make_aabb_obstacle(
    const elysia::core::Rect& rect,
    elysia::core::Color color) noexcept;

class QueryProbe final : public elysia::core::GameObject
{
public:
    QueryProbe(
        elysia::physics::PhysicsWorld& world,
        example::showcase::gameplay::BlockCombatActor& player) noexcept;

    void execute();
    void target_removed(const elysia::core::SceneObject& object) { if (_player == &object) _player = nullptr; }

  private:
    elysia::physics::PhysicsWorld* _world = nullptr;
    example::showcase::gameplay::BlockCombatActor* _player = nullptr;
};
}
