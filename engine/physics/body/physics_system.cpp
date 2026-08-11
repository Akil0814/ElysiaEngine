#include "physics_system.h"

#include "../physics_world_config.h"

#include <algorithm>
#include <cmath>

namespace elysia::physics
{
void PhysicsSystem::integrate(
    std::span<PhysicsObjectState> object_states,
    const PhysicsWorldConfig& config,
    double fixed_delta_seconds) const noexcept
{
    if (!std::isfinite(fixed_delta_seconds) || fixed_delta_seconds <= 0.0)
        return;
    const float dt = static_cast<float>(fixed_delta_seconds);
    for (PhysicsObjectState& state : object_states)
    {
        PhysicsBody* body = state.body;
        if (!body)
            continue;

        const elysia::core::Vector2 force = body->accumulated_force;
        body->accumulated_force = {};
        if (!body->enabled || body->type == BodyType::Static)
            continue;
        if (!std::isfinite(body->velocity.x) || !std::isfinite(body->velocity.y))
        {
            body->velocity = {};
            continue;
        }

        if (body->type == BodyType::Dynamic)
        {
            if (!std::isfinite(body->mass) || body->mass <= 0.0f
                || !std::isfinite(body->gravity_scale)
                || !std::isfinite(force.x) || !std::isfinite(force.y))
            {
                continue;
            }
            const elysia::core::Vector2 acceleration =
                config.gravity * body->gravity_scale + force / body->mass;
            body->velocity += acceleration * dt;
        }

        if (body->type == BodyType::Dynamic)
        {
            const float damping = std::isfinite(body->linear_damping)
                ? std::max(0.0f, body->linear_damping)
                : 0.0f;
            body->velocity *= std::max(0.0f, 1.0f - damping * dt);
        }
        if (std::isfinite(body->max_speed.x) && body->max_speed.x > 0.0f)
            body->velocity.x = std::clamp(
                body->velocity.x, -body->max_speed.x, body->max_speed.x);
        if (std::isfinite(body->max_speed.y) && body->max_speed.y > 0.0f)
            body->velocity.y = std::clamp(
                body->velocity.y, -body->max_speed.y, body->max_speed.y);
        state.current_owner_origin += body->velocity * dt;
    }
}
}
