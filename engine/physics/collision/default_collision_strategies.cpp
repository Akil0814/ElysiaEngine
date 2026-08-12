#include "default_collision_strategies.h"

#include <algorithm>
#include <cmath>

namespace elysia::physics
{
namespace
{
[[nodiscard]] bool bounds_touch(
    const elysia::core::Rect& first,
    const elysia::core::Rect& second) noexcept
{
    return first.touches_or_intersects(second);
}

[[nodiscard]] BroadPhasePair normalized_broad_pair(
    ColliderId first,
    ColliderId second) noexcept
{
    return first < second
        ? BroadPhasePair{first, second}
        : BroadPhasePair{second, first};
}

void normalize_pairs(std::vector<BroadPhasePair>& pairs)
{
    std::ranges::sort(pairs);
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
}

void normalize_candidates(std::vector<ColliderId>& candidates)
{
    std::ranges::sort(candidates);
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
}

[[nodiscard]] bool one_way_allows(
    const CollisionShapeView& platform,
    const CollisionShapeView& actor,
    const CollisionManifold& actor_to_platform,
    const elysia::core::Vector2& relative_actor_move,
    const OneWayCollision& one_way) noexcept
{
    const auto previous_platform = shape_bounds(platform.previous_shape);
    const auto previous_actor = shape_bounds(actor.previous_shape);
    const float tolerance = std::max(0.0f, one_way.tolerance);
    const auto allows = [&](PassThroughDirection direction) noexcept
    {
        return has_pass_through_direction(one_way.pass_through, direction);
    };

    if (allows(PassThroughDirection::Up)
        && relative_actor_move.y < 0.0f
        && previous_actor.top() >= previous_platform.bottom() - tolerance
        && actor_to_platform.normal.y < -0.5f)
    {
        return true;
    }
    if (allows(PassThroughDirection::Down)
        && relative_actor_move.y > 0.0f
        && previous_actor.bottom() <= previous_platform.top() + tolerance
        && actor_to_platform.normal.y > 0.5f)
    {
        return true;
    }
    if (allows(PassThroughDirection::Left)
        && relative_actor_move.x < 0.0f
        && previous_actor.left() >= previous_platform.right() - tolerance
        && actor_to_platform.normal.x < -0.5f)
    {
        return true;
    }
    if (allows(PassThroughDirection::Right)
        && relative_actor_move.x > 0.0f
        && previous_actor.right() <= previous_platform.left() + tolerance
        && actor_to_platform.normal.x > 0.5f)
    {
        return true;
    }
    return false;
}
}

void BruteForceBroadPhaseIndex::synchronize(
    std::span<const BroadPhaseProxy> proxies)
{
    _proxies.assign(proxies.begin(), proxies.end());
    std::ranges::sort(_proxies, {}, &BroadPhaseProxy::collider);
}

void BruteForceBroadPhaseIndex::collect_pairs(
    std::vector<BroadPhasePair>& out_pairs) const
{
    out_pairs.clear();
    for (std::size_t i = 0; i < _proxies.size(); ++i)
    {
        const auto& first = _proxies[i];
        if (!first.enabled || first.collider == InvalidColliderId)
            continue;
        for (std::size_t j = i + 1; j < _proxies.size(); ++j)
        {
            const auto& second = _proxies[j];
            if (!second.enabled || second.collider == InvalidColliderId)
                continue;
            if (!collision_filters_allow(first.filter, second.filter)
                || !bounds_touch(first.swept_bounds, second.swept_bounds))
                continue;
            out_pairs.push_back(normalized_broad_pair(first.collider, second.collider));
        }
    }
    normalize_pairs(out_pairs);
}

void BruteForceBroadPhaseIndex::query_aabb(
    const elysia::core::Rect& bounds,
    std::vector<ColliderId>& out_candidates) const
{
    out_candidates.clear();
    for (const auto& proxy : _proxies)
    {
        if (proxy.enabled && proxy.collider != InvalidColliderId
            && bounds_touch(bounds, proxy.current_bounds))
        {
            out_candidates.push_back(proxy.collider);
        }
    }
    normalize_candidates(out_candidates);
}

void BruteForceBroadPhaseIndex::clear() noexcept
{
    _proxies.clear();
}

void SweepAndPruneBroadPhaseIndex::synchronize(
    std::span<const BroadPhaseProxy> proxies)
{
    _proxies.assign(proxies.begin(), proxies.end());
    std::ranges::sort(_proxies, [](const auto& first, const auto& second)
    {
        if (first.swept_bounds.left() != second.swept_bounds.left())
            return first.swept_bounds.left() < second.swept_bounds.left();
        if (first.swept_bounds.right() != second.swept_bounds.right())
            return first.swept_bounds.right() < second.swept_bounds.right();
        return first.collider < second.collider;
    });
}

void SweepAndPruneBroadPhaseIndex::collect_pairs(
    std::vector<BroadPhasePair>& out_pairs) const
{
    out_pairs.clear();
    for (std::size_t i = 0; i < _proxies.size(); ++i)
    {
        const auto& first = _proxies[i];
        if (!first.enabled || first.collider == InvalidColliderId)
            continue;
        for (std::size_t j = i + 1; j < _proxies.size(); ++j)
        {
            const auto& second = _proxies[j];
            if (second.swept_bounds.left() > first.swept_bounds.right())
                break;
            if (!second.enabled || second.collider == InvalidColliderId)
                continue;
            if (collision_filters_allow(first.filter, second.filter)
                && bounds_touch(first.swept_bounds, second.swept_bounds))
                out_pairs.push_back(normalized_broad_pair(first.collider, second.collider));
        }
    }
    normalize_pairs(out_pairs);
}

void SweepAndPruneBroadPhaseIndex::query_aabb(
    const elysia::core::Rect& bounds,
    std::vector<ColliderId>& out_candidates) const
{
    out_candidates.clear();
    for (const auto& proxy : _proxies)
    {
        if (proxy.swept_bounds.left() > bounds.right())
            break;
        if (!proxy.enabled || proxy.collider == InvalidColliderId
            || proxy.current_bounds.right() < bounds.left())
        {
            continue;
        }
        if (bounds_touch(bounds, proxy.current_bounds))
            out_candidates.push_back(proxy.collider);
    }
    normalize_candidates(out_candidates);
}

void SweepAndPruneBroadPhaseIndex::clear() noexcept
{
    _proxies.clear();
}

std::optional<CollisionHit> DefaultDiscreteCollisionStrategy::detect(
    const CollisionShapeView& first,
    const CollisionShapeView& second,
    const CollisionDetectionContext& context) const noexcept
{
    return detect_discrete_shapes(
        first.current_shape, second.current_shape, context.epsilon);
}

std::optional<CollisionHit> SweptAabbCollisionStrategy::detect(
    const CollisionShapeView& first,
    const CollisionShapeView& second,
    const CollisionDetectionContext& context) const noexcept
{
    (void)context.fixed_delta_seconds;
    const auto* first_previous = std::get_if<WorldAabb>(&first.previous_shape);
    const auto* first_current = std::get_if<WorldAabb>(&first.current_shape);
    const auto* second_previous = std::get_if<WorldAabb>(&second.previous_shape);
    const auto* second_current = std::get_if<WorldAabb>(&second.current_shape);
    if (!first_previous || !first_current || !second_previous || !second_current)
        return std::nullopt;
    return detect_swept_aabbs(
        *first_previous,
        *first_current,
        *second_previous,
        *second_current,
        context.epsilon);
}

CollisionResponse DefaultCollisionResponseStrategy::classify(
    const CollisionShapeView& first,
    const CollisionShapeView& second,
    const CollisionHit& hit,
    const CollisionResponseContext& context) const noexcept
{
    if (context.transiently_ignored)
        return CollisionResponse::Ignore;
    const CollisionResponse combined = combine_collision_responses(
        first.response, second.response);
    if (combined != CollisionResponse::Block)
        return combined;

    if (first.one_way)
    {
        CollisionManifold actor_to_platform = hit.manifold;
        actor_to_platform.normal = -actor_to_platform.normal;
        if (one_way_allows(
                first,
                second,
                actor_to_platform,
                context.second_displacement - context.first_displacement,
                *first.one_way))
        {
            return CollisionResponse::Ignore;
        }
    }
    if (second.one_way
        && one_way_allows(
            second,
            first,
            hit.manifold,
            context.first_displacement - context.second_displacement,
            *second.one_way))
    {
        return CollisionResponse::Ignore;
    }
    return CollisionResponse::Block;
}

CollisionStrategySet make_default_collision_strategies()
{
    return CollisionStrategySet{
        std::make_unique<SweepAndPruneBroadPhaseIndex>(),
        std::make_unique<DefaultDiscreteCollisionStrategy>(),
        std::make_unique<SweptAabbCollisionStrategy>(),
        std::make_unique<DefaultCollisionResponseStrategy>()
    };
}

CollisionStrategySet make_brute_force_collision_strategies()
{
    return CollisionStrategySet{
        std::make_unique<BruteForceBroadPhaseIndex>(),
        std::make_unique<DefaultDiscreteCollisionStrategy>(),
        std::make_unique<SweptAabbCollisionStrategy>(),
        std::make_unique<DefaultCollisionResponseStrategy>()
    };
}
}
