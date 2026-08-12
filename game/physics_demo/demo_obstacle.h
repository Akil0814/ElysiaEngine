#pragma once

#include "demo_collision_layers.h"
#include "colored_block_object.h"

#include "../../engine/core/interface/updatable.h"
#include "../../engine/physics/contracts/collider_provider.h"
#include "../../engine/physics/contracts/physics_body_provider.h"

namespace example::physics_demo
{
struct ObstacleConfig
{
    elysia::core::Rect rect{};
    elysia::core::Color color{};
    elysia::physics::ColliderShape shape{elysia::physics::AabbShape{}};
    elysia::physics::CollisionResponse response = elysia::physics::CollisionResponse::Block;
    elysia::physics::CollisionDetectionMode detection = elysia::physics::CollisionDetectionMode::Discrete;
    std::optional<elysia::physics::OneWayCollision> one_way;
    elysia::physics::PhysicsMaterial material{};
};

class StaticBlockObstacle final
    : public ColoredBlockObject
    , public elysia::physics::ColliderProvider
{
public:
    explicit StaticBlockObstacle(ObstacleConfig config);
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;
    [[nodiscard]] std::span<elysia::physics::Collider> colliders() noexcept override { return {&_collider, 1}; }
    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept override { return {&_collider, 1}; }
private:
    elysia::physics::Collider _collider;
};

class DynamicBlockObstacle final
    : public ColoredBlockObject
    , public elysia::core::Updatable
    , public elysia::physics::ColliderProvider
    , public elysia::physics::PhysicsBodyProvider
{
public:
    explicit DynamicBlockObstacle(ObstacleConfig config,
        elysia::core::Vector2 velocity = {});
    void update(double delta) override { update_visual(delta); }
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::physics::PhysicsBody* physics_body() noexcept override { return &_body; }
    [[nodiscard]] const elysia::physics::PhysicsBody* physics_body() const noexcept override { return &_body; }
    [[nodiscard]] std::span<elysia::physics::Collider> colliders() noexcept override { return {&_collider, 1}; }
    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept override { return {&_collider, 1}; }
private:
    elysia::physics::PhysicsBody _body;
    elysia::physics::Collider _collider;
};

class KinematicMovingPlatform final
    : public ColoredBlockObject
    , public elysia::core::Updatable
    , public elysia::physics::ColliderProvider
    , public elysia::physics::PhysicsBodyProvider
{
public:
    KinematicMovingPlatform(
        ObstacleConfig config,
        float left,
        float right,
        float speed);
    void update(double delta) override;
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;
    [[nodiscard]] elysia::physics::PhysicsBody* physics_body() noexcept override { return &_body; }
    [[nodiscard]] const elysia::physics::PhysicsBody* physics_body() const noexcept override { return &_body; }
    [[nodiscard]] std::span<elysia::physics::Collider> colliders() noexcept override { return {&_collider, 1}; }
    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept override { return {&_collider, 1}; }
private:
    elysia::physics::PhysicsBody _body;
    elysia::physics::Collider _collider;
    float _left = 0.0f;
    float _right = 0.0f;
    float _speed = 0.0f;
};
}
