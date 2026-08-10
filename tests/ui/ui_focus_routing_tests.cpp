#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_chrome_container.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/input/ui_gamepad_scroll_synthesizer.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_slider.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>

namespace
{
using namespace elysia;
using elysia::tests::require;

ui::UiInputEvent navigation_event(ui::UiAction action,input::InputDevice device)
{
    return {
        .action = action,
        .type = ui::UiInputEventType::ActionPressed,
        .device = device
    };
}

ui::UiInputEvent confirm_event(ui::UiInputEventType type,input::InputDevice device)
{
    return {
        .action = ui::UiAction::Confirm,
        .type = type,
        .device = device
    };
}

ui::UiInputEvent wheel_event(int wheel_x,int wheel_y,input::InputDevice device)
{
    return {
        .type = ui::UiInputEventType::MouseWheel,
        .device = device,
        .wheel_x = wheel_x,
        .wheel_y = wheel_y
    };
}

class DisplayOnlyElement final : public ui::UiElement
{
public:
    explicit DisplayOnlyElement(const core::Vector2& desired_size)
        : UiElement(core::Rect{ 0,0,desired_size.x,desired_size.y }),_desired_size(desired_size) {}

    [[nodiscard]] core::Vector2 content_extent() const noexcept override { return _desired_size; }

private:
    core::Vector2 _desired_size;
};

struct TabFixture
{
    ui::UiTabContainer tabs{ core::Rect{ 0,0,540,300 } };
    ui::UiListContainer* pages[3]{};
    ui::UiButton* buttons[3][3]{};
    int selection_callbacks = 0;

    TabFixture()
    {
        for (std::size_t page_index = 0; page_index < 3; ++page_index)
        {
            auto page = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,500,220 });
            pages[page_index] = page.get();
            for (std::size_t button_index = 0; button_index < 3; ++button_index)
            {
                auto button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,160,36 });
                buttons[page_index][button_index] = button.get();
                page->add_back(std::move(button));
            }
            require(tabs.add_tab(ui::ui_raw_text("Tab " + std::to_string(page_index)),std::move(page)).added,
                "fixture tab should be added");
        }
        tabs.set_on_selection_changed([this](std::optional<std::size_t>) { ++selection_callbacks; });
        tabs.set_scope_focused(true);
        require(tabs.focus_first_available(),"tab container should focus its first tab");
    }
};

std::unique_ptr<ui::UiScrollContainer> scrolled_section_page(
    std::unique_ptr<ui::UiElement> first_content,
    ui::UiButton*& trailing_button)
{
    auto page = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,900,390 });
    page->set_scroll_axis(ui::UiScrollAxis::Vertical);
    auto page_list = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,870,500 });
    auto section = std::make_unique<ui::UiChromeContainer>(core::Rect{ 0,0,840,420 });
    auto section_body = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,820,360 });
    section_body->add_back(std::move(first_content));
    auto trailing = std::make_unique<ui::UiButton>(core::Rect{ 0,0,240,40 });
    trailing_button = trailing.get();
    section_body->add_back(std::move(trailing));
    section->set_body(std::move(section_body));
    page_list->add_back(std::move(section));
    page->set_content(std::move(page_list));
    return page;
}

struct WindowTabFixture
{
    ui::UiWindow window{ core::Rect{ 0,0,1120,616 } };
    ui::UiTabContainer* tabs = nullptr;

    explicit WindowTabFixture(std::unique_ptr<ui::UiElement> page)
    {
        auto owned_tabs = std::make_unique<ui::UiTabContainer>(core::Rect{ 0,0,1080,530 });
        tabs = owned_tabs.get();
        require(tabs->add_tab(ui::ui_raw_text("Page"),std::move(page)).added,"window fixture page should be added");
        window.add_child(std::move(owned_tabs));
        window.register_focus_scope(*tabs);
        require(window.focus_first_available_scope(),"window should focus the tab scope");
    }
};

