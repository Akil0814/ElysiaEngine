#include "engine/ui/composites/ui_option_selector.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_chrome_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace
{
using namespace elysia;
using namespace elysia::ui;
using elysia::tests::require;

UiInputEvent action(UiAction value,input::InputDevice device = input::InputDevice::Keyboard,
    UiInputEventType type = UiInputEventType::ActionPressed)
{
    return {.action=value,.type=type,.device=device};
}
UiInputEvent pointer(UiInputEventType type,int x,int y)
{
    return {.type=type,.device=input::InputDevice::Mouse,.control=input::RawInputControl::MouseLeft,
        .mouse_x=x,.mouse_y=y};
}
std::vector<UiOptionSelectorOption> options()
{
    return {{ui_raw_text("one")},{ui_raw_text("locked"),false},{ui_raw_text("three")}};
}
void click(UiOptionSelector& selector,int x,int y)
{
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerPressed,x,y));
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerReleased,x,y));
}

void test_selection_contract()
{
    UiOptionSelector selector;
    require(selector.options().empty() && !selector.selected_index() && !selector.wraps_selection()
        && !selector.is_adjusting() && selector.orientation() == UiOptionSelectorOrientation::Horizontal,
        "construction must start empty, horizontal, bounded and not adjusting");
    int changes = 0;
    selector.set_on_selection_changed([&](std::size_t){ ++changes; });
    selector.set_options(options());
    require(selector.selected_index() == 0 && changes == 0,"structural selection repair must be silent");
    require(!selector.set_selected_index(1) && !selector.set_selected_index(7),"disabled and missing indices must fail");
    require(selector.set_selected_index(0) && changes == 0,"reselecting a valid index succeeds silently");
    require(selector.select_next() && selector.selected_index() == 2 && changes == 1,"steps must skip disabled entries");
    require(!selector.select_next() && changes == 1,"bounded selection must stop at the last enabled entry");
    selector.set_wrap_selection(true);
    require(selector.select_next() && selector.selected_index() == 0,"forward wrapping must return to the first entry");
    require(selector.select_previous() && selector.selected_index() == 2,"backward wrapping must return to the last entry");
    const int before = changes;
    selector.set_options(options());
    require(selector.selected_index() == 2 && changes == before,"list replacement must preserve a usable index silently");
    selector.set_options({{ui_raw_text("only")}});
    require(selector.selected_index() == 0 && !selector.select_next() && !selector.select_previous(),
        "single-item lists must never step to themselves even with wrapping");
    selector.set_focused(true);
    (void)selector.on_ui_input_event(action(UiAction::Confirm));
    require(!selector.is_adjusting(),"single-item lists must not enter adjustment");
    selector.set_options({{ui_raw_text("disabled"),false}});
    require(!selector.selected_index(),"all-disabled lists must have no selection");
    selector.clear_options();
    require(!selector.selected_index() && selector.options().empty() && changes == before,
        "clearing or repairing a selection must not invoke the selection callback");
    selector.reset();
    selector.set_options(options());
    require(selector.select_next() && changes == before,"reset must also clear the old selection callback");
}

