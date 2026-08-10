#define SDL_MAIN_HANDLED

#include "engine/application/lifecycle/application_event_boundary.h"
#include "engine/scene/scene.h"
#include "engine/ui/containers/ui_button_group.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_checkbox.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
using elysia::tests::require;

class TestScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
};

class ClearingChild final : public elysia::ui::UiElement,
    public elysia::core::Updatable,
    public elysia::ui::UiInputFrameReceiver,
    public elysia::ui::UiInputEventReceiver
{
public:
    enum class Trigger { Update,Presentation,Frame,Event,Render };

    ClearingChild(elysia::ui::UiChildHost& host,Trigger trigger,int& calls)
        : _host(host),_trigger(trigger),_calls(calls) {}

    void update(double) override { if (_trigger == Trigger::Update) { ++_calls; _host.clear_children(); } }
    void update_presentation_animations(double) override
    { if (_trigger == Trigger::Presentation) { ++_calls; _host.clear_children(); } }
    void on_ui_input_frame(const elysia::ui::UiInputFrame&) override
    { if (_trigger == Trigger::Frame) { ++_calls; _host.clear_children(); } }
    bool on_ui_input_event(const elysia::ui::UiInputEvent&) override
    {
        if (_trigger == Trigger::Event) { ++_calls; _host.clear_children(); }
        return false;
    }
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>&) const override
    { if (_trigger == Trigger::Render) { ++_calls; _host.clear_children(); } }

private:
    elysia::ui::UiChildHost& _host;
    Trigger _trigger;
    int& _calls;
};

class CountingChild final : public elysia::ui::UiElement,
    public elysia::core::Updatable,
    public elysia::ui::UiInputFrameReceiver,
    public elysia::ui::UiInputEventReceiver
{
public:
    explicit CountingChild(int& calls) : _calls(calls) {}
    void update(double) override { ++_calls; }
    void update_presentation_animations(double) override { ++_calls; }
    void on_ui_input_frame(const elysia::ui::UiInputFrame&) override { ++_calls; }
    bool on_ui_input_event(const elysia::ui::UiInputEvent&) override { ++_calls; return false; }
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>&) const override { ++_calls; }

private:
    int& _calls;
};

class OrderedChild final : public elysia::ui::UiElement, public elysia::ui::UiInputEventReceiver
{
public:
    OrderedChild(int id,int order,std::vector<int>& rendered,std::vector<int>& received)
        : UiElement(elysia::core::Rect{ 0,0,20,20 },order),_id(id),_rendered(rendered),_received(received) {}

    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>&) const override
    {
        _rendered.push_back(_id);
    }

    bool on_ui_input_event(const elysia::ui::UiInputEvent&) override
    {
        _received.push_back(_id);
        return false;
    }

private:
    int _id;
    std::vector<int>& _rendered;
    std::vector<int>& _received;
};

class AddingChild final : public elysia::ui::UiElement, public elysia::core::Updatable
{
public:
    AddingChild(elysia::ui::UiChildHost& host,int& added_calls) : _host(host),_added_calls(added_calls) {}

    void update(double) override
    {
        if (_added)
            return;
        _added = true;
        _host.add_child(std::make_unique<CountingChild>(_added_calls));
    }

private:
    elysia::ui::UiChildHost& _host;
    int& _added_calls;
    bool _added = false;
};

