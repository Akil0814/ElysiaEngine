#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
using elysia::tests::require;

void test_empty_focus_scopes()
{
    elysia::ui::UiListContainer list;
    elysia::ui::UiPanel panel;
    require(!list.focus_first_available(),"empty list should not acquire focus");
    require(!panel.focus_first_available(),"empty panel should not acquire focus");
}

void test_nested_focus_and_dropdown_navigation()
{
    auto inner = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 0,0,200,40 });
    inner->add_back(std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 }));
    elysia::ui::UiListContainer outer(elysia::core::Rect{ 0,0,240,80 });
    outer.add_back(std::move(inner));
    outer.set_scope_focused(true);
    require(outer.focus_first_available(),"nested list should find delegated button focus");
    require(outer.focused_target() != nullptr,"nested list should expose cached focused target");

    auto scroll_content = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 0,0,200,80 });
    scroll_content->add_back(std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 }));
    elysia::ui::UiScrollContainer scroll(elysia::core::Rect{ 0,0,200,40 });
    scroll.set_content(std::move(scroll_content));
    scroll.set_scope_focused(true);
    require(scroll.focus_first_available(),"scroll container should delegate focus to content");

    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
    auto dropdown = std::make_unique<elysia::ui::UiDropdown>(
        elysia::core::Rect{ 20,20,200,40 });
    auto* raw_dropdown = dropdown.get();
    raw_dropdown->set_options({
        { elysia::ui::ui_raw_text("one") },
        { elysia::ui::ui_raw_text("two") }
    });
    raw_dropdown->register_with_window(window);
    window.add_child(std::move(dropdown));
    raw_dropdown->open();
    require(raw_dropdown->is_open(),"owned dropdown should open");
    window.on_ui_input_event(elysia::ui::UiInputEvent{
        .action = elysia::ui::UiAction::NavigateDown,
        .type = elysia::ui::UiInputEventType::ActionPressed
    });
    window.on_ui_input_event(elysia::ui::UiInputEvent{
        .action = elysia::ui::UiAction::Confirm,
        .type = elysia::ui::UiInputEventType::ActionPressed
    });
    window.on_ui_input_event(elysia::ui::UiInputEvent{
        .action = elysia::ui::UiAction::Confirm,
        .type = elysia::ui::UiInputEventType::ActionReleased
    });
    require(raw_dropdown->selected_index() == 1,"dropdown navigation should select focused option");
    require(!raw_dropdown->is_open(),"dropdown should close after confirmation");
}

void test_dropdown_option_rebuild_repairs_cached_focus()
{
    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
    elysia::ui::UiDropdown dropdown(elysia::core::Rect{ 20,20,200,40 });
    dropdown.set_options({
        { elysia::ui::ui_raw_text("one") },
        { elysia::ui::ui_raw_text("two") }
    });
    dropdown.register_with_window(window);
    dropdown.open();
    require(dropdown.is_open(),"dropdown should open before rebuilding its options");
    dropdown.close();

    dropdown.set_options({
        { elysia::ui::ui_raw_text("three") },
        { elysia::ui::ui_raw_text("four") }
    });
    dropdown.unregister_from_window();
    dropdown.register_with_window(window);
    dropdown.open();

    require(dropdown.is_open(),"dropdown should reopen after rebuilding its option focus tree");
    require(dropdown.selected_index() == 0,"rebuilt dropdown should retain a valid selection");
}

elysia::ui::UiInputEvent navigation_event(elysia::ui::UiAction action)
{
    return elysia::ui::UiInputEvent{
        .action = action,
        .type = elysia::ui::UiInputEventType::ActionPressed,
        .device = elysia::input::InputDevice::Gamepad
    };
}

void test_deep_nested_focus_propagation()
{
    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,480 });
    auto scroll = std::make_unique<elysia::ui::UiScrollContainer>(elysia::core::Rect{ 0,0,320,240 });
    auto* scroll_raw = scroll.get();
    auto list = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 0,0,300,220 });
    auto* list_raw = list.get();
    auto panel = std::make_unique<elysia::ui::UiPanel>(elysia::core::Rect{ 0,0,280,180 });
    auto* panel_raw = panel.get();
    auto grid = std::make_unique<elysia::ui::UiGridContainer>(elysia::core::Rect{ 0,0,260,160 });
    auto* grid_raw = grid.get();
    grid->set_column_count(2);
    auto first = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 });
    auto second = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    require(grid->add_child(std::move(first)) == first_raw,"grid add_child should return the adopted child");
    require(grid->add_child(std::move(second)) == second_raw,"grid add_child should return the adopted child");
    require(panel->add_child(std::move(grid),elysia::ui::UiPanelInsertDirection::Down) == grid_raw,
        "panel add_child should return the adopted child");
    list->add_back(std::move(panel));
    scroll->set_content(std::move(list));
    window.add_child(std::move(scroll));
    window.register_focus_scope(*scroll_raw);

    require(window.focus_first_available_scope(),"window should focus a deeply nested scope");
    require(window.focused_scope() == scroll_raw,"window should retain the registered outer scope");
    require(scroll_raw->is_scope_focused(),"scroll scope should receive window focus");
    require(list_raw->is_scope_focused(),"nested list should receive delegated focus");
    require(panel_raw->is_scope_focused(),"nested panel should receive delegated focus");
    require(grid_raw->is_scope_focused(),"nested grid should receive delegated focus");
    require(scroll_raw->focused_target() == first_raw,"outer scope should expose the focused leaf control");
    require(list_raw->focused_target() == first_raw,"list should expose the same focused leaf control");
    require(panel_raw->focused_target() == first_raw,"panel should expose the same focused leaf control");
    require(grid_raw->focused_target() == first_raw,"grid should initially focus its first button");
    require(first_raw->is_focused() && !second_raw->is_focused(),"only one leaf control should own focus");

    window.on_ui_input_event(navigation_event(elysia::ui::UiAction::NavigateRight));
    require(grid_raw->focused_target() == second_raw,"grid navigation should move focus to the next leaf");
    require(scroll_raw->focused_target() == second_raw,"outer delegated target should follow inner navigation");
    require(!first_raw->is_focused() && second_raw->is_focused(),"leaf focus visuals should remain exclusive");
}

