#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/input/input_system.h"
#include "engine/scene/scene_manager.h"
#include "engine/ui/input/ui_input_router.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/test_assertions.h"
#include <functional>
#include <iostream>
#include <type_traits>
using namespace elysia::input;
using namespace elysia::gameplay;
using elysia::tests::require;
namespace
{
const InputActionId Move{"regression.move"}, Wheel{"regression.wheel"}, Fire{"regression.fire"},
    Throttle{"regression.throttle"};
struct Actor : elysia::core::GameObject, ControlCommandReceiver
{
    Actor() : GameObject(elysia::core::DepthLayer::Character)
    {
    }
    std::vector<ControlCommand> commands;
    std::vector<InputCancelReason> reasons;
    std::function<void()> callback;
    void on_control_command(const ControlCommand &c, double) override
    {
        commands.push_back(c);
        if (callback)
            callback();
    }
    void on_control_cancelled(InputCancelReason r) override
    {
        reasons.push_back(r);
    }
};
struct Driver : Controller
{
    std::function<void()> produce, cancel;
    void produce_intent(std::uint64_t, double) override
    {
        ActionInputResult input;
        input.frame.set(Move, InputActionValueType::Axis1D, {1, 0});
        if (produce)
            produce();
        submit(std::move(input));
    }
    void cancelled(InputCancelReason) override
    {
        if (cancel)
            cancel();
    }
};
struct Local : LocalPlayerController
{
    using LocalPlayerController::LocalPlayerController;
    std::function<void()> mapped;
    void on_mapped_input(const ActionInputResult &) override
    {
        if (mapped)
            mapped();
    }
};
struct World : GameplayScene
{
    explicit World(World **slot)
    {
        *slot = this;
    }
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
struct OtherWorld : World
{
    using World::World;
};
struct Fixture
{
    elysia::scene::SceneManager manager;
    ControllerService *service = ControllerService::instance();
    World *world = nullptr;
    Fixture()
    {
        require(bool(service->begin_session()), "Session starts");
        manager.register_game_scene<World>(987, &world);
        manager.start({.target = 987});
    }
    void tick()
    {
        manager.on_update(1.0 / 60);
    }
    ControllerHandle driver(Actor &a)
    {
        auto h = *service->create<Driver>({ControllerScope::Scene, world->control_context().token()});
        require(service->bind_target(h, world->control_context(), a).succeeded(), "Bind driver");
        return h;
    }
};
InputActionMap movement_map()
{
    InputActionMap map;
    require(map.register_action(
                {Move, InputActionValueType::Axis2D},
                {{Move, Axis2DInputBinding{RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY}}}),
            "Movement map");
    require(map.register_action({Wheel, InputActionValueType::Axis1D, 0.5f, 0.2f, InputValueSemantics::Delta},
                                {{Wheel, PointerDeltaBinding{PointerDeltaAxis::WheelY}}}),
            "Wheel map");
    require(map.register_action({Fire, InputActionValueType::Button},
                                {{Fire, ButtonInputBinding{RawInputControl::KeySpace}}}),
            "Fire map");
    require(map.register_action({Throttle, InputActionValueType::Axis1D, 0.01f, 0.01f},
                                {{Throttle, AxisInputBinding{RawInputAxis::GamepadRightX}}}),
            "Low dead zone map");
    return map;
}
void test_gates_and_validation()
{
    InputSuppression gate;
    RawInputState physical;
    physical.set_axis(RawInputAxis::GamepadLeftX, 0.19f);
    physical.set_axis(RawInputAxis::GamepadLeftY, 0.19f);
    gate.observe(physical, InputCapture::Gamepad);
    auto output = gate.filter(physical, InputCapture::Gamepad);
    require(output.axis_value(RawInputAxis::GamepadLeftX) == 0 &&
                output.axis_value(RawInputAxis::GamepadLeftY) == 0,
            "Small diagonal is captured");
    physical.set_axis(RawInputAxis::GamepadLeftX, 0.8f);
    gate.observe(physical, InputCapture::Gamepad);
    RawInputState neutral;
    (void)gate.filter(neutral);
    require(gate.filter(physical).axis_value(RawInputAxis::GamepadLeftX) == 0,
            "Filter queries cannot release gate");
    gate.observe(neutral);
    gate.observe(physical);
    require(gate.filter(physical).axis_value(RawInputAxis::GamepadLeftX) == 0.8f,
            "Observed neutral restores axis");
    physical.set_pressed(RawInputControl::KeySpace, true);
    gate.observe(physical, InputCapture::Keyboard);
    require(!gate.filter(physical).is_pressed(RawInputControl::KeySpace), "Held key gated");
    physical.set_pressed(RawInputControl::KeySpace, false);
    gate.observe(physical);
    physical.set_pressed(RawInputControl::KeySpace, true);
    gate.observe(physical);
    require(gate.filter(physical).is_pressed(RawInputControl::KeySpace),
            "Release/repress in same frame restores key");
    InputActionMap map;
    require(
        !map.register_action({Move, InputActionValueType::Axis1D},
                             {{Move, ButtonInputBinding{RawInputControl::KeyD, InputActionComponent::Y}}}),
        "Axis1D rejects Y registration");
    require(map.register_action({Move, InputActionValueType::Axis1D},
                                {{Move, ButtonInputBinding{RawInputControl::KeyD}}}),
            "Valid X registration");
    require(!map.add_binding({Move, ButtonInputBinding{RawInputControl::KeyD, InputActionComponent::Y}}),
            "Axis1D rejects Y addition");
    require(!map.replace_bindings(Move, {{Move, AxisInputBinding{static_cast<RawInputAxis>(-1)}}}) &&
                map.bindings(Move).size() == 1,
            "Invalid axis replacement preserves mapping");
    require(!map.add_binding({Move, AxisInputBinding{static_cast<RawInputAxis>(999)}}),
            "Upper axis bound validated");
    physical.set_axis(static_cast<RawInputAxis>(999), 1);
    require(physical.axis_value(static_cast<RawInputAxis>(-1)) == 0, "Raw state rejects out of range axes");
    InputActionMap two;
    require(two.register_action({Move, InputActionValueType::Axis2D},
                                {{Move, ButtonInputBinding{RawInputControl::KeyD, InputActionComponent::Y}}}),
            "Axis2D accepts Y");
    elysia::tests::InputSnapshotBuilder input;
    input.press(RawInputControl::KeyD, true);
    require(two.resolve(input.take()).frame.axis2d(Move).y == 1, "Axis2D Y binding resolves");
    LocalPlayerRegistry players;
    require(players.bind_source({}, InputSourceId::mouse()).error() == InputBindingError::InvalidPlayer,
            "Invalid player diagnostic");
    require(players.unbind_source(InputSourceId::gamepad(7)).has_value(),
            "Unassigned valid source unbind is idempotent");
    require(players.unbind_source({}).error() == InputBindingError::InvalidSource,
            "Invalid source diagnostic");
}
void test_capture_wheel_and_notifications()
{
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    require(bool(f.world->local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(7))),
            "Own pad");
    auto h = *f.service->create<Local>({ControllerScope::Scene, f.world->control_context().token()},
                                       PrimaryLocalPlayer, movement_map());
    require(f.service->bind_target(h, f.world->control_context(), *a).succeeded(), "Bind local");
    elysia::tests::InputSnapshotBuilder input;
    f.manager.on_input(input.take());
    input.axis(RawInputAxis::GamepadLeftX, 0.19f);
    input.axis(RawInputAxis::GamepadLeftY, 0.19f);
    input.axis(RawInputAxis::GamepadRightX, 0.05f);
    auto captured = input.take();
    captured.capture = InputCapture::Gamepad;
    f.manager.on_input(captured);
    f.tick();
    require(a->commands.back().state.axis2d(Move).is_zero(), "Captured diagonal never reaches controller");
    require(a->commands.back().state.axis1d(Throttle) == 0, "Capture overrides configurable low dead zone");
    KeyboardMouseInputTranslator translator;
    elysia::ui::UiInputRouter ui;
    for (float delta : {0.25f, -0.25f, 0.5f, -0.5f})
    {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_WHEEL;
        event.wheel.y = delta;
        auto raw = translator.translate_event(event).front();
        raw.source = InputSourceId::mouse();
        require(raw.wheel_y == delta && ui.route_event(raw).front().wheel_y == delta,
                "Wheel fraction survives raw and UI routing");
        input.event(raw);
        f.manager.on_input(input.take());
        input.event(raw);
        f.manager.on_input(input.take());
        f.tick();
        require(a->commands.back().deltas.at(Wheel).x == 2 * delta,
                "Zero tick frames accumulate fractional wheel");
        f.tick();
        require(a->commands.back().deltas.empty(), "Wheel consumed exactly once");
    }
    a->reasons.clear();
    auto lost = input.take();
    lost.focus_lost = true;
    f.manager.on_input(lost);
    require(a->reasons == std::vector{InputCancelReason::FocusLost},
            "Multi-source focus loss cancels once with correct reason");
    a->reasons.clear();
    f.world->pause();
    f.manager.on_input(input.take());
    f.manager.on_input(input.take());
    require(a->reasons == std::vector{InputCancelReason::Paused},
            "Pause does not add suppressed/unavailable cancellations");
    f.world->resume();
    f.manager.on_input(input.take());
    f.service->get<Local>(h)->mapped = [&] { f.world->set_all_gameplay_input_blocked(true); };
    input.press(RawInputControl::KeySpace, true);
    f.manager.on_input(input.take());
    f.tick();
    require(a->commands.back().events.empty() && !a->commands.back().state.is_pressed(Fire),
            "Mapped callback cancellation rejects old actions");
}
void test_reentrant_binding()
{
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    auto *b = f.world->create_and_add_object<Actor>();
    auto h = f.driver(*a);
    auto *driver = f.service->get<Driver>(h);
    driver->cancel = [&] { b->destroy(); };
    auto failed = f.service->bind_target(h, f.world->control_context(), *b);
    require(failed.error() == ControllerError::InvalidTarget && !f.service->describe(h)->bound,
            "Post-cancel destroyed target fails safely unbound");
    driver->cancel = {};
    require(f.service->bind_target(h, f.world->control_context(), *a).succeeded(), "Rebind live target");
    auto *c = f.world->create_and_add_object<Actor>();
    driver->cancel = [&] { (void)f.service->remove(h); };
    auto removed = f.service->bind_target(h, f.world->control_context(), *c);
    require(removed.error() == ControllerError::InvalidHandle && !f.service->get(h),
            "Callback removal completes operation without stale commit");
}
void test_pending_results()
{
    static_assert(!std::is_convertible_v<ControllerOperation, bool>);
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    auto *b = f.world->create_and_add_object<Actor>();
    auto *c = f.world->create_and_add_object<Actor>();
    auto h = f.driver(*a);
    auto competitor = *f.service->create<Driver>({ControllerScope::Session, {}});
    std::optional<ControllerOperation> first, second, rejected;
    bool once = false;
    a->callback = [&] {
        if (std::exchange(once, true))
            return;
        first = f.service->bind_target(h, f.world->control_context(), *b);
        require(first->pending(), "Callback request pending");
        rejected = f.service->bind_target(competitor, f.world->control_context(), *b);
        require(rejected->error() == ControllerError::TargetBusy, "Queued target reserved");
        second = f.service->bind_target(h, f.world->control_context(), *c);
        require(first->error() == ControllerError::Superseded && second->pending(),
                "New valid request supersedes old");
        auto *dead = f.world->create_and_add_object<Actor>();
        dead->destroy();
        auto invalid = f.service->bind_target(h, f.world->control_context(), *dead);
        require(invalid.error() == ControllerError::InvalidTarget && second->pending(),
                "Rejected request does not supersede valid pending operation");
    };
    f.tick();
    require(second->succeeded() && c->commands.empty(), "Final request commits after dispatch");
    f.tick();
    require(c->commands.size() == 1, "New target receives only later tick");
    std::swap(b, c);
    // A removed reserved target terminates its ticket even before the queue commits.
    std::optional<ControllerOperation> destroyed;
    b->callback = [&] {
        if (destroyed)
            return;
        destroyed = f.service->bind_target(h, f.world->control_context(), *c);
        c->destroy();
    };
    f.tick();
    require(destroyed->error() == ControllerError::InvalidTarget, "Destroyed pending target fails at commit");
    require(f.service->describe(h)->bound, "Pre-commit validation failure retains old target");
    b->callback = {};
    // Discarding a ticket does not discard the operation.
    auto *d = f.world->create_and_add_object<Actor>();
    bool dropped = false;
    b->callback = [&] {
        if (!std::exchange(dropped, true))
            (void)f.service->bind_target(h, f.world->control_context(), *d);
    };
    f.tick();
    f.tick();
    require(d->commands.size() == 1, "Dropped ticket still commits");
    std::optional<ControllerOperation> ended;
    d->callback = [&] {
        ended = f.service->unbind_target(h);
        require(ended->pending(), "Unbind pending");
        f.service->end_session();
    };
    f.tick();
    require(ended->error() == ControllerError::NoSession, "Session end finishes pending result");
    require(second->succeeded(), "Completed result survives session lifetime");
}
void test_pending_maps_and_unavailability()
{
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    auto h = *f.service->create<Local>({ControllerScope::Scene, f.world->control_context().token()},
                                       PrimaryLocalPlayer, movement_map());
    require(f.service->bind_target(h, f.world->control_context(), *a).succeeded(), "Bind map test");
    elysia::tests::InputSnapshotBuilder input;
    f.manager.on_input(input.take());
    auto map_with = [](RawInputControl key) {
        InputActionMap map;
        require(map.register_action({Fire, InputActionValueType::Button}, {{Fire, ButtonInputBinding{key}}}),
                "Replacement map");
        return map;
    };
    std::optional<ControllerOperation> first, second;
    f.service->get<Local>(h)->mapped = [&] {
        if (first)
            return;
        first = f.service->replace_input_map(h, map_with(RawInputControl::KeyA));
        second = f.service->replace_input_map(h, map_with(RawInputControl::KeyB));
        require(first->pending() && second->pending(), "Maps queued in callback");
    };
    f.manager.on_input(input.take());
    require(first->succeeded() && second->succeeded(), "Both FIFO map requests complete");
    input.press(RawInputControl::KeyB, true);
    f.manager.on_input(input.take());
    f.tick();
    require(a->commands.back().state.is_pressed(Fire), "Final queued mapping active");
    a->reasons.clear();
    a->set_active(false);
    f.manager.on_input(input.take());
    f.tick();
    f.manager.on_input(input.take());
    f.tick();
    require(a->reasons == std::vector{InputCancelReason::Unavailable},
            "Continuous unavailability cancels once");
    a->set_active(true);
    f.manager.on_input(input.take());
    f.tick();
    require(!a->commands.back().state.is_pressed(Fire), "Reactivation waits for release");
    input.press(RawInputControl::KeyB, false);
    input.press(RawInputControl::KeyB, true);
    f.manager.on_input(input.take());
    f.tick();
    require(a->commands.back().state.is_pressed(Fire), "Same-frame release and repress after restore works");
}
void test_context_invalidation_and_target_removal()
{
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    auto *b = f.world->create_and_add_object<Actor>();
    auto h = f.driver(*a);
    std::optional<ControllerOperation> operation;
    a->callback = [&] {
        if (operation)
            return;
        operation = f.service->bind_target(h, f.world->control_context(), *b);
        b->destroy();
        f.world->on_update(0);
        require(operation->error() == ControllerError::InvalidTarget,
                "Object-removing hook invalidates pending request immediately");
        (void)f.world->create_and_add_object<Actor>();
    };
    f.tick();
    require(operation->failed() && f.service->describe(h)->bound,
            "Removed address cannot revive old request");
    a->callback = {};
    World *other = nullptr;
    f.manager.register_game_scene<OtherWorld>(988, &other);
    auto *target = f.world->create_and_add_object<Actor>();
    f.service->get<Driver>(h)->cancel = [&] {
        f.manager.on_scene_request(
            {.type = elysia::scene::SceneRequestType::Switch, .route = {.target = 988}});
        f.manager.on_update(0);
    };
    auto moved = f.service->bind_target(h, f.world->control_context(), *target);
    require(moved.error() == ControllerError::InvalidContext && !f.service->describe(h)->bound,
            "Context deactivation inside cancellation aborts binding");
    f.service->get<Driver>(h)->cancel = {};
}
void test_producer_cancellation()
{
    Fixture f;
    auto *a = f.world->create_and_add_object<Actor>();
    auto h = f.driver(*a);
    auto *driver = f.service->get<Driver>(h);
    driver->produce = [&] {
        f.world->pause();
        f.world->resume();
    };
    f.tick();
    require(a->commands.empty(), "Cancel/resume within producer discards stale intent");
}
} // namespace
int main()
{
    test_gates_and_validation();
    test_capture_wheel_and_notifications();
    test_reentrant_binding();
    test_pending_results();
    test_pending_maps_and_unavailability();
    test_context_invalidation_and_target_removal();
    test_producer_cancellation();
    std::cout << "input regression tests passed\n";
}