void test_callback_exceptions_reach_window_scene_and_boundary()
{
    using namespace elysia;
    const ui::UiInputEvent pressed{ .action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionPressed };
    const ui::UiInputEvent released{ .action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionReleased };

    ui::UiWindow window(core::Rect{ 0,0,320,120 });
    auto panel = std::make_unique<ui::UiPanel>(core::Rect{ 0,0,320,120 });
    ui::UiPanel* panel_raw = panel.get();
    auto checkbox = std::make_unique<ui::UiCheckbox>(core::Rect{ 0,0,40,40 });
    checkbox->set_on_toggled([](ui::UiCheckboxState) { throw std::runtime_error("checkbox callback"); });
    panel_raw->add_child(std::move(checkbox));
    window.add_child(std::move(panel));
    window.register_focus_scope(*panel_raw);
    require(window.focus_first_available_scope(),"window should focus the checkbox panel");
    require(window.on_ui_input_event(pressed),"checkbox confirm press should route through window");
    bool window_threw = false;
    try { (void)window.on_ui_input_event(released); }
    catch (const std::runtime_error&) { window_threw = true; }
    require(window_threw,"callback exceptions must escape the window input path");

    TestScene scene;
    auto scene_window = std::make_unique<ui::UiWindow>(core::Rect{ 0,0,320,120 });
    auto scene_panel = std::make_unique<ui::UiPanel>(core::Rect{ 0,0,320,120 });
    ui::UiPanel* scene_panel_raw = scene_panel.get();
    auto scene_checkbox = std::make_unique<ui::UiCheckbox>(core::Rect{ 0,0,40,40 });
    scene_checkbox->set_on_toggled([](ui::UiCheckboxState) { throw std::runtime_error("scene callback"); });
    scene_panel_raw->add_child(std::move(scene_checkbox));
    scene_window->add_child(std::move(scene_panel));
    scene_window->register_focus_scope(*scene_panel_raw);
    require(scene_window->focus_first_available_scope(),"scene window should focus the checkbox panel");
    scene.add_object(std::move(scene_window));
    const input::RawInputEvent raw_press{ .control=input::RawInputControl::KeyEnter,.type=input::RawInputEventType::ControlPressed,.device=input::InputDevice::Keyboard };
    const input::RawInputEvent raw_release{ .control=input::RawInputControl::KeyEnter,.type=input::RawInputEventType::ControlReleased,.device=input::InputDevice::Keyboard };
    bool scene_threw = false;
    try { scene.on_input({}, { raw_press,raw_release }); }
    catch (const std::runtime_error&) { scene_threw = true; }
    require(scene_threw,"callback exceptions must escape the full Scene UI input route");

    bool continued = false;
    const bool completed = elysia::application::run_event_boundary("test",[&]()
    {
        throw std::runtime_error("boundary callback");
        continued = true;
    });
    require(!completed && !continued,"application boundary must catch callback exceptions and stop the phase");
}

void test_child_host_tolerates_callback_tree_mutation()
{
    using namespace elysia;
    for (const auto trigger : {
            ClearingChild::Trigger::Update,
            ClearingChild::Trigger::Presentation,
            ClearingChild::Trigger::Frame,
            ClearingChild::Trigger::Event,
            ClearingChild::Trigger::Render })
    {
        ui::UiChildHost host;
        int clear_calls = 0;
        int sibling_calls = 0;
        if (trigger == ClearingChild::Trigger::Event)
        {
            host.add_child(std::make_unique<CountingChild>(sibling_calls));
            host.add_child(std::make_unique<ClearingChild>(host,trigger,clear_calls));
        }
        else
        {
            host.add_child(std::make_unique<ClearingChild>(host,trigger,clear_calls));
            host.add_child(std::make_unique<CountingChild>(sibling_calls));
        }
        if (trigger == ClearingChild::Trigger::Update)
            host.update(0.0);
        else if (trigger == ClearingChild::Trigger::Presentation)
            host.update_presentation_animations(0.0);
        else if (trigger == ClearingChild::Trigger::Frame)
            host.on_ui_input_frame({});
        else if (trigger == ClearingChild::Trigger::Event)
            (void)host.on_ui_input_event({});
        else
        {
            std::vector<core::UiRenderCommand> commands;
            host.submit_ui_render_commands(commands);
        }
        require(clear_calls == 1 && sibling_calls == 0 && host.child_count() == 0,
            "host traversal must tolerate a child clearing the tree");
    }
}

