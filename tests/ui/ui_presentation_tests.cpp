#define SDL_MAIN_HANDLED

#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/core/ui_render_command_range_utils.h"
#include "engine/ui/widgets/ui_button.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
using elysia::tests::require;

const elysia::core::UiRenderCommand* find_command(
    const std::vector<elysia::core::UiRenderCommand>& commands,
    elysia::core::UiRenderCommandType type)
{
    for (const auto& command : commands)
    {
        if (command.type == type)
            return &command;
    }
    return nullptr;
}

void test_presentation_translation_animation()
{
    elysia::ui::UiButton button(elysia::core::Rect{ 10,20,100,40 });
    button.bind_translation_animation("linear",{
        .from = elysia::core::Vector2(-40.0f,0.0f),
        .to = elysia::core::Vector2(0.0f,0.0f),
        .duration_seconds = 2.0,
        .easing = elysia::ui::UiTranslationAnimationEasing::Linear
    });
    require(button.play_translation_animation("linear"),"bound translation animation should play");
    require(button.presentation_translation().nearly_equals({ -40.0f,0.0f }),"play should apply animation start translation");
    require(button.screen_rect().nearly_equals(elysia::core::Rect{ 10,20,100,40 }),"presentation animation must not change layout rect");

    button.update_presentation_animations(1.0);
    require(button.presentation_translation().nearly_equals({ -20.0f,0.0f }),"linear animation should interpolate translation");
    require(button.presentation_screen_rect().nearly_equals(elysia::core::Rect{ -10,20,100,40 }),"presentation rect should include local translation");

    button.bind_translation_animation("instant",{
        .from = elysia::core::Vector2(1.0f,2.0f),
        .to = elysia::core::Vector2(3.0f,4.0f),
        .duration_seconds = 0.0
    });
    require(button.play_translation_animation("instant"),"second animation should replace current track");
    require(button.presentation_translation().nearly_equals({ 3.0f,4.0f }),"zero duration animation should complete at destination");
    require(!button.is_translation_animation_playing(),"zero duration animation should not remain playing");
    require(!button.play_translation_animation("missing"),"unknown animation must not play");
    button.reset();
    require(button.presentation_translation().is_zero(),"reset should clear presentation translation");
    require(!button.active_translation_animation().has_value(),"reset should clear animation definitions and active state");
}

void test_presentation_translation_subtree_render_and_hit_test()
{
    elysia::ui::UiChildHost root(elysia::core::Rect{ 0,0,300,200 });
    root.set_presentation_translation({ 10.0f,20.0f });
    auto button = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,80,30 });
    elysia::ui::UiButton* raw = button.get();
    raw->set_presentation_translation({ 5.0f,7.0f });
    int clicks = 0;
    raw->set_on_click([&clicks] { ++clicks; });
    root.add_child(std::move(button));

    require(raw->accumulated_presentation_translation().nearly_equals({ 15.0f,27.0f }),
        "child presentation translation should accumulate ancestors");
    require(raw->presentation_screen_rect().nearly_equals(elysia::core::Rect{ 15,27,80,30 }),
        "child presentation rect should include ancestor translation");

    root.on_ui_input_event({
        .type = elysia::ui::UiInputEventType::PointerPressed,
        .device = elysia::input::InputDevice::Mouse,
        .control = elysia::input::RawInputControl::MouseLeft,
        .mouse_x = 20,
        .mouse_y = 30
    });
    root.on_ui_input_event({
        .type = elysia::ui::UiInputEventType::PointerReleased,
        .device = elysia::input::InputDevice::Mouse,
        .control = elysia::input::RawInputControl::MouseLeft,
        .mouse_x = 20,
        .mouse_y = 30
    });
    require(clicks == 1,"pointer hit test should follow presentation translation");

    std::vector<elysia::core::UiRenderCommand> commands;
    root.submit_ui_render_commands(commands);
    elysia::ui::render_command_range_utils::apply_translation_to_range(commands,0,root.presentation_translation());
    const auto* fill = find_command(commands,elysia::core::UiRenderCommandType::FillRect);
    require(fill && fill->screen_rect.x() == 15.0f && fill->screen_rect.y() == 27.0f,
        "root and child translations should each apply exactly once to render commands");
}

void test_render_command_range_translation()
{
    std::vector<elysia::core::UiRenderCommand> commands;
    auto rect = elysia::core::make_ui_fill_rect_command(
        elysia::core::Rect{ 1,2,3,4 },elysia::core::Color{});
    elysia::core::set_ui_command_clip_rect(rect,elysia::core::Rect{ 5,6,7,8 });
    commands.push_back(rect);
    commands.push_back(elysia::core::make_ui_draw_line_command({ 1,2 },{ 3,4 },elysia::core::Color{}));
    commands.push_back(elysia::core::make_ui_fill_circle_command({ 5,6 },4.0f,elysia::core::Color{}));
    elysia::ui::render_command_range_utils::apply_translation_to_range(commands,0,{ 10.0f,-2.0f });

    require(commands[0].screen_rect.nearly_equals({ 11,0,3,4 }) && commands[0].clip_rect.nearly_equals({ 15,4,7,8 }),
        "range translation should move rect and clip geometry");
    require(commands[1].line_start.nearly_equals({ 11,0 }) && commands[1].line_end.nearly_equals({ 13,2 }),
        "range translation should move lines");
    require(commands[2].circle_center.nearly_equals({ 15,4 }),"range translation should move circles");
}
}

int main()
{
    test_presentation_translation_animation();
    test_presentation_translation_subtree_render_and_hit_test();
    test_render_command_range_translation();
    std::cout << "ui presentation tests passed\n";
    return EXIT_SUCCESS;
}