void run_keyboard_gamepad_matrix(input::InputDevice device)
{
    TabFixture fixture;
    require(fixture.tabs.focused_index() == 0,"first tab should be focused after scope entry");
    require(fixture.tabs.selected_index() == 0,"first tab should be selected after construction");

    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateRight,device)),
        "tab bar should consume right navigation");
    require(fixture.tabs.focused_index() == 1 && fixture.tabs.selected_index() == 0,
        "tab focus and selection must remain independent");

    fixture.tabs.on_ui_input_event(confirm_event(ui::UiInputEventType::ActionPressed,device));
    fixture.tabs.on_ui_input_event(confirm_event(ui::UiInputEventType::ActionReleased,device));
    require(fixture.tabs.focused_index() == 1 && fixture.tabs.selected_index() == 1,
        "confirm should select the focused tab");
    require(fixture.selection_callbacks == 1,"confirm should emit one selection callback");

    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "down from the tab bar should enter the selected page");
    require(fixture.pages[1]->focused_target() == fixture.buttons[1][0],
        "page entry should focus the first available control");
    require(fixture.tabs.focused_target() == fixture.buttons[1][0],
        "tab container should mirror the page's focused leaf");

    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "page scope should consume navigation to its second control");
    require(fixture.pages[1]->focused_target() == fixture.buttons[1][1],
        "second down should move beyond the first page control");
    require(fixture.tabs.focused_target() == fixture.buttons[1][1],
        "nested focus movement should propagate back to the tab container");

    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "page scope should consume navigation to its third control");
    require(fixture.pages[1]->focused_target() == fixture.buttons[1][2],
        "third down should reach the last page control");

    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateUp,device)),
        "up inside the page should be consumed by the page scope first");
    require(fixture.pages[1]->focused_target() == fixture.buttons[1][1],
        "up inside the page should move to the previous control");
    fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateUp,device));
    require(fixture.pages[1]->focused_target() == fixture.buttons[1][0],
        "page should return to its first control before leaving the page");
    require(fixture.tabs.on_ui_input_event(navigation_event(ui::UiAction::NavigateUp,device)),
        "up at the page boundary should return to the tab bar");
    require(fixture.tabs.focused_index() == 1,"returning to the tab bar should restore its focused tab");
}

void run_real_nested_page_matrix(input::InputDevice device)
{
    auto first_button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,240,40 });
    ui::UiButton* first = first_button.get();
    ui::UiButton* second = nullptr;
    WindowTabFixture overlays(scrolled_section_page(std::move(first_button),second));

    require(overlays.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "window should route down through tab, scroll and chrome into the section body");
    require(overlays.tabs->focused_target() == first,"nested overlay-style page should focus its first button");
    overlays.window.update(1.0 / 60.0);
    require(overlays.tabs->focused_target() == first,"frame synchronization must preserve the nested focused leaf");
    require(overlays.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "nested section body should consume its second down");
    require(overlays.tabs->focused_target() == second,"nested overlay-style page should reach its second button");
    overlays.window.update(1.0 / 60.0);
    require(overlays.tabs->focused_target() == second,"chrome update must not reset body focus to its first control");

    auto grid = std::make_unique<ui::UiGridContainer>(core::Rect{ 0,0,560,170 });
    grid->set_column_count(3);
    ui::UiButton* grid_buttons[6]{};
    for (std::size_t index = 0; index < 6; ++index)
    {
        auto button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,160,40 });
        grid_buttons[index] = button.get();
        grid->add_child(std::move(button));
    }
    ui::UiButton* after_grid = nullptr;
    WindowTabFixture containers(scrolled_section_page(std::move(grid),after_grid));

    require(containers.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "window should route down into a grid nested below scroll and chrome");
    require(containers.tabs->focused_target() == grid_buttons[0],"container-style page should enter the first grid cell");
    require(containers.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "grid should consume vertical navigation inside the page");
    require(containers.tabs->focused_target() == grid_buttons[3],"grid down should reach the next row");
    containers.window.update(1.0 / 60.0);
    require(containers.tabs->focused_target() == grid_buttons[3],"frame synchronization must preserve nested grid focus");
    require(containers.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device)),
        "grid boundary should propagate to the next section-body item");
    require(containers.tabs->focused_target() == after_grid,"grid boundary should enter the following control");
}

