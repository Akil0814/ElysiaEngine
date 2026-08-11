#include "physics_demo_layout.h"

#include <algorithm>

namespace example::scene
{
PhysicsDemoLayout make_physics_demo_layout(
    float logical_width,
    float logical_height) noexcept
{
    const float width = std::max(0.0f, logical_width);
    const float height = std::max(0.0f, logical_height);
    const float horizontal_content_width = std::max(0.0f, width - 48.0f);

    const float health_width = std::min(260.0f, horizontal_content_width);
    const float health_x = std::max(24.0f, width - 24.0f - health_width);
    const float title_width = std::max(
        0.0f,
        std::min(520.0f, health_x - 40.0f));

    const float status_width = std::min(440.0f, std::max(0.0f, width - 40.0f));
    const float status_height = std::min(80.0f, std::max(0.0f, height - 40.0f));
    const float menu_width = std::min(420.0f, std::max(0.0f, width - 40.0f));
    const float menu_height = std::min(520.0f, std::max(0.0f, height - 40.0f));

    return PhysicsDemoLayout{
        .viewport = {0.0f, 0.0f, width, height},
        .hud_panel = {
            10.0f,
            10.0f,
            std::max(0.0f, width - 20.0f),
            std::min(116.0f, std::max(0.0f, height - 20.0f))},
        .title = {24.0f, 18.0f, title_width, 32.0f},
        .controls = {24.0f, 52.0f, horizontal_content_width, 24.0f},
        .health = {health_x, 20.0f, health_width, 18.0f},
        .stats = {24.0f, 84.0f, horizontal_content_width, 26.0f},
        .status = elysia::core::Rect::from_center(
            {width * 0.5f, height * 0.5f},
            {status_width, status_height}),
        .menu_list = elysia::core::Rect::from_center(
            {width * 0.5f, height * 0.5f},
            {menu_width, menu_height})};
}

elysia::ui::UiLayoutChildOptions physics_demo_layout_options(
    const elysia::core::Rect& rect) noexcept
{
    elysia::ui::UiLayoutChildOptions options;
    options._anchor = elysia::ui::UiLayoutAnchor::TopLeft;
    options._margin.left = rect.x();
    options._margin.top = rect.y();
    options._size_override = rect.size();
    options._use_size_override = true;
    return options;
}
}
