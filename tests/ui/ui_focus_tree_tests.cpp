#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_chrome_container.h"
#include "engine/ui/containers/ui_grid_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_checkbox.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
using elysia::tests::require;

elysia::ui::UiInputEvent navigate(elysia::ui::UiAction action)
{
    return {
        .action = action,
        .type = elysia::ui::UiInputEventType::ActionPressed,
        .device = elysia::input::InputDevice::Gamepad
    };
}

void test_nested_boundary_restores_preferred_leaf()
{
    using namespace elysia;
    ui::UiListContainer outer(core::Rect{ 0,0,320,240 });
    auto grid = std::make_unique<ui::UiGridContainer>(core::Rect{ 0,0,300,120 });
    grid->set_column_count(2);
    auto* grid_raw = grid.get();
    auto first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    grid->add_child(std::move(first));
    grid->add_child(std::move(second));
    auto trailing = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* trailing_raw = trailing.get();
    outer.add_back(std::move(grid));
    outer.add_back(std::move(trailing));
    outer.set_scope_focused(true);

    require(outer.focus_first_available(),"outer list should enter its nested grid");
    require(outer.focused_target() == first_raw,"nested grid should initially select its first leaf");
    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateRight)),"inner grid navigation should be handled");
    require(grid_raw->focused_target() == second_raw,"inner navigation should update the nested scope target");

    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateDown)),"boundary navigation should leave the nested region");
    require(outer.focused_target() == trailing_raw && trailing_raw->is_focused(),
        "outer list should enter its trailing direct-control region");
    require(!grid_raw->is_scope_focused(),"inactive nested scope should lose scope focus");

    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateUp)),"reverse boundary navigation should re-enter nested region");
    require(outer.focused_target() == second_raw && grid_raw->focused_target() == second_raw,
        "re-entering a nested region should restore its last valid leaf");
    require(second_raw->is_focused() && !first_raw->is_focused(),"nested leaf focus visuals should remain exclusive");
}

void test_window_repairs_focus_across_invalid_scope()
{
    using namespace elysia;
    ui::UiWindow window(core::Rect{ 0,0,640,240 });
    auto left = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,240,120 });
    auto right = std::make_unique<ui::UiListContainer>(core::Rect{ 300,0,240,120 });
    auto* left_raw = left.get();
    auto* right_raw = right.get();
    auto left_first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto left_second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto right_button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* left_first_raw = left_first.get();
    auto* left_second_raw = left_second.get();
    auto* right_button_raw = right_button.get();
    left->add_back(std::move(left_first));
    left->add_back(std::move(left_second));
    right->add_back(std::move(right_button));
    window.add_child(std::move(left));
    window.add_child(std::move(right));
    window.register_focus_scope(*left_raw);
    window.register_focus_scope(*right_raw);

    require(window.focus_first_available_scope(),"window should focus the first registered usable scope");
    left_first_raw->set_visible(false);
    window.update(0.0);
    require(left_raw->focused_target() == left_second_raw && left_second_raw->is_focused(),
        "a scope should repair focus to its next usable direct child");

    left_second_raw->set_enabled(false);
    window.update(0.0);
    require(window.focused_scope() == right_raw,"window should fall back when its focused scope loses every target");
    require(right_raw->is_scope_focused() && right_button_raw->is_focused(),
        "fallback scope and leaf should receive focus together");
    require(!left_first_raw->is_focused() && !left_second_raw->is_focused(),
        "invalidated scope must not retain stale leaf visuals");
}

void test_scroll_scope_suppresses_and_restores_nested_focus()
{
    using namespace elysia;
    ui::UiScrollContainer scroll(core::Rect{ 0,0,200,80 });
    auto content = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,200,240 });
    auto button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* button_raw = button.get();
    content->add_back(std::move(button));
    scroll.set_content(std::move(content));
    scroll.set_scope_focused(true);

    require(scroll.focus_first_available() && scroll.focused_target() == button_raw,
        "scroll scope should delegate initial focus to content");
    require(button_raw->is_focused(),"delegated content leaf should render focused");
    require(scroll.clear_focus_for_gamepad_scroll(),"scrolling should temporarily clear an existing nested focus target");
    require(scroll.focused_target() == nullptr && !button_raw->is_focused(),
        "temporary scroll focus suppression should clear leaf focus visuals");
    require(scroll.restore_focus_after_gamepad_scroll(),"scroll scope should restore focus after scrolling ends");
    require(scroll.focused_target() == button_raw && button_raw->is_focused(),
        "restoration should recover the previous valid nested focus target");
}