void run_slider_focus_matrix(input::InputDevice device)
{
    ui::UiWindow window{ core::Rect{ 0,0,640,480 } };
    auto list = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,400,300 });
    ui::UiListContainer* list_raw = list.get();
    auto before = std::make_unique<ui::UiButton>(core::Rect{ 0,0,200,40 });
    ui::UiButton* before_raw = before.get();
    auto slider = std::make_unique<ui::UiSlider>(core::Rect{ 0,0,240,40 });
    ui::UiSlider* slider_raw = slider.get();
    slider->set_step(0.1f);
    slider->set_value(0.5f);
    auto after = std::make_unique<ui::UiButton>(core::Rect{ 0,0,200,40 });
    ui::UiButton* after_raw = after.get();
    list->add_back(std::move(before));
    list->add_back(std::move(slider));
    list->add_back(std::move(after));
    window.add_child(std::move(list));
    window.register_focus_scope(*list_raw);
    require(window.focus_first_available_scope() && list_raw->focused_target() == before_raw,
        "slider focus fixture should start on the preceding button");

    window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device));
    require(list_raw->focused_target() == slider_raw && !slider_raw->is_adjusting(),
        "normal navigation should enter the focused slider without beginning adjustment");
    window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device));
    require(list_raw->focused_target() == after_raw,
        "unconfirmed slider must yield its primary container direction");

    window.on_ui_input_event(navigation_event(ui::UiAction::NavigateUp,device));
    require(list_raw->focused_target() == slider_raw,"up should return focus to the slider");
    window.on_ui_input_event(confirm_event(ui::UiInputEventType::ActionPressed,device));
    require(slider_raw->is_adjusting(),"confirm should begin slider adjustment inside a list");
    const float before_adjustment = slider_raw->value();
    window.on_ui_input_event(navigation_event(ui::UiAction::NavigateRight,device));
    require(slider_raw->value() > before_adjustment,"adjusting slider should consume and apply its horizontal axis");
    window.on_ui_input_event(confirm_event(ui::UiInputEventType::ActionPressed,device));
    require(!slider_raw->is_adjusting(),"second confirm should exit slider adjustment");
    window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device));
    require(list_raw->focused_target() == after_raw,"exited slider adjustment must restore container navigation");

    auto nested_slider = std::make_unique<ui::UiSlider>(core::Rect{ 0,0,240,40 });
    ui::UiSlider* nested_slider_raw = nested_slider.get();
    nested_slider->set_step(0.1f);
    nested_slider->set_value(0.5f);
    ui::UiButton* nested_after = nullptr;
    WindowTabFixture nested(scrolled_section_page(std::move(nested_slider),nested_after));
    nested.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device));
    require(nested.tabs->focused_target() == nested_slider_raw,"nested scroll/chrome page should enter its slider");
    nested.window.on_ui_input_event(confirm_event(ui::UiInputEventType::ActionPressed,device));
    require(nested_slider_raw->is_adjusting(),"nested slider should enter adjustment mode");
    nested.window.on_ui_input_event(navigation_event(ui::UiAction::NavigateDown,device));
    require(nested.tabs->focused_target() == nested_after && !nested_slider_raw->is_adjusting(),
        "secondary navigation must leave nested slider and clear adjustment mode");
}