void test_directional_adjustment()
{
    for (const auto device : {input::InputDevice::Keyboard,input::InputDevice::Gamepad})
    for (const auto orientation : {UiOptionSelectorOrientation::Horizontal,UiOptionSelectorOrientation::Vertical})
    {
        UiOptionSelector selector;
        selector.set_options(options());
        selector.set_orientation(orientation);
        selector.set_focused(true);
        require(!selector.on_ui_input_event(action(UiAction::NavigateRight,device)),"unlocked directions belong to navigation");
        require(selector.on_ui_input_event(action(UiAction::Confirm,device)) && selector.is_adjusting(),
            "confirm must enter adjustment");
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device,UiInputEventType::ActionReleased));
        require(selector.is_adjusting(),"confirm release must not toggle adjustment twice");
        const auto next = orientation == UiOptionSelectorOrientation::Horizontal ? UiAction::NavigateRight : UiAction::NavigateDown;
        const auto previous = orientation == UiOptionSelectorOrientation::Horizontal ? UiAction::NavigateLeft : UiAction::NavigateUp;
        require(selector.on_ui_input_event(action(next,device)) && selector.selected_index() == 2,"the selected axis must advance");
        require(selector.on_ui_input_event(action(next,device)) && selector.selected_index() == 2,"endpoints must still consume navigation");
        const auto other = orientation == UiOptionSelectorOrientation::Horizontal ? UiAction::NavigateDown : UiAction::NavigateRight;
        require(selector.on_ui_input_event(action(other,device)) && selector.selected_index() == 2,
            "the other axis must not move selection or escape the control");
        require(selector.on_ui_input_event(action(previous,device)) && selector.selected_index() == 0,"reverse must skip disabled entries");
        (void)selector.on_ui_input_event(action(next,device));
        require(selector.on_ui_input_event(action(UiAction::Cancel,device)) && !selector.is_adjusting()
            && selector.selected_index() == 2,"cancel must leave adjustment without reverting live selection");
        require(!selector.on_ui_input_event(action(UiAction::Cancel,device)),"unlocked cancel must reach the parent");
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        require(!selector.is_adjusting(),"a second confirm must leave adjustment");
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        require(!selector.on_ui_input_event(action(UiAction::Tab,device)) && !selector.is_adjusting(),
            "Tab must end adjustment and remain available for host navigation");
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        selector.set_focused(false);
        require(!selector.is_adjusting(),"focus loss must end adjustment");
        selector.set_focused(true);
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        selector.set_options(options());
        require(!selector.is_adjusting(),"list replacement must end adjustment");
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        selector.set_enabled(false);
        require(!selector.is_adjusting() && !selector.on_ui_input_event(action(next,device)),"disabled controls cannot adjust");
        selector.set_enabled(true);
        selector.set_focused(true);
        (void)selector.on_ui_input_event(action(UiAction::Confirm,device));
        selector.set_orientation(orientation == UiOptionSelectorOrientation::Horizontal
            ? UiOptionSelectorOrientation::Vertical : UiOptionSelectorOrientation::Horizontal);
        require(!selector.is_adjusting(),"orientation changes must end adjustment");
    }
}

void test_pointer_contract()
{
    UiOptionSelector selector(core::Rect{10,20,300,48});
    selector.set_options(options());
    click(selector,286,44);
    require(selector.selected_index() == 2 && !selector.is_adjusting(),"mouse arrows must step without entering adjustment");
    click(selector,160,44);
    require(selector.is_adjusting(),"clicking the center must enter adjustment");
    click(selector,160,44);
    require(!selector.is_adjusting(),"clicking the center again must leave adjustment");
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerPressed,34,44));
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerReleased,286,44));
    require(selector.selected_index() == 2,"releasing over the opposite arrow must cancel");
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerPressed,34,44));
    (void)selector.on_ui_input_event(pointer(UiInputEventType::MouseMoved,400,44));
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerReleased,34,44));
    require(selector.selected_index() == 2,"moving out must cancel the original press");
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerPressed,34,44));
    selector.cancel_input_interaction();
    (void)selector.on_ui_input_event(pointer(UiInputEventType::PointerReleased,34,44));
    require(selector.selected_index() == 2,"explicit cancellation must prevent a delayed click");
    selector.set_presentation_translation({50,60});
    click(selector,84,104);
    require(selector.selected_index() == 0,"pointer hits must account for presentation translation");
    selector.set_orientation(UiOptionSelectorOrientation::Vertical);
    selector.set_screen_rect({10,20,200,160});
    click(selector,160,216);
    require(selector.selected_index() == 2,"the bottom arrow must advance a vertical selector");
    click(selector,160,104);
    require(selector.selected_index() == 0,"the top arrow must reverse a vertical selector");
    selector.set_style_overrides({.spacing=3.0f});
    selector.set_wrap_selection(true);
    selector.reset();
    require(!selector.selected_index() && selector.options().empty() && !selector.wraps_selection()
        && !selector.is_adjusting() && !selector.has_style_overrides()
        && selector.orientation() == UiOptionSelectorOrientation::Horizontal,"reset must clear configuration and interaction");
}

