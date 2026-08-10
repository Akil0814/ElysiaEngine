#pragma once

#include "../collision/collider.h"
#include "../collision/collision_contact.h"

#include <optional>
#include <span>
#include <vector>

namespace elysia::physics
{
struct ColliderView
{
    const Collider* collider = nullptr;
    elysia::core::Vector2 previous_owner_origin{};
    elysia::core::Vector2 current_owner_origin{};
};

class IBroadPhaseStrategy
{
public:
    virtual ~IBroadPhaseStrategy() = default;

    virtual void collect_pairs(
        std::span<const ColliderView> colliders,
        std::vector<CollisionPair>& out_pairs
    ) const = 0;
};

class ICollisionDetectionStrategy
{
public:
    virtual ~ICollisionDetectionStrategy() = default;

    [[nodiscard]] virtual std::optional<CollisionHit> detect(
        const ColliderView& first,
        const ColliderView& second,
        double delta_seconds
    ) const = 0;
};

class ICollisionResponseStrategy
{
public:
    virtual ~ICollisionResponseStrategy() = default;

    [[nodiscard]] virtual CollisionResponse resolve(
        const ColliderView& first,
        const ColliderView& second,
        const CollisionHit& hit,
        double delta_seconds
    ) const = 0;
};
}
