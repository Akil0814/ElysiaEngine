#pragma once

#include "collision_system.h"

namespace elysia::physics
{
class BruteForceBroadPhaseIndex final : public IBroadPhaseIndex
{
public:
    void synchronize(std::span<const BroadPhaseProxy> proxies) override;
    void collect_pairs(std::vector<BroadPhasePair>& out_pairs) const override;
    void query_aabb(
        const elysia::core::Rect& bounds,
        std::vector<ColliderId>& out_candidates) const override;
    void clear() noexcept override;

private:
    std::vector<BroadPhaseProxy> _proxies;
};

class SweepAndPruneBroadPhaseIndex final : public IBroadPhaseIndex
{
public:
    void synchronize(std::span<const BroadPhaseProxy> proxies) override;
    void collect_pairs(std::vector<BroadPhasePair>& out_pairs) const override;
    void query_aabb(
        const elysia::core::Rect& bounds,
        std::vector<ColliderId>& out_candidates) const override;
    void clear() noexcept override;

private:
    std::vector<BroadPhaseProxy> _proxies;
};

class DefaultDiscreteCollisionStrategy final
    : public ICollisionDetectionStrategy
{
public:
    [[nodiscard]] std::optional<CollisionHit> detect(
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        double delta_seconds) const noexcept override;
};

class SweptAabbCollisionStrategy final
    : public ICollisionDetectionStrategy
{
public:
    [[nodiscard]] std::optional<CollisionHit> detect(
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        double delta_seconds) const noexcept override;
};

class DefaultCollisionResponseStrategy final
    : public ICollisionResponseStrategy
{
public:
    [[nodiscard]] CollisionResponse classify(
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        const CollisionHit& hit,
        const CollisionResponseContext& context) const noexcept override;
};

[[nodiscard]] CollisionStrategySet make_default_collision_strategies();
[[nodiscard]] CollisionStrategySet make_brute_force_collision_strategies();
}
