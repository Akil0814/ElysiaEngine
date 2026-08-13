#pragma once

#include "../../../engine/core/game_object.h"
#include "../../../engine/core/render/color.h"

namespace example::demo::physics
{
class ColoredBlockObject : public elysia::core::GameObject
{
public:
    ColoredBlockObject(
        elysia::core::DepthLayer layer,
        const elysia::core::Rect& rect,
        elysia::core::Color color,
        int order = 0) noexcept;

    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;

    void set_base_color(elysia::core::Color color) noexcept { _base_color = color; }
    [[nodiscard]] elysia::core::Color base_color() const noexcept { return _base_color; }
    void set_alpha(std::uint8_t alpha) noexcept { _alpha = alpha; }
    void flash(double seconds) noexcept;
    void set_dead_visual(bool dead) noexcept { _dead_visual = dead; }

protected:
    void update_visual(double delta) noexcept;
    [[nodiscard]] elysia::core::Color display_color() const noexcept;

private:
    elysia::core::Color _base_color{};
    std::uint8_t _alpha = 255;
    double _flash_remaining = 0.0;
    bool _dead_visual = false;
};
}
