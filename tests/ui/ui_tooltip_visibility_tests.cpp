#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/composites/ui_tooltip.h"
#include "engine/ui/core/ui_child_host.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using namespace elysia;
using elysia::tests::require;

constexpr core::Color marker_color{ 17,43,91,255 };

class FixedHost final : public ui::UiChildHost
{
public:
    using UiChildHost::UiChildHost;

protected:
    void rebuild_layout() override {}
};

class MarkerElement final : public ui::UiElement
{
public:
    explicit MarkerElement(const core::Rect& rect) noexcept : UiElement(rect) {}

    void submit_ui_render_commands(std::vector<core::UiRenderCommand>& out_commands) const override
    {
        if (is_visible())
            out_commands.push_back(core::make_ui_fill_rect_command(screen_rect(),marker_color));
    }
};

ui::UiInputEvent mouse_move(int mouse_x,int mouse_y)
{
    return {
        .type = ui::UiInputEventType::MouseMoved,
        .device = input::InputDevice::Mouse,
        .mouse_x = mouse_x,
        .mouse_y = mouse_y
    };
}

void use_gamepad_focus(ui::UiWindow& window)
{
    (void)window.on_ui_input_event({
        .type = ui::UiInputEventType::ActionPressed,
        .device = input::InputDevice::Gamepad
    });
}

bool has_tooltip_marker(ui::UiWindow& window)
{
    std::vector<core::UiRenderCommand> commands;
    window.submit_ui_render_commands(commands);
    for (const auto& command : commands)
    {
        if (command.type == core::UiRenderCommandType::FillRect && command.color == marker_color)
            return true;
    }
    return false;
}

ui::UiTooltip* add_tooltip(ui::UiWindow& window,ui::UiElement& trigger,double show_delay = 0.0)
{
    auto* tooltip = window.create_child<ui::UiTooltip>(0);
    require(tooltip != nullptr,"window should own the tooltip");
    tooltip->bind_trigger(trigger);
    tooltip->set_content(std::make_unique<MarkerElement>(core::Rect{ 0,0,40,20 }));
    tooltip->set_show_delay(show_delay);
    tooltip->register_with_window(window);
    return tooltip;
}

void hover(ui::UiWindow& window,const ui::UiElement& trigger,double delta = 0.0)
{
    const core::Vector2 center = trigger.presentation_screen_rect().center();
    (void)window.on_ui_input_event(mouse_move(static_cast<int>(center.x),static_cast<int>(center.y)));
    window.update(delta);
}

void test_visible_trigger_and_ancestor_state()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto host = std::make_unique<FixedHost>(core::Rect{ 0,0,200,120 });
    FixedHost* host_raw = host.get();
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 20,20,80,30 });
    ui::UiButton* trigger_raw = trigger.get();
    host_raw->add_child(std::move(trigger));
    window.add_child(std::move(host));
    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw);

    window.update(0.0);
    hover(window,*trigger_raw);
    require(tooltip->is_open() && has_tooltip_marker(window),
        "a visible trigger should open and render its tooltip");

    host_raw->set_visible(false);
    require(!has_tooltip_marker(window),
        "render validation should suppress a tooltip immediately when an ancestor becomes hidden");
    window.update(0.0);
    require(!tooltip->is_open(),"an ancestor hidden state should close the tooltip on update");

    host_raw->set_visible(true);
    hover(window,*trigger_raw);
    require(tooltip->is_open(),"restoring ancestor visibility should allow the tooltip to reopen");

    host_raw->set_active(false);
    require(!has_tooltip_marker(window),
        "render validation should suppress a tooltip immediately when an ancestor becomes inactive");
    window.update(0.0);
    require(!tooltip->is_open(),"an ancestor inactive state should close the tooltip on update");

    host_raw->set_active(true);
    (void)window.on_ui_input_event(mouse_move(350,250));
    use_gamepad_focus(window);
    trigger_raw->set_focused(true);
    tooltip->update(0.0);
    require(tooltip->is_open(),
        "a visible gamepad-focused trigger should open its tooltip without pointer hover");
}