void test_gamepad_scroll_synthesizer_axes()
{
    elysia::ui::UiGamepadScrollSynthesizer synthesizer;
    elysia::input::RawInputFrame frame{};
    frame.active_device = elysia::input::InputDevice::Gamepad;
    frame.state.set_axis(elysia::input::RawInputAxis::GamepadLeftX,1.0f);
    frame.state.set_axis(elysia::input::RawInputAxis::GamepadLeftY,1.0f);
    require(!synthesizer.synthesize(frame).has_value(),"first diagonal stick sample should accumulate independently");
    const auto diagonal = synthesizer.synthesize(frame);
    require(diagonal && diagonal->wheel_x == -1 && diagonal->wheel_y == -1,
        "diagonal stick input should synthesize independent horizontal and vertical wheel steps");

    frame.state.set_axis(elysia::input::RawInputAxis::GamepadLeftX,0.1f);
    frame.state.set_axis(elysia::input::RawInputAxis::GamepadLeftY,0.0f);
    require(!synthesizer.synthesize(frame).has_value(),"deadzone input should reset both idle axis accumulators");

    frame.state.set_axis(elysia::input::RawInputAxis::GamepadLeftX,-1.0f);
    require(!synthesizer.synthesize(frame).has_value(),"fresh horizontal input should not inherit pre-deadzone accumulation");
    const auto horizontal = synthesizer.synthesize(frame);
    require(horizontal && horizontal->wheel_x == 1 && horizontal->wheel_y == 0,
        "horizontal stick input should emit only horizontal wheel steps");

    frame.device_switched_this_frame = true;
    require(!synthesizer.synthesize(frame).has_value(),"device switches should clear pending scroll accumulation");
    frame.device_switched_this_frame = false;
    require(!synthesizer.synthesize(frame).has_value(),"post-switch input should restart accumulation from zero");
}

