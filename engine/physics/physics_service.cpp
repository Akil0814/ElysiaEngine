#include "physics_service.h"

#include <utility>

namespace elysia::physics
{
bool PhysicsService::configure(CollisionStrategyFactories factories)
{
    if (_configured
        || !factories.create_broad_phase
        || !factories.create_discrete_detector
        || !factories.create_continuous_detector
        || !factories.create_response_strategy)
    {
        return false;
    }

    _factories = std::move(factories);
    _configured = true;
    return true;
}

bool PhysicsService::apply_to(CollisionSystem& collision_system) const
{
    if (!_configured)
        return false;

    std::unique_ptr<IBroadPhaseStrategy> broad_phase =
        _factories.create_broad_phase();
    std::unique_ptr<ICollisionDetectionStrategy> discrete_detector =
        _factories.create_discrete_detector();
    std::unique_ptr<ICollisionDetectionStrategy> continuous_detector =
        _factories.create_continuous_detector();
    std::unique_ptr<ICollisionResponseStrategy> response_strategy =
        _factories.create_response_strategy();

    if (!broad_phase || !discrete_detector || !continuous_detector || !response_strategy)
        return false;

    collision_system.set_broad_phase_strategy(std::move(broad_phase));
    collision_system.set_discrete_detection_strategy(std::move(discrete_detector));
    collision_system.set_continuous_detection_strategy(std::move(continuous_detector));
    collision_system.set_response_strategy(std::move(response_strategy));
    return true;
}

bool PhysicsService::is_configured() const noexcept
{
    return _configured;
}

void PhysicsService::shutdown() noexcept
{
    _factories = {};
    _configured = false;
}
}
