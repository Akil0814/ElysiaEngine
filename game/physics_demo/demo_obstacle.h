#pragma once

#include "demo_collision_layers.h"
#include "solid_color_render.h"

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
};

class StaticBlockObstacle final
    : public ColoredBlockObject
    , public elysia::physics::ColliderProvider
{
public:
    StaticBlockObstacle(SolidColorTexture& texture, ObstacleConfig config);
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
    DynamicBlockObstacle(SolidColorTexture& texture, ObstacleConfig config,
        elysia::core::Vector2 velocity = {});
    void update(double delta) override { update_visual(delta); }
    [[nodiscard]] elysia::physics::PhysicsBody* physics_body() noexcept override { return &_body; }
    [[nodiscard]] const elysia::physics::PhysicsBody* physics_body() const noexcept override { return &_body; }
    [[nodiscard]] std::span<elysia::physics::Collider> colliders() noexcept override { return {&_collider, 1}; }
    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept override { return {&_collider, 1}; }
private:
    elysia::physics::PhysicsBody _body;
    elysia::physics::Collider _collider;
};
}