void test_callbacks_can_reenter_throw_and_remove()
{
    UiOptionSelector selector;
    selector.set_options(options());
    int first = 0,second = 0;
    selector.set_on_selection_changed([&](std::size_t)
    {
        ++first;
        selector.clear_options();
        selector.set_on_selection_changed([&](std::size_t){ ++second; });
    });
    require(selector.select_next() && first == 1 && selector.options().empty(),"callbacks may replace the option list");
    selector.set_options(options());
    require(selector.select_next() && second == 1,"callbacks may replace themselves safely");
    selector.set_on_selection_changed([](std::size_t){ throw std::runtime_error("selector callback"); });
    bool caught = false;
    try { (void)selector.select_previous(); } catch (const std::runtime_error&) { caught = true; }
    require(caught && selector.selected_index() == 0,"callback failures must propagate after selection is committed");

    UiPanel host(core::Rect{0,0,400,100});
    auto* owned = host.create_child<UiOptionSelector>(core::Rect{0,0,300,48});
    owned->set_options(options());
    host.set_scope_focused(true);
    require(host.focus_first_available(),"host must focus the selector");
    (void)host.on_ui_input_event(action(UiAction::Confirm));
    owned->set_on_selection_changed([&](std::size_t){ host.clear_children(); });
    require(host.on_ui_input_event(action(UiAction::NavigateRight)) && host.child_count() == 0,
        "selection callbacks may remove their owning child without further access");
}

void test_nested_focus_and_cancel()
{
    UiWindow window(core::Rect{0,0,800,600});
    int window_cancel = 0;
    window.set_on_cancel([&]{ ++window_cancel; });
    auto tabs = std::make_unique<UiTabContainer>(core::Rect{0,0,700,500});
    auto* tabs_raw = tabs.get();
    auto chrome = std::make_unique<UiChromeContainer>(core::Rect{0,0,600,400});
    auto header = std::make_unique<UiButton>(core::Rect{0,0,80,32});
    auto* header_raw = header.get();
    chrome->add_left_action(std::move(header));
    auto body = std::make_unique<UiListContainer>(core::Rect{0,0,500,300});
    auto* body_raw = body.get();
    auto selector = std::make_unique<UiOptionSelector>(core::Rect{0,0,300,140});
    auto* selector_raw = selector.get();
    selector->set_orientation(UiOptionSelectorOrientation::Vertical);
    selector->set_options(options());
    body->add_back(std::move(selector));
    chrome->set_body(std::move(body));
    require(tabs->add_tab(ui_raw_text("controls"),std::move(chrome)).added,"nested controls tab must attach");
    window.add_child(std::move(tabs));
    window.register_focus_scope(*tabs_raw);
    require(window.focus_first_available_scope(),"window must focus the nested scope");
    require(window.on_ui_input_event(action(UiAction::NavigateDown)),"navigation must enter the tab page");
    require(window.on_ui_input_event(action(UiAction::NavigateDown)) && selector_raw->is_focused(),
        "navigation must enter the chrome body selector");
    require(window.on_ui_input_event(action(UiAction::Confirm)) && selector_raw->is_adjusting(),
        "nested selectors must receive confirmation");
    require(window.on_ui_input_event(action(UiAction::NavigateUp)) && selector_raw->is_adjusting()
        && body_raw->focused_target() == selector_raw,"adjustment must win over the chrome header escape");
    require(window.on_ui_input_event(action(UiAction::NavigateDown)) && selector_raw->selected_index() == 2,
        "nested directional input must be delivered exactly once");
    require(window.on_ui_input_event(action(UiAction::Cancel)) && !selector_raw->is_adjusting()
        && window_cancel == 0,"the first Cancel must only exit the nested adjustment");
    require(window.on_ui_input_event(action(UiAction::Cancel)) && window_cancel == 1,
        "the next Cancel must reach the window");
    require(window.on_ui_input_event(action(UiAction::NavigateUp)) && header_raw->is_focused(),
        "unlocked selectors must still permit header navigation");
}

class CancelSpy final : public UiButton
{
public:
    using UiButton::UiButton;
    int cancellations = 0;
    bool on_ui_input_event(const UiInputEvent& event) override
    {
        if (event.action == UiAction::Cancel) ++cancellations;
        return UiButton::on_ui_input_event(event);
    }
};

