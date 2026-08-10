#pragma once

#include "render_command.h"

#include "../../camera/camera.h"

#include <vector>

namespace elysia::core
{
[[nodiscard]] inline ScreenRenderCommand project_render_command_to_screen(
    const RenderCommand& render_command,
    const elysia::camera::Camera& camera
) noexcept
{
    ScreenRenderCommand projected_command;
    projected_command.texture = render_command.texture;
    projected_command.screen_rect = camera.world_to_screen(render_command.command_rect);
    projected_command.alpha = render_command.alpha;
    projected_command.texture_color_modulation =
        render_command.texture_color_modulation;
    projected_command.use_src_rect = render_command.use_src_rect;
    projected_command.src_rect = render_command.src_rect;
    projected_command.rotation_degrees = render_command.rotation_degrees;
    projected_command.rotation_origin = render_command.rotation_origin;
    projected_command.flip = render_command.flip;
    return projected_command;
}

inline void project_render_commands_to_screen(
    const std::vector<RenderCommand>& render_commands,
    const elysia::camera::Camera& camera,
    std::vector<ScreenRenderCommand>& out_commands
) noexcept
{
    out_commands.clear();
    out_commands.reserve(render_commands.size());

    for (const RenderCommand& render_command : render_commands)
    {
        out_commands.push_back(project_render_command_to_screen(render_command, camera));
    }
}
}