void test_child_host_cached_visual_order_and_lifetime_handles()
{
    using namespace elysia;
    ui::UiChildHost host;
    std::vector<int> rendered;
    std::vector<int> received;

    auto first = std::make_unique<OrderedChild>(1,10,rendered,received);
    OrderedChild* first_raw = first.get();
    host.add_child(std::move(first));
    host.add_child(std::make_unique<OrderedChild>(2,0,rendered,received));
    host.add_child(std::make_unique<OrderedChild>(3,10,rendered,received));

    std::vector<core::UiRenderCommand> commands;
    host.submit_ui_render_commands(commands);
    require(rendered == std::vector<int>{ 2,1,3 },
        "visual cache must sort by order while retaining logical order for ties");
    (void)host.on_ui_input_event({});
    require(received == std::vector<int>{ 3,1,2 },
        "input must traverse the cached visual order in reverse");

    rendered.clear();
    received.clear();
    first_raw->set_order(20);
    host.submit_ui_render_commands(commands);
    require(rendered == std::vector<int>{ 2,3,1 },
        "set_order must invalidate and rebuild the visual cache");
    (void)host.on_ui_input_event({});
    require(received == std::vector<int>{ 1,3,2 },
        "input cache must observe a changed child order on the next dispatch");

    auto extracted = host.extract_child(1);
    require(extracted != nullptr,"extract_child must return the selected logical child");
    ui::UiChildHost second_host;
    second_host.add_child(std::move(extracted));
    rendered.clear();
    host.submit_ui_render_commands(commands);
    require(rendered == std::vector<int>{ 3,1 },
        "removed child handles must not resolve in the original host cache");
    rendered.clear();
    second_host.submit_ui_render_commands(commands);
    require(rendered == std::vector<int>{ 2 },
        "re-added children must receive fresh lifetime handles in their new host");
}

void test_child_host_cached_logical_order_after_mutation()
{
    using namespace elysia;
    ui::UiChildHost host;
    int added_calls = 0;
    host.add_child(std::make_unique<AddingChild>(host,added_calls));

    host.update(0.0);
    require(added_calls == 0,"children added during traversal must wait for the next traversal");
    host.update(0.0);
    require(added_calls == 1,"the rebuilt logical cache must include children added by a prior traversal");

    std::vector<int> rendered;
    std::vector<int> received;
    ui::UiChildHost ties;
    ties.add_child(std::make_unique<OrderedChild>(1,0,rendered,received));
    ties.add_child(std::make_unique<OrderedChild>(2,0,rendered,received));
    ties.add_child(std::make_unique<OrderedChild>(3,0,rendered,received));
    require(ties.move_child(2,0),"move_child must accept valid logical indices");
    std::vector<core::UiRenderCommand> commands;
    ties.submit_ui_render_commands(commands);
    require(rendered == std::vector<int>{ 3,1,2 },
        "move_child must invalidate equal-order visual tie ordering");
}

void test_text_input_callback_can_remove_its_owner()
{
    using namespace elysia;
    ui::UiChildHost host;
    auto input = std::make_unique<ui::UiTextInput>(core::Rect{ 0,0,160,40 });
    ui::UiTextInput* raw_input = input.get();
    raw_input->set_on_text_changed([&](std::string_view) { host.clear_children(); });
    host.add_child(std::move(input));
    raw_input->set_text("new text");
    require(host.child_count() == 0,"text callback should be able to remove its owning subtree");
}
}

int main()
{
    test_callback_exceptions_reach_window_scene_and_boundary();
    test_child_host_tolerates_callback_tree_mutation();
    test_child_host_cached_visual_order_and_lifetime_handles();
    test_child_host_cached_logical_order_after_mutation();
    test_text_input_callback_can_remove_its_owner();
    std::cout << "ui callback safety tests passed\n";
    return EXIT_SUCCESS;
}