void test_tab_switch_stops_stale_tooltip()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto tabs = std::make_unique<ui::UiTabContainer>(core::Rect{ 0,0,360,240 });
    ui::UiTabContainer* tabs_raw = tabs.get();

    auto first_page = std::make_unique<FixedHost>(core::Rect{ 0,0,360,196 });
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 20,20,100,30 });
    ui::UiButton* trigger_raw = trigger.get();
    first_page->add_child(std::move(trigger));
    require(tabs_raw->add_tab(ui::ui_raw_text("First"),std::move(first_page)).added,
        "first tooltip tab should be added");
    require(tabs_raw->add_tab(
        ui::ui_raw_text("Second"),
        std::make_unique<FixedHost>(core::Rect{ 0,0,360,196 })).added,
        "second tooltip tab should be added");
    window.add_child(std::move(tabs));
    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw);

    window.update(0.0);
    hover(window,*trigger_raw);
    require(tooltip->is_open() && has_tooltip_marker(window),
        "the selected tab trigger should open its tooltip");

    require(tabs_raw->set_selected_index(1),"tab selection should switch to the second page");
    require(!has_tooltip_marker(window),
        "a hidden tab trigger must stop rendering its tooltip before the next update");
    window.update(0.0);
    require(!tooltip->is_open(),"a hidden tab trigger should close its tooltip on update");
}

void test_clipping_and_presentation_translation()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto host = std::make_unique<FixedHost>(core::Rect{ 0,0,100,100 });
    FixedHost* host_raw = host.get();
    host_raw->set_clip_children(true);
    host_raw->set_presentation_translation({ 50,40 });
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 80,20,40,30 });
    ui::UiButton* trigger_raw = trigger.get();
    host_raw->add_child(std::move(trigger));
    window.add_child(std::move(host));
    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw);
    window.update(0.0);

    (void)window.on_ui_input_event(mouse_move(160,75));
    window.update(0.0);
    require(!tooltip->is_open(),
        "the clipped portion of a translated trigger must not activate its tooltip");

    (void)window.on_ui_input_event(mouse_move(140,75));
    window.update(0.0);
    require(tooltip->is_open(),
        "the visible portion of a translated trigger should activate its tooltip");
}

void test_non_modal_overlay_uses_local_occlusion()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto background = std::make_unique<FixedHost>(core::Rect{ 0,0,400,300 });
    auto covered = std::make_unique<ui::UiButton>(core::Rect{ 140,100,80,30 });
    auto outside = std::make_unique<ui::UiButton>(core::Rect{ 20,20,80,30 });
    ui::UiButton* covered_raw = covered.get();
    ui::UiButton* outside_raw = outside.get();
    background->add_child(std::move(covered));
    background->add_child(std::move(outside));
    window.add_child(std::move(background));

    auto overlay = std::make_unique<FixedHost>(core::Rect{ 0,0,160,100 });
    FixedHost* overlay_raw = overlay.get();
    auto overlay_trigger = std::make_unique<ui::UiButton>(core::Rect{ 20,20,80,30 });
    ui::UiButton* overlay_trigger_raw = overlay_trigger.get();
    overlay_raw->add_child(std::move(overlay_trigger));
    window.add_child(std::move(overlay));
    require(window.register_overlay(*overlay_raw,{
        .open = true,
        .modal = false,
        .close_on_cancel = false,
        .close_on_outside_click = false,
        .placement = ui::UiOverlayPlacement::Center,
        .transition = ui::UiOverlayTransition::None,
        .fallback_size = core::Vector2(160,100),
        .order = 900
    }),"non-modal overlay should register");

    ui::UiTooltip* tooltip = add_tooltip(window,*covered_raw);
    window.update(0.0);
    hover(window,*covered_raw);
    require(!tooltip->is_open(),
        "a non-modal overlay should block a background trigger inside its covered area");

    tooltip->bind_trigger(*outside_raw);
    hover(window,*outside_raw);
    require(tooltip->is_open(),
        "a non-modal overlay should preserve background tooltips outside its covered area");

    tooltip->bind_trigger(*overlay_trigger_raw);
    hover(window,*overlay_trigger_raw);
    require(tooltip->is_open(),
        "a trigger owned by the active overlay should remain eligible");

    tooltip->bind_trigger(*covered_raw);
    (void)window.on_ui_input_event(mouse_move(20,250));
    use_gamepad_focus(window);
    covered_raw->set_focused(true);
    tooltip->close();
    tooltip->update(0.0);
    require(!tooltip->is_open(),
        "focus must not activate a background tooltip intersecting a non-modal overlay");
}

