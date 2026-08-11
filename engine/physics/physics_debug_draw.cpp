#include "physics_debug_draw.h"

#include "../tools/debug_draw.h"

namespace elysia::physics
{
void submit_physics_debug_snapshot(
    const PhysicsDebugSnapshot& snapshot,
    elysia::tools::DebugDraw& debug_draw)
{
    using elysia::tools::DebugDrawCategory;
    const auto draw_shape = [&](const WorldColliderShape& shape,
                                DebugDrawCategory category,
                                elysia::core::Color color)
    {
        if (!debug_draw.is_enabled(category))
            return;
        if (const auto* box = std::get_if<WorldAabb>(&shape))
            debug_draw.draw_rect(category, box->rect, color);
        else
        {
            const auto& circle = std::get<WorldCircle>(shape);
            debug_draw.draw_circle(category, circle.center, circle.radius, color);
        }
    };
    for (const PhysicsDebugShape& shape : snapshot.shapes)
    {
        draw_shape(shape.current, DebugDrawCategory::PhysicsCollider, {80, 230, 120});
        draw_shape(shape.previous, DebugDrawCategory::PhysicsCcd, {110, 120, 150, 180});
        if (debug_draw.is_enabled(DebugDrawCategory::PhysicsBroadPhase))
            debug_draw.draw_rect(
                DebugDrawCategory::PhysicsBroadPhase,
                shape.swept_bounds,
                {230, 190, 70, 180});
    }
    for (const CollisionContact& contact : snapshot.contacts)
    {
        for (std::uint8_t i = 0; i < contact.manifold.contact_point_count; ++i)
        {
            const auto point = contact.manifold.contact_points[i];
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsContact))
                debug_draw.draw_point(
                    DebugDrawCategory::PhysicsContact, point, 4.0f, {255, 80, 80});
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsContactNormal))
                debug_draw.draw_line(
                    DebugDrawCategory::PhysicsContactNormal,
                    point,
                    point + contact.manifold.normal * 16.0f,
                    {255, 120, 40});
        }
    }
    for (const PhysicsDebugVelocity& velocity : snapshot.velocities)
    {
        if (debug_draw.is_enabled(DebugDrawCategory::PhysicsVelocity))
            debug_draw.draw_line(
                DebugDrawCategory::PhysicsVelocity,
                velocity.origin,
                velocity.origin + velocity.velocity * 0.1f,
                {80, 180, 255});
    }
}
}