void test_list_panel_grid_three_level_focus_chain()
{
    using namespace elysia;
    ui::UiListContainer list(core::Rect{ 0,0,360,280 });
    auto panel = std::make_unique<ui::UiPanel>(core::Rect{ 0,0,340,180 });
    auto* panel_raw = panel.get();
    auto grid = std::make_unique<ui::UiGridContainer>(core::Rect{ 0,0,320,120 });
    auto* grid_raw = grid.get();
    grid->set_column_count(2);
    auto first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    grid->add_child(std::move(first));
    grid->add_child(std::move(second));
    panel->add_child(std::move(grid),ui::UiPanelInsertDirection::Down);
    auto after = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* after_raw = after.get();
    list.add_back(std::move(panel));
    list.add_back(std::move(after));
    list.set_scope_focused(true);

    require(list.focus_first_available() && list.focused_target() == first_raw,
        "list-panel-grid chain should enter the first grid leaf");
    require(panel_raw->is_scope_focused() && grid_raw->is_scope_focused(),
        "every delegated scope in a three-level chain should receive focus");
    require(list.on_ui_input_event(navigate(ui::UiAction::NavigateRight)),"grid should consume horizontal navigation");
    require(grid_raw->focused_target() == second_raw && list.focused_target() == second_raw,
        "inner grid movement should propagate through panel and list");
    require(list.on_ui_input_event(navigate(ui::UiAction::NavigateDown)),
        "a grid boundary should propagate through panel to the list sibling");
    require(list.focused_target() == after_raw && after_raw->is_focused(),
        "outer list should receive an unhandled nested boundary navigation");
}

void test_scroll_list_panel_three_level_focus_repair()
{
    using namespace elysia;
    ui::UiScrollContainer scroll(core::Rect{ 0,0,260,100 });
    auto list = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,240,300 });
    auto* list_raw = list.get();
    auto panel = std::make_unique<ui::UiPanel>(core::Rect{ 0,0,220,160 });
    auto* panel_raw = panel.get();
    auto first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    panel->add_child(std::move(first),ui::UiPanelInsertDirection::Down);
    panel->add_child(std::move(second),ui::UiPanelInsertDirection::Down);
    list->add_back(std::move(panel));
    scroll.set_content(std::move(list));
    scroll.set_scope_focused(true);

    require(scroll.focus_first_available() && scroll.focused_target() == first_raw,
        "scroll-list-panel chain should enter the first panel leaf");
    require(list_raw->is_scope_focused() && panel_raw->is_scope_focused(),
        "scroll focus should propagate to every nested delegated scope");
    require(scroll.on_ui_input_event(navigate(ui::UiAction::NavigateDown)),"panel should consume down navigation");
    require(scroll.focused_target() == second_raw,"scroll should mirror a nested panel navigation result");
    second_raw->set_visible(false);
    scroll.update(0.0);
    require(scroll.focused_target() == first_raw && first_raw->is_focused(),
        "hiding a deep focused leaf should repair focus through scroll, list, and panel");
}

void test_tab_chrome_scroll_list_four_level_focus_chain()
{
    using namespace elysia;
    ui::UiTabContainer tabs(core::Rect{ 0,0,600,360 });
    auto chrome = std::make_unique<ui::UiChromeContainer>(core::Rect{ 0,0,560,300 });
    auto* chrome_raw = chrome.get();
    auto scroll = std::make_unique<ui::UiScrollContainer>(core::Rect{ 0,0,540,230 });
    auto* scroll_raw = scroll.get();
    auto list = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,520,360 });
    auto* list_raw = list.get();
    auto first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,120,40 });
    auto second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,120,40 });
    auto* first_raw = first.get();
    auto* second_raw = second.get();
    list->add_back(std::move(first));
    list->add_back(std::move(second));
    scroll->set_content(std::move(list));
    chrome->set_body(std::move(scroll));
    require(tabs.add_tab(ui::ui_raw_text("Nested"),std::move(chrome)).added,
        "four-level fixture should accept its tab page");
    tabs.set_scope_focused(true);

    require(tabs.focus_first_available(),"tab container should focus its tab bar first");
    require(tabs.on_ui_input_event(navigate(ui::UiAction::NavigateDown)),
        "tab boundary navigation should enter the chrome-scroll-list page");
    require(tabs.focused_target() == first_raw,"four-level chain should reach the first list leaf");
    require(chrome_raw->focused_target() == first_raw && scroll_raw->focused_target() == first_raw
            && list_raw->focused_target() == first_raw,
        "each nested container should mirror the active leaf through the four-level chain");
    require(tabs.on_ui_input_event(navigate(ui::UiAction::NavigateDown)),"nested list should consume down navigation");
    require(tabs.focused_target() == second_raw && second_raw->is_focused(),
        "deep navigation should propagate the focused leaf back to the tab container");
}

