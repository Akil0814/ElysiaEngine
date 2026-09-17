#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/input/input_system.h"
#include "engine/ui/core/ui_element.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/test_assertions.h"
#include <functional>
#include <limits>
#include <iostream>
using namespace elysia::gameplay;
using namespace elysia::input;
using elysia::tests::require;
const InputActionId Motion{"test.motion"}, Look{"test.look"}, Wheel{"test.wheel"}, Fire{"test.fire"};
struct Actor final : elysia::core::GameObject, ControlCommandReceiver
{
    Actor() : GameObject(elysia::core::DepthLayer::Character)
    {
    }
    void on_control_command(const ControlCommand &c, double) override
    {
        commands.push_back(c);
        if (callback)
            callback();
    }
    void on_control_cancelled(InputCancelReason reason) override
    {
        reasons.push_back(reason);
    }
    std::vector<ControlCommand> commands;
    std::vector<InputCancelReason> reasons;
    std::function<void()> callback;
};
struct SyntheticController final : Controller
{
    std::function<void()> callback;
    float value = 0.75f;
    bool tap = false;
    float previous_event_value = 0;
    void produce_intent(std::uint64_t, double) override
    {
        if (callback)
            callback();
        ActionInputResult input;
        input.frame.set(Motion, InputActionValueType::Axis1D, {value, 0});
        if (tap)
            input.events.push_back(
                {Fire, InputActionValueType::Button, ActionInputPhase::Started, {1, 0}, {previous_event_value, 0}});
        submit(std::move(input));
    }
};
template <int Tag> struct World final : GameplayScene
{
    explicit World(World **output)
    {
        *output = this;
    }
    void on_enter(const elysia::scene::ScenePayload &) override
    {
        ++enters;
    }
    void on_exit() override
    {
        ++exits;
    }
    void reset() override
    {
        ++resets;
    }
    void on_game_fixed_update(std::uint64_t, double) override
    {
        ++extensions;
        if (check)
            check();
    }
    int enters = 0, exits = 0, resets = 0, extensions = 0;
    std::function<void()> check;
};
struct PointerSink final : elysia::ui::UiElement, elysia::ui::UiInputEventReceiver
{
    bool on_ui_input_event(const elysia::ui::UiInputEvent &event) override
    {
        return event.type == elysia::ui::UiInputEventType::MouseMoved;
    }
};
struct PointerCapture final : elysia::ui::UiElement
{
    InputCapture input_capture() const noexcept override
    {
        return InputCapture::Pointer;
    }
};
struct Menu final : elysia::scene::Scene
{
    void on_enter(const elysia::scene::ScenePayload &) override
    {
    }
    void on_exit() override
    {
    }
    void reset() override
    {
    }
};
InputActionMap map()
{
    InputActionMap result;
    require(result.register_action({Motion, InputActionValueType::Axis1D},
                                   {{Motion, ButtonInputBinding{RawInputControl::KeyD}}}),
            "Movement map");
    require(result.register_action({Fire, InputActionValueType::Button},
                                   {{Fire, ButtonInputBinding{RawInputControl::KeySpace}}}),
            "Button map");
    require(result.register_action(
                {Look, InputActionValueType::Axis2D, 0.5f, 0.2f, InputValueSemantics::Delta},
                {{Look, PointerDeltaBinding{PointerDeltaAxis::MouseX}},
                 {Look, PointerDeltaBinding{PointerDeltaAxis::MouseY, InputActionComponent::Y}}}),
            "Pointer map");
    require(
        result.register_action({Wheel, InputActionValueType::Axis1D, 0.5f, 0.2f, InputValueSemantics::Delta},
                               {{Wheel, PointerDeltaBinding{PointerDeltaAxis::WheelY}}}),
        "Wheel map");
    require(!result.add_binding({Look, AxisInputBinding{RawInputAxis::GamepadLeftX}}),
            "Mixed state/delta action rejected");
    require(!result.add_binding({Motion, ButtonInputBinding{RawInputControl::KeyA, InputActionComponent::X,
                                                            std::numeric_limits<float>::infinity()}}),
            "Non-finite mapping scale rejected");
    return result;
}
void route(elysia::scene::SceneManager &manager, int key,
           elysia::scene::SceneReloadMode reload = elysia::scene::SceneReloadMode::Reuse)
{
    manager.on_scene_request(
        {.type = elysia::scene::SceneRequestType::Switch,
         .route = {.target = static_cast<elysia::scene::SceneKey>(key), .reload_mode = reload}});
    manager.on_update(0);
}
int main()
{
    auto *service = ControllerService::instance();
    require(!service->create<SyntheticController>({ControllerScope::Session, {}}), "No implicit session");
    require(!service->begin_session(), "No engine runtime before SceneManager initialization");
    elysia::scene::SceneManager manager;
    require(bool(service->begin_session()), "Begin explicit session");
    require(!service->begin_session(), "Repeated begin cannot reset session");
    World<1> *first = nullptr;
    World<2> *second = nullptr;
    manager.register_game_scene<World<1>>(1, &first);
    manager.register_game_scene<World<2>>(2, &second);
    manager.register_game_scene<Menu>(3);
    manager.start({.target = 1});
    auto *a = first->create_and_add_object<Actor>();
    auto *b = first->create_and_add_object<Actor>();
    manager.on_update(1.0 / 60);
    require(a->commands.empty(), "Empty gameplay scenes create no controllers");
    auto custom =
        service->create<SyntheticController>({ControllerScope::Scene, first->control_context().token()});
    require(bool(custom), "Create non-device controller without a player");
    require(bool(service->bind_target(*custom, first->control_context(), *a)), "Bind custom controller");
    first->check = [&] { require(!a->commands.empty(), "Commands precede game fixed extension"); };
    manager.on_update(1.0 / 60);
    first->check = {};
    require(a->commands.back().state.axis1d(Motion) == 0.75f,
            "Non-device intention uses common command pipeline");
    require(!service->begin_session() && service->get(*custom),
            "Duplicate session begin preserves live controllers");
    require(!service->replace_input_map(*custom, map()) && service->describe(*custom)->bound,
            "Mapping replacement rejects non-local controller without changing its binding");
    auto session = service->create<SyntheticController>({ControllerScope::Session, {}});
    require(!service->bind_target(*session, first->control_context(), *a), "Exclusive targets");
    require(bool(service->bind_target(*session, first->control_context(), *b)), "Bind session controller");
    require(!service->bind_target(*custom, first->control_context(), *b) && service->describe(*custom)->bound,
            "Failed binding keeps original target");
    auto old_generation = service->describe(*custom)->binding_generation;
    route(manager, 3);
    require(!service->describe(*custom)->bound && !service->describe(*session)->bound,
            "Leaving unbinds all scopes");
    auto count = a->commands.size();
    manager.on_update(0.1);
    require(a->commands.size() == count, "Pure menu never schedules cached worlds");
    route(manager, 2);
    auto *foreign = second->create_and_add_object<Actor>();
    require(!service->bind_target(*custom, second->control_context(), *foreign),
            "Scene-owned controller cannot move to another scene");
    require(!service->bind_target(*session, second->control_context(), *a),
            "Foreign target rejected even with active context");
    require(!service->bind_target(*session, first->control_context(), *a),
            "Inactive scene context rejects binding");
    require(bool(service->bind_target(*session, second->control_context(), *foreign)),
            "Session controller binds a different active world");
    manager.on_update(1.0 / 60);
    require(foreign->commands.size() == 1, "Second world consumes session controller command");
    route(manager, 1);
    require(service->get(*custom) && service->get(*session), "Reuse preserves instances");
    require(service->describe(*custom)->binding_generation > old_generation,
            "Leaving invalidates binding generation");
    require(bool(service->bind_target(*session, first->control_context(), *a)),
            "Game explicitly rebinds reused world");
    auto old_context = first->control_context().token();
    route(manager, 1, elysia::scene::SceneReloadMode::Reset);
    require(!service->get(*custom) && service->get(*session), "Reset releases scene scope only");
    require(!service->create<SyntheticController>({ControllerScope::Scene, old_context}),
            "Stale scene generation rejected");
    require(!service->describe(*session)->bound, "Reset invalidates session target");
    auto scene_again =
        service->create<SyntheticController>({ControllerScope::Scene, first->control_context().token()});
    route(manager, 1, elysia::scene::SceneReloadMode::Recreate);
    require(!service->get(*scene_again) && service->get(*session), "Recreate preserves only session scope");
    a = first->create_and_add_object<Actor>();
    b = first->create_and_add_object<Actor>();
    auto local = service->create<LocalPlayerController>(
        {ControllerScope::Scene, first->control_context().token()}, PrimaryLocalPlayer, map());
    require(bool(service->bind_target(*local, first->control_context(), *a)), "Local controller target");
    auto duplicate =
        service->create<LocalPlayerController>({ControllerScope::Session, {}}, PrimaryLocalPlayer, map());
    require(!service->bind_target(*duplicate, first->control_context(), *b),
            "One bound local controller per player per scene");
    elysia::tests::InputSnapshotBuilder devices;
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 40, .mouse_delta_y = -9});
    devices.event({.type = RawInputEventType::MouseWheel, .wheel_y = 3});
    devices.press(RawInputControl::KeySpace, true);
    devices.press(RawInputControl::KeySpace, false);
    first->on_input(devices.take());
    manager.on_update(0);
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 22});
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(a->commands.back().deltas.at(Look).x == 62 && a->commands.back().deltas.at(Look).y == -9 &&
                a->commands.back().deltas.at(Wheel).x == 3,
            "Zero-tick deltas accumulate exactly once without clamping");
    require(a->commands.back().events.size() == 2, "Tap retains ordered started and canceled events");
    manager.on_update(3.0 / 60);
    require(a->commands.back().deltas.empty() && a->commands.back().events.empty(),
            "Catchup never repeats deltas or events");
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 17});
    first->on_input(devices.take());
    devices.event({.type = RawInputEventType::MouseWheel, .wheel_y = 5});
    devices.press(RawInputControl::KeySpace, true);
    devices.press(RawInputControl::KeySpace, false);
    first->on_input(devices.take());
    auto *sink = first->create_and_add_object<PointerSink>();
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 23});
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(!a->commands.back().deltas.contains(Look) && a->commands.back().deltas.at(Wheel).x == 5 &&
                a->commands.back().events.size() == 2,
            "HUD motion consumption clears queued look delta but preserves unrelated wheel and tap");
    sink->set_active(false);
    sink->set_visible(false);
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 17});
    first->on_input(devices.take());
    auto *capture = first->create_and_add_object<PointerCapture>();
    devices.event({.type = RawInputEventType::MouseMoved, .mouse_delta_x = 23});
    devices.event({.type = RawInputEventType::MouseWheel, .wheel_y = 5});
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(a->commands.back().deltas.empty(),
            "UI pointer capture clears pending and current mouse/wheel deltas");
    capture->set_active(false);
    capture->set_visible(false);
    devices.press(RawInputControl::KeyD, true);
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(a->commands.back().state.axis1d(Motion) == 1, "Held movement");
    require(bool(service->replace_input_map(*local, map())), "Replace validated mapping");
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(a->commands.back().state.axis1d(Motion) == 0, "Replacing map requires held control release");
    devices.press(RawInputControl::KeyD, false);
    first->on_input(devices.take());
    devices.press(RawInputControl::KeyD, true);
    first->on_input(devices.take());
    manager.on_update(1.0 / 60);
    require(a->commands.back().state.axis1d(Motion) == 1, "Fresh input after mapping change");
    devices.press(RawInputControl::KeySpace, true);
    first->on_input(devices.take());
    auto queued_count = a->commands.size();
    first->local_players().unbind_source(InputSourceId::keyboard_mouse());
    manager.on_update(1.0 / 60);
    require(a->commands.size() == queued_count,
            "Device reassignment between input and tick cannot deliver stale command");
    require(first->local_players().bind_source(PrimaryLocalPlayer, InputSourceId::keyboard_mouse()),
            "Restore keyboard ownership");
    service->unbind_target(*local);
    require(bool(service->bind_target(*session, first->control_context(), *a)), "Non-device takeover");
    first->set_all_gameplay_input_blocked(true);
    manager.on_update(1.0 / 60);
    require(a->commands.back().state.axis1d(Motion) == 0.75f,
            "Local UI capture does not cancel custom controllers");
    count = a->commands.size();
    first->pause();
    manager.on_update(0.1);
    require(a->commands.size() == count, "World pause stops every controller");
    first->resume();
    // Removing a later controller from a callback must stop its delivery in the same tick.
    service->remove(*session);
    auto driver =
        service->create<SyntheticController>({ControllerScope::Scene, first->control_context().token()});
    auto victim =
        service->create<SyntheticController>({ControllerScope::Scene, first->control_context().token()});
    (void)service->bind_target(*driver, first->control_context(), *a);
    (void)service->bind_target(*victim, first->control_context(), *b);
    ControllerHandle created;
    bool once = false;
    auto competitor = service->create<SyntheticController>({ControllerScope::Session, {}});
    a->callback = [&] {
        if (once)
            return;
        once = true;
        service->remove(*victim);
        created = *service->create<SyntheticController>({ControllerScope::Session, {}});
        require(bool(service->bind_target(created, first->control_context(), *b)),
                "Callback binding accepted at safe boundary");
        require(!service->bind_target(*competitor, first->control_context(), *b),
                "Pending bindings reserve exclusive target ownership");
    };
    manager.on_update(1.0 / 60);
    require(b->commands.empty(), "Removed and newly created controllers cannot deliver this tick");
    manager.on_update(1.0 / 60);
    require(b->commands.size() == 1, "New controller starts next tick");
    a->callback = {};
    // A callback can exchange targets only after the current dispatch boundary.
    auto *replacement = first->create_and_add_object<Actor>();
    auto generation = service->describe(created)->binding_generation;
    b->callback = [&] {
        require(bool(service->bind_target(created, first->control_context(), *replacement)),
                "Callback rebind");
    };
    manager.on_update(1.0 / 60);
    require(replacement->commands.empty() && service->describe(created)->binding_generation > generation,
            "Rebind commits after dispatch, clears old commands");
    manager.on_update(1.0 / 60);
    require(replacement->commands.size() == 1, "Rebound target receives next tick");
    b = replacement;
    const auto before_pause = b->commands.size();
    a->callback = [&] { first->pause(); };
    manager.on_update(1.0 / 60);
    require(b->commands.size() == before_pause,
            "Pause requested during delivery stops later controllers immediately");
    a->callback = {};
    first->resume();

    service->get<SyntheticController>(created)->value = std::numeric_limits<float>::quiet_NaN();
    manager.on_update(1.0 / 60);
    require(b->commands.back().state.finite(), "Non-finite custom state never reaches command cache");
    auto* invalid_event_controller=service->get<SyntheticController>(created);
    invalid_event_controller->value=0.75f;
    invalid_event_controller->tap=true;
    invalid_event_controller->previous_event_value=std::numeric_limits<float>::infinity();
    manager.on_update(1.0/60);
    require(b->commands.back().events.empty(),"Non-finite previous event values are rejected as well as current values");
    b->destroy();
    manager.on_update(1.0 / 60);
    require(!service->describe(created)->bound, "Object released only after target unregister");
    service->end_session();
    require(!service->get(created) && !service->get(*driver), "End session invalidates handles");
    manager.shutdown();
    require(!service->begin_session(), "Shutdown disables runtime");
    elysia::scene::SceneManager restarted_manager;
    require(bool(service->begin_session()), "New application/session lifetime");
    auto restarted = service->create<SyntheticController>({ControllerScope::Session, {}});
    require(restarted->runtime != created.runtime && !service->get(created),
            "Runtime generation rejects old handles");
    service->end_session();
    std::cout << "controller runtime tests passed\n";
}