void test_first_click_activates_the_delegated_scope()
{
    UiWindow window(core::Rect{0,0,800,600});
    auto tabs = std::make_unique<UiTabContainer>(core::Rect{0,0,700,500});
    auto* tabs_raw = tabs.get();
    auto row = std::make_unique<UiListContainer>(core::Rect{0,0,600,300});
    row->set_direction(UiListDirection::Horizontal);
    auto* selector = static_cast<UiOptionSelector*>(row->add_back(
        std::make_unique<UiOptionSelector>(core::Rect{0,0,300,48})));
    selector->set_options(options());
    auto* spy = static_cast<CancelSpy*>(row->add_back(std::make_unique<CancelSpy>(core::Rect{0,0,80,48})));
    require(tabs->add_tab(ui_raw_text("page"),std::move(row)).added,"pointer test page must attach");
    window.add_child(std::move(tabs));
    window.register_focus_scope(*tabs_raw);
    require(window.focus_first_available_scope(),"pointer test starts with the tab bar focused");
    window.update(0);
    const auto rect = selector->presentation_screen_rect();
    const int x = static_cast<int>(rect.right() - 24),y = static_cast<int>(rect.center().y);
    require(window.on_ui_input_event(pointer(UiInputEventType::PointerPressed,x,y)),"first pointer press must reach the page");
    require(window.on_ui_input_event(pointer(UiInputEventType::PointerReleased,x,y)) && selector->selected_index() == 2,
        "activating a delegated page must not cancel its first click");
    require(window.on_ui_input_event(action(UiAction::Confirm)) && selector->is_adjusting(),"the clicked selector must retain focus");
    require(window.on_ui_input_event(action(UiAction::Cancel)) && spy->cancellations == 0,
        "Cancel must be directed to the focused control instead of broadcast to siblings");
    (void)window.on_ui_input_event(action(UiAction::Cancel,input::InputDevice::Keyboard,UiInputEventType::ActionReleased));
    require(spy->cancellations == 0,"Cancel release must not be broadcast to unrelated controls");
    require(window.on_ui_input_event(action(UiAction::Confirm)),"selector must re-enter adjustment");
    selector->set_on_selection_changed([](std::size_t){ throw std::runtime_error("window selector callback"); });
    bool caught = false;
    try { (void)window.on_ui_input_event(action(UiAction::NavigateLeft)); }
    catch (const std::runtime_error&) { caught = true; }
    require(caught && selector->selected_index() == 0,"selector callback failures must propagate through the window");
}

void test_overlay_and_popup_cancel_priority()
{
    for (bool modal : {false,true})
    {
        UiWindow window(core::Rect{0,0,800,600});
        int background_cancels = 0;
        window.set_on_cancel([&]{ ++background_cancels; });
        auto* overlay = window.create_child<UiPanel>(core::Rect{0,0,400,200});
        auto* selector = overlay->create_child<UiOptionSelector>(core::Rect{0,0,300,48});
        selector->set_options(options());
        UiOverlayOptions settings;
        settings.modal = modal;
        settings.close_on_cancel = true;
        require(window.register_overlay(*overlay,settings),"selector overlay must register");
        window.open_overlay(*overlay);
        require(window.on_ui_input_event(action(UiAction::Confirm)) && selector->is_adjusting(),"overlay selector must enter adjustment");
        require(window.on_ui_input_event(action(UiAction::Cancel)) && window.is_overlay_open(*overlay)
            && !selector->is_adjusting(),"first Cancel must preserve the overlay");
        require(window.on_ui_input_event(action(UiAction::Cancel)) && !window.is_overlay_open(*overlay)
            && background_cancels == 0,"second Cancel must close only the overlay");
    }
    UiWindow window(core::Rect{0,0,800,600});
    int cancels = 0;
    window.set_on_cancel([&]{ ++cancels; });
    auto* panel = window.create_child<UiPanel>(core::Rect{0,0,400,200});
    auto* selector = panel->create_child<UiOptionSelector>(core::Rect{0,0,300,48});
    selector->set_options(options());
    window.register_focus_scope(*panel);
    require(window.focus_first_available_scope(),"popup test scope must focus");
    require(window.on_ui_input_event(action(UiAction::Confirm)),"selector must enter adjustment before opening popup");
    auto* dropdown = window.create_child<UiDropdown>(core::Rect{0,100,200,40});
    dropdown->set_options({{ui_raw_text("item")}});
    dropdown->register_with_window(window);
    dropdown->open();
    require(dropdown->is_open(),"transient popup must open");
    require(window.on_ui_input_event(action(UiAction::Cancel)) && !dropdown->is_open() && cancels == 0,
        "transient popup Cancel must not reach the background window");
}
}

int main()
{
    test_selection_contract();
    test_directional_adjustment();
    test_pointer_contract();
    test_callbacks_can_reenter_throw_and_remove();
    test_nested_focus_and_cancel();
    test_first_click_activates_the_delegated_scope();
    test_overlay_and_popup_cancel_priority();
    return EXIT_SUCCESS;
}