// Exercises: Container(Component + Component + Container(Component + Container(Component) + Component)).
void test_asymmetric_component_container_focus_tree()
{
    using namespace elysia;
    ui::UiListContainer outer(core::Rect{ 0,0,400,420 });
    auto outer_first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,140,40 });
    auto outer_second = std::make_unique<ui::UiCheckbox>(core::Rect{ 0,0,140,40 });
    auto* outer_first_raw = outer_first.get();
    auto* outer_second_raw = outer_second.get();
    outer.add_back(std::move(outer_first));
    outer.add_back(std::move(outer_second));

    auto panel = std::make_unique<ui::UiPanel>(core::Rect{ 0,0,360,260 });
    auto* panel_raw = panel.get();
    auto panel_first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,140,40 });
    auto* panel_first_raw = panel_first.get();
    auto nested_list = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,300,100 });
    auto* nested_list_raw = nested_list.get();
    auto deep_button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,140,40 });
    auto* deep_button_raw = deep_button.get();
    nested_list->add_back(std::move(deep_button));
    auto panel_last = std::make_unique<ui::UiButton>(core::Rect{ 0,0,140,40 });
    auto* panel_last_raw = panel_last.get();
    panel->add_child(std::move(panel_first),ui::UiPanelInsertDirection::Down);
    panel->add_child(std::move(nested_list),ui::UiPanelInsertDirection::Down);
    panel->add_child(std::move(panel_last),ui::UiPanelInsertDirection::Down);
    outer.add_back(std::move(panel));
    outer.set_scope_focused(true);

    require(outer.focus_first_available() && outer.focused_target() == outer_first_raw,
        "compound tree should begin at the first outer component");
    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateDown))
            && outer.focused_target() == outer_second_raw,
        "outer container should navigate between its direct components before entering the nested container");
    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateDown))
            && outer.focused_target() == panel_first_raw,
        "outer container should enter the first component in its nested panel");
    require(panel_raw->is_scope_focused(),"panel should become the active delegated scope on entry");
    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateDown))
            && outer.focused_target() == deep_button_raw,
        "panel should enter its nested list component at the next boundary");
    require(nested_list_raw->is_scope_focused() && nested_list_raw->focused_target() == deep_button_raw,
        "deepest nested container should own the active component");
    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateDown))
            && outer.focused_target() == panel_last_raw,
        "leaving the nested list should resume panel navigation at its trailing component");

    require(outer.on_ui_input_event(navigate(ui::UiAction::NavigateUp))
            && outer.focused_target() == deep_button_raw,
        "reverse navigation should restore the nested list's retained component");
    deep_button_raw->set_enabled(false);
    outer.update(0.0);
    require(outer.focused_target() == outer_first_raw && outer_first_raw->is_focused(),
        "invalidating a deep component should rebuild focus from the outer scope's first viable region");
}
}

int main()
{
    test_nested_boundary_restores_preferred_leaf();
    test_window_repairs_focus_across_invalid_scope();
    test_scroll_scope_suppresses_and_restores_nested_focus();
    test_list_panel_grid_three_level_focus_chain();
    test_scroll_list_panel_three_level_focus_repair();
    test_tab_chrome_scroll_list_four_level_focus_chain();
    test_asymmetric_component_container_focus_tree();
    std::cout << "ui focus container tests passed\n";
    return EXIT_SUCCESS;
}