void test_nested_focus_boundary_navigation()
{
    elysia::ui::UiWindow window(elysia::core::Rect{ 0,0,640,240 });
    auto left = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 0,0,240,120 });
    auto right = std::make_unique<elysia::ui::UiListContainer>(elysia::core::Rect{ 300,0,240,120 });
    auto* left_raw = left.get();
    auto* right_raw = right.get();
    auto left_button = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,120,40 });
    auto right_button = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,120,40 });
    auto* left_button_raw = left_button.get();
    auto* right_button_raw = right_button.get();
    left->add_back(std::move(left_button));
    right->add_back(std::move(right_button));
    window.add_child(std::move(left));
    window.add_child(std::move(right));
    window.register_focus_scope(*left_raw,elysia::ui::UiFocusScopeNeighbors{ nullptr,nullptr,nullptr,right_raw });
    window.register_focus_scope(*right_raw,elysia::ui::UiFocusScopeNeighbors{ nullptr,nullptr,left_raw,nullptr });
    window.set_focused_scope(left_raw);

    require(window.focused_scope() == left_raw && left_button_raw->is_focused(),
        "left scope should own the initial leaf focus");
    window.on_ui_input_event(navigation_event(elysia::ui::UiAction::NavigateRight));
    require(window.focused_scope() == right_raw,"unconsumed boundary navigation should move to the neighbor scope");
    require(!left_raw->is_scope_focused() && right_raw->is_scope_focused(),
        "scope focus should transfer exclusively at the boundary");
    require(!left_button_raw->is_focused() && right_button_raw->is_focused(),
        "leaf focus should transfer with its owning scope");
    window.on_ui_input_event(navigation_event(elysia::ui::UiAction::NavigateLeft));
    require(window.focused_scope() == left_raw && left_button_raw->is_focused(),
        "reverse boundary navigation should restore the previous scope");
}

void test_nested_focus_repair_after_visibility_and_removal()
{
    elysia::ui::UiListContainer list(elysia::core::Rect{ 0,0,240,160 });
    auto first = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,120,40 });
    auto second = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,120,40 });
    auto third = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,120,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    auto* third_raw = third.get();
    list.add_back(std::move(first));
    list.add_back(std::move(second));
    list.add_back(std::move(third));
    list.set_scope_focused(true);
    require(list.focus_first_available() && list.focused_target() == first_raw,
        "repair test should begin on the first control");

    first_raw->destroy();
    list.update(0.0);
    require(list.focused_target() == second_raw && second_raw->is_focused(),
        "destroying the focused control should repair focus to the next live control");
    second_raw->set_visible(false);
    list.update(0.0);
    require(list.focused_target() == third_raw && third_raw->is_focused(),
        "hiding the focused control should repair focus to the next visible control");
    third_raw->set_enabled(false);
    list.update(0.0);
    require(list.focused_target() == nullptr && !third_raw->is_focused(),
        "disabling the final focusable control should clear stale focus");

    third_raw->set_enabled(true);
    require(list.focus_first_available() && list.focused_target() == third_raw,
        "focus should recover when a valid control becomes available again");
    list.clear_children();
    list.update(0.0);
    require(list.focused_target() == nullptr && !list.has_focusable_target(),
        "clearing the focused subtree should clear every cached target");
}
}

int main()
{
    test_empty_focus_scopes();
    test_nested_focus_and_dropdown_navigation();
    test_dropdown_option_rebuild_repairs_cached_focus();
    test_deep_nested_focus_propagation();
    test_nested_focus_boundary_navigation();
    test_nested_focus_repair_after_visibility_and_removal();
    std::cout << "ui focus lifecycle tests passed\n";
    return EXIT_SUCCESS;
}
