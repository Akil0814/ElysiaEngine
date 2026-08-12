#include "colored_block_object.h"

#include "../../engine/core/render/colors.h"
#include "../../engine/core/render/render_command.h"

#include <algorithm>

namespace example::physics_demo
{
ColoredBlockObject::ColoredBlockObject(
    elysia::core::DepthLayer layer,
    const elysia::core::Rect& rect,
    elysia::core::Color color,
    int order) noexcept
    : GameObject(layer, order), _base_color(color)
{
    set_world_rect(rect);
}

void ColoredBlockObject::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    if (world_rect().is_empty())
        return;
    out_commands.push_back(
        elysia::core::make_world_fill_rect_command(render_rect(), display_color()));
}

void ColoredBlockObject::flash(double seconds) noexcept
{
    _flash_remaining = std::max(_flash_remaining, std::max(0.0, seconds));
}

void ColoredBlockObject::update_visual(double delta) noexcept
{
    _flash_remaining = std::max(0.0, _flash_remaining - std::max(0.0, delta));
}

elysia::core::Color ColoredBlockObject::display_color() const noexcept
{
    elysia::core::Color color = _base_color;
    if (_dead_visual)
        color = elysia::core::colors::gray_500;
    else if (_flash_remaining > 0.0)
        color = elysia::core::colors::white;
    color.a = static_cast<std::uint8_t>(
        (static_cast<unsigned>(_alpha) * color.a) / 255u);
    return color;
}
}
