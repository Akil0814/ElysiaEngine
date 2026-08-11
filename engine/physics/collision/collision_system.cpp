#include "collision_system.h"

#include <utility>

namespace elysia::physics
{
void CollisionSystem::set_broad_phase_index(
    std::unique_ptr<IBroadPhaseIndex> index) noexcept
{
    _broad_phase_index = std::move(index);
}

void CollisionSystem::set_discrete_detection_strategy(
    std::unique_ptr<ICollisionDetectionStrategy> strategy) noexcept
{
    _discrete_detection_strategy = std::move(strategy);
}

void CollisionSystem::set_continuous_detection_strategy(
    std::unique_ptr<ICollisionDetectionStrategy> strategy) noexcept
{
    _continuous_detection_strategy = std::move(strategy);
}

void CollisionSystem::set_response_strategy(
    std::unique_ptr<ICollisionResponseStrategy> strategy) noexcept
{
    _response_strategy = std::move(strategy);
}

const IBroadPhaseIndex* CollisionSystem::broad_phase_index() const noexcept
{
    return _broad_phase_index.get();
}

const ICollisionDetectionStrategy* CollisionSystem::discrete_detection_strategy() const noexcept
{
    return _discrete_detection_strategy.get();
}

const ICollisionDetectionStrategy* CollisionSystem::continuous_detection_strategy() const noexcept
{
    return _continuous_detection_strategy.get();
}

const ICollisionResponseStrategy* CollisionSystem::response_strategy() const noexcept
{
    return _response_strategy.get();
}

void CollisionSystem::evaluate(
    std::span<const ColliderView> collider_views,
    const ITileCollisionWorld* tile_world,
    double fixed_delta_seconds,
    CollisionFrame& out_frame) noexcept
{
    (void)collider_views;
    (void)tile_world;
    (void)fixed_delta_seconds;
    out_frame.clear();
}
}
