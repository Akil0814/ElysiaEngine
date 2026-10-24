#pragma once
#include "engine/core/render/render_command.h"
#include "engine/physics/collision/world_shape.h"
namespace example::showcase
{
inline void submit_collider_shape(const elysia::physics::ColliderShape &shape, elysia::physics::PhysicsPose pose,
                                  elysia::core::Color color, std::vector<elysia::core::RenderCommand> &out)
{
    using namespace elysia::core;
    using namespace elysia::physics;
    if (const auto *box = std::get_if<BoxShape>(&shape); box && pose.angle == 0 && box->local_angle == 0)
    {
        out.push_back(make_world_fill_rect_command(box->local_bounds().translated(pose.position), color));
        return;
    }
    auto world = transform_collider_shape(shape, pose);
    std::visit(
        [&](const auto &s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, WorldCircle>)
                out.push_back(make_world_fill_circle_command(s.center, s.radius, color));
            else if constexpr (std::is_same_v<T, WorldCapsule>)
            {
                out.push_back(make_world_fill_circle_command(s.first, s.radius, color));
                out.push_back(make_world_fill_circle_command(s.second, s.radius, color));
                auto axis = (s.second - s.first).normalized();
                Vector2 n{-axis.y * s.radius, axis.x * s.radius};
                out.push_back(make_world_fill_triangle_command(s.first + n, s.second + n, s.second - n, color));
                out.push_back(make_world_fill_triangle_command(s.first + n, s.second - n, s.first - n, color));
            }
            else if constexpr (std::is_same_v<T,WorldSegment>)
                out.push_back(make_world_draw_line_command(s.first,s.second,color,1));
            else
                for (int i = 1; i + 1 < s.vertex_count; ++i)
                    out.push_back(
                        make_world_fill_triangle_command(s.vertices[0], s.vertices[i], s.vertices[i + 1], color));
        },
        world);
}
} // namespace example::showcase
