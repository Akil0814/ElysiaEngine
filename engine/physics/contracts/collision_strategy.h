#pragma once

#include "../collision/collider.h"
#include "../collision/collision_contact.h"
#include "../collision/world_shape.h"
#include "../physics_object_handle.h"

#include <optional>

namespace elysia::physics
{
struct CollisionShapeView
{
    CollisionTarget target{};
    PhysicsObjectHandle object{};
    WorldColliderShape previous_shape{WorldAabb{}};
    WorldColliderShape current_shape{WorldAabb{}};
    elysia::core::Rect current_bounds{};
    elysia::core::Rect swept_bounds{};
    elysia::core::Vector2 previous_owner_origin{};
    elysia::core::Vector2 current_owner_origin{};
    CollisionFilter filter{};
    CollisionResponse response = CollisionResponse::Ignore;
    CollisionDetectionMode detection_mode = CollisionDetectionMode::Discrete;
    std::optional<OneWayCollision> one_way;
    PhysicsMaterial material{};
};

struct CollisionResponseContext
{
    elysia::core::Vector2 first_displacement{};
    elysia::core::Vector2 second_displacement{};
    float epsilon = 1.0e-5f;
    bool transiently_ignored = false;
};

struct CollisionDetectionContext
{
    double fixed_delta_seconds = 0.0;
    float epsilon = 1.0e-5f;
};

class ICollisionDetectionStrategy
{
public:
    virtual ~ICollisionDetectionStrategy() = default;

    [[nodiscard]] virtual std::optional<CollisionHit> detect(
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        const CollisionDetectionContext& context) const noexcept = 0;
};

class ICollisionResponseStrategy
{
public:
    virtual ~ICollisionResponseStrategy() = default;

    [[nodiscard]] virtual CollisionResponse classify(
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        const CollisionHit& hit,
        const CollisionResponseContext& context) const noexcept = 0;
};
}
