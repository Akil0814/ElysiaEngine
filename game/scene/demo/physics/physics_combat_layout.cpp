#include "physics_combat_layout.h"

#include <algorithm>

namespace example::scene
{
PhysicsTestLayout make_physics_test_layout(float width,float height) noexcept
{
    PhysicsTestLayout l;
    l.viewport={0,0,width,height};
    l.title={16,12,width-32,32};l.purpose={16,48,width-32,28};l.expected={16,78,width-32,28};
    const float button_width=(width-32-5*8)/6;
    for(int i=0;i<6;++i)l.actions[i]={16+i*(button_width+8),116,button_width,38};
    l.details={16,170,280,height-186};l.arena={312,170,width-328,height-186};
    return l;
}
PhysicsCombatLayout make_physics_combat_layout(
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

    return PhysicsCombatLayout{
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

elysia::ui::UiLayoutChildOptions physics_combat_layout_options(
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