void test_modal_overlay_blocks_background_tooltips()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto background = std::make_unique<FixedHost>(core::Rect{ 0,0,400,300 });
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 20,20,80,30 });
    ui::UiButton* trigger_raw = trigger.get();
    background->add_child(std::move(trigger));
    window.add_child(std::move(background));

    auto overlay = std::make_unique<FixedHost>(core::Rect{ 0,0,120,80 });
    FixedHost* overlay_raw = overlay.get();
    window.add_child(std::move(overlay));
    require(window.register_overlay(*overlay_raw,{
        .open = true,
        .modal = true,
        .close_on_cancel = false,
        .close_on_outside_click = false,
        .placement = ui::UiOverlayPlacement::Center,
        .transition = ui::UiOverlayTransition::None,
        .fallback_size = core::Vector2(120,80),
        .order = 900
    }),"modal overlay should register");

    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw);
    window.update(0.0);
    hover(window,*trigger_raw);
    require(!tooltip->is_open(),
        "a modal overlay should block background tooltips even outside its visual bounds");
}

void test_transient_popup_occlusion_remains_intact()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto host = std::make_unique<FixedHost>(core::Rect{ 0,0,300,200 });
    auto dropdown = std::make_unique<ui::UiDropdown>(core::Rect{ 0,0,120,40 });
    ui::UiDropdown* dropdown_raw = dropdown.get();
    dropdown_raw->set_options({
        ui::UiDropdownOption{ ui::ui_raw_text("One") },
        ui::UiDropdownOption{ ui::ui_raw_text("Two") }
    });
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 0,40,120,40 });
    ui::UiButton* trigger_raw = trigger.get();
    host->add_child(std::move(dropdown));
    host->add_child(std::move(trigger));
    window.add_child(std::move(host));
    dropdown_raw->register_with_window(window);
    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw);
    window.update(0.0);

    dropdown_raw->open();
    (void)window.on_ui_input_event(mouse_move(20,55));
    window.update(0.0);
    require(!tooltip->is_open(),
        "an active dropdown popup should continue to block an underlying hover tooltip");

    (void)window.on_ui_input_event(mouse_move(300,250));
    use_gamepad_focus(window);
    trigger_raw->set_focused(true);
    tooltip->update(0.0);
    require(!tooltip->is_open(),
        "an active dropdown popup should continue to block background focus tooltips");
}

void test_invalid_period_resets_hover_delay()
{
    ui::UiWindow window(core::Rect{ 0,0,400,300 });
    auto host = std::make_unique<FixedHost>(core::Rect{ 0,0,200,120 });
    FixedHost* host_raw = host.get();
    auto trigger = std::make_unique<ui::UiButton>(core::Rect{ 20,20,80,30 });
    ui::UiButton* trigger_raw = trigger.get();
    host_raw->add_child(std::move(trigger));
    window.add_child(std::move(host));
    ui::UiTooltip* tooltip = add_tooltip(window,*trigger_raw,1.0);
    window.update(0.0);

    hover(window,*trigger_raw,0.6);
    require(!tooltip->is_open(),"partial hover delay should not open the tooltip");
    host_raw->set_visible(false);
    window.update(0.1);
    host_raw->set_visible(true);
    window.update(0.5);
    require(!tooltip->is_open(),"hidden time must reset rather than preserve the hover delay");
    window.update(0.5);
    require(tooltip->is_open(),"a restored trigger should reopen after a full fresh delay");
}
}

int main()
{
    test_visible_trigger_and_ancestor_state();
    test_tab_switch_stops_stale_tooltip();
    test_clipping_and_presentation_translation();
    test_non_modal_overlay_uses_local_occlusion();
    test_modal_overlay_blocks_background_tooltips();
    test_transient_popup_occlusion_remains_intact();
    test_invalid_period_resets_hover_delay();
    std::cout << "ui tooltip visibility tests passed\n";
    return EXIT_SUCCESS;
}
