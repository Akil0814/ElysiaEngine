#include "physics_system.h"

#include "../physics_world_config.h"

namespace elysia::physics
{
void PhysicsSystem::integrate(
    std::span<PhysicsBodyView> body_views,
    const PhysicsWorldConfig& config,
    double fixed_delta_seconds) const noexcept
{
    (void)body_views;
    (void)config;
    (void)fixed_delta_seconds;
}

void PhysicsSystem::clear_forces(
    std::span<PhysicsBodyView> body_views) const noexcept
{
    (void)body_views;
}
}
