#include "collision_system.h"

#include <utility>

namespace elysia::physics
{
void CollisionSystem::set_broad_phase_strategy(
    std::unique_ptr<IBroadPhaseStrategy> strategy
) noexcept
{
    _broad_phase_strategy = std::move(strategy);
}

void CollisionSystem::set_discrete_detection_strategy(
    std::unique_ptr<ICollisionDetectionStrategy> strategy
) noexcept
{
    _discrete_detection_strategy = std::move(strategy);
}

void CollisionSystem::set_continuous_detection_strategy(
    std::unique_ptr<ICollisionDetectionStrategy> strategy
) noexcept
{
    _continuous_detection_strategy = std::move(strategy);
}

void CollisionSystem::set_response_strategy(
    std::unique_ptr<ICollisionResponseStrategy> strategy
) noexcept
{
    _response_strategy = std::move(strategy);
}

const IBroadPhaseStrategy* CollisionSystem::broad_phase_strategy() const noexcept
{
    return _broad_phase_strategy.get();
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
}