void test_passive_scroll_target_routing()
{
    ui::UiWindow window{ core::Rect{ 0,0,640,480 } };
    auto tabs_owned = std::make_unique<ui::UiTabContainer>(core::Rect{ 0,0,600,420 });
    ui::UiTabContainer* tabs = tabs_owned.get();
    auto page = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,560,330 });
    ui::UiScrollContainer* page_scroll = page.get();
    page->set_scroll_axis(ui::UiScrollAxis::Vertical);
    page->set_content(std::make_unique<DisplayOnlyElement>(core::Vector2{ 540,960 }));
    require(tabs->add_tab(ui::ui_raw_text("Display"),std::move(page)).added,
        "display-only tab page should be added");
    auto alternate_page = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,560,330 });
    ui::UiScrollContainer* alternate_scroll = alternate_page.get();
    alternate_page->set_scroll_axis(ui::UiScrollAxis::Vertical);
    alternate_page->set_content(std::make_unique<DisplayOnlyElement>(core::Vector2{ 540,960 }));
    require(tabs->add_tab(ui::ui_raw_text("Alternate"),std::move(alternate_page)).added,
        "alternate display-only tab page should be added");
    window.add_child(std::move(tabs_owned));
    window.register_focus_scope(*tabs);
    require(window.focus_first_available_scope(),"display-only tab should receive window focus");
    window.update(1.0 / 60.0);

    const float initial_y = page_scroll->scroll_offset_y();
    require(window.on_ui_input_event(wheel_event(0,-1,input::InputDevice::Gamepad)),
        "gamepad wheel should resolve a passive scroll target without a focused page leaf");
    require(page_scroll->scroll_offset_y() > initial_y,"passive gamepad scrolling should move the display-only page");
    require(window.gamepad_scroll_target() == page_scroll,"window should retain the resolved passive scroll target");
    require(tabs->selected_index() == 0,"passive scrolling must not change tab selection");

    const float after_wheel = page_scroll->scroll_offset_y();
    require(window.on_ui_input_event(navigation_event(ui::UiAction::PageDown,input::InputDevice::Keyboard)),
        "PageDown should route to the passive scroll target after focused scope declines it");
    require(page_scroll->scroll_offset_y() > after_wheel,"PageDown should advance the display-only scroll page");
    require(window.on_ui_input_event(navigation_event(ui::UiAction::Home,input::InputDevice::Keyboard)),
        "Home should route to the passive scroll target when no leaf consumes it");
    require(page_scroll->scroll_offset_y() == 0.0f,"Home should return passive scrolling to the beginning");
    tabs->set_selected_index(1);
    window.update(1.0 / 60.0);
    require(window.gamepad_scroll_target() == nullptr,"switching tabs should invalidate a hidden page scroll target");
    window.on_ui_input_event(wheel_event(0,-1,input::InputDevice::Gamepad));
    require(alternate_scroll->scroll_offset_y() > 0.0f && window.gamepad_scroll_target() == alternate_scroll,
        "passive scrolling after a tab switch should resolve the newly visible page");

    ui::UiWindow horizontal_window{ core::Rect{ 0,0,500,260 } };
    auto horizontal_owned = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,440,180 });
    ui::UiScrollContainer* horizontal = horizontal_owned.get();
    horizontal_owned->set_scroll_axis(ui::UiScrollAxis::Horizontal);
    horizontal_owned->set_content(std::make_unique<DisplayOnlyElement>(core::Vector2{ 960,140 }));
    horizontal_window.add_child(std::move(horizontal_owned));
    horizontal_window.update(1.0 / 60.0);
    require(horizontal_window.on_ui_input_event(wheel_event(-1,0,input::InputDevice::Gamepad)),
        "horizontal gamepad wheel should resolve a passive horizontal scroll target");
    require(horizontal->scroll_offset_x() > 0.0f,"horizontal wheel input should move the horizontal scroll container");
    require(horizontal_window.on_ui_input_event(navigation_event(ui::UiAction::End,input::InputDevice::Keyboard)),
        "End should route to an unfocused horizontal passive target");
    require(horizontal->scroll_offset_x() == horizontal->max_scroll_offset().x,
        "End should move passive horizontal scrolling to its far edge");

    ui::UiWindow pointer_window{ core::Rect{ 0,0,760,280 } };
    auto first_owned = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,320,220 });
    ui::UiScrollContainer* first = first_owned.get();
    first_owned->set_scroll_axis(ui::UiScrollAxis::Vertical);
    first_owned->set_content(std::make_unique<DisplayOnlyElement>(core::Vector2{ 300,720 }));
    ui::UiLayoutChildOptions first_options{};
    first_options._margin.left = 10.0f;
    first_options._margin.top = 10.0f;
    pointer_window.add_child(std::move(first_owned),first_options);

    auto second_owned = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,320,220 });
    ui::UiScrollContainer* second = second_owned.get();
    second_owned->set_scroll_axis(ui::UiScrollAxis::Vertical);
    second_owned->set_content(std::make_unique<DisplayOnlyElement>(core::Vector2{ 300,720 }));
    ui::UiLayoutChildOptions second_options{};
    second_options._margin.left = 400.0f;
    second_options._margin.top = 10.0f;
    pointer_window.add_child(std::move(second_owned),second_options);
    pointer_window.update(1.0 / 60.0);
    pointer_window.on_ui_input_event({
        .type = ui::UiInputEventType::PointerPressed,
        .device = input::InputDevice::Mouse,
        .control = input::RawInputControl::MouseLeft,
        .mouse_x = 450,
        .mouse_y = 50
    });
    require(pointer_window.gamepad_scroll_target() == second,"pointer-used scroll should win over deepest fallback selection");
    pointer_window.on_ui_input_event(wheel_event(0,-1,input::InputDevice::Gamepad));
    require(second->scroll_offset_y() > 0.0f && first->scroll_offset_y() == 0.0f,
        "passive wheel input should use the pointer-promoted scroll target");
    second->set_visible(false);
    pointer_window.update(1.0 / 60.0);
    require(pointer_window.gamepad_scroll_target() == nullptr,"hidden passive targets must be pruned before later input");
    pointer_window.on_ui_input_event(wheel_event(0,-1,input::InputDevice::Gamepad));
    require(first->scroll_offset_y() > 0.0f,"next passive scroll should fall back to another visible container after target invalidation");
}

}

int main()
{
    test_gamepad_scroll_synthesizer_axes();
    run_keyboard_gamepad_matrix(input::InputDevice::Keyboard);
    run_keyboard_gamepad_matrix(input::InputDevice::Gamepad);
    run_real_nested_page_matrix(input::InputDevice::Keyboard);
    run_real_nested_page_matrix(input::InputDevice::Gamepad);
    run_slider_focus_matrix(input::InputDevice::Keyboard);
    run_slider_focus_matrix(input::InputDevice::Gamepad);
    test_passive_scroll_target_routing();
    std::cout << "ui focus routing tests passed\n";
    return EXIT_SUCCESS;
}
