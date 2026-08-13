#pragma once

#include "../../../demo/physics/block_actor.h"
#include "../../../demo/physics/demo_obstacle.h"
#include "../../../../engine/gameplay/input/contracts/gameplay_input_frame_receiver.h"

namespace example::scene::detail
{
[[nodiscard]] elysia::physics::PhysicsWorldConfig make_gravity_config(
    float gravity) noexcept;

[[nodiscard]] example::demo::physics::ObstacleConfig make_aabb_obstacle(
    const elysia::core::Rect& rect,
    elysia::core::Color color) noexcept;

class QueryProbe final
    : public elysia::core::GameObject
    , public elysia::gameplay::GameplayInputFrameReceiver
{
public:
    QueryProbe(
        elysia::physics::PhysicsWorld& world,
        example::demo::physics::BlockCombatActor& player) noexcept;

    void on_gameplay_input_frame(
        const elysia::gameplay::GameplayInputFrame& input) override;

private:
    elysia::physics::PhysicsWorld* _world = nullptr;
    example::demo::physics::BlockCombatActor* _player = nullptr;
};
}
