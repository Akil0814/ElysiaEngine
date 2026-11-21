#include "tests/support/control_scene_fixture.h"
#include "game/gameplay/control/command_view.h"
#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/input/input_system.h"
#include "tests/physics/physics_test_support.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"
#include "tests/support/input_snapshot_builder.h"
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace elysia::input;
using namespace elysia::gameplay;
using elysia::tests::require;
class Actor : public elysia::core::GameObject, public ControlCommandReceiver
{
  public:
    Actor() : GameObject(elysia::core::DepthLayer::Character)
    {
    }
    void on_control_command(const ControlCommand &c, double) override
    {
        ++ticks;
        presses += example::gameplay::CommandView(c).press_count(example::input::actions::Jump);
        move = example::gameplay::CommandView(c).move();
        last = c;
    }
    void on_control_cancelled(InputCancelReason) override
    {
        ++cancels;
        move = {};
    }
    int ticks = 0, presses = 0, cancels = 0;
    elysia::core::Vector2 move;
    ControlCommand last;
};
class TestScene : public GameplayScene
{
  public:
    explicit TestScene(TestScene** slot) { *slot=this; }
    void on_enter(const elysia::scene::ScenePayload &) override
    {
    }
    void on_exit() override
    {
    }
    void on_reset() override {}
};
void key(InputSystem &input, Uint32 type, SDL_Keycode code)
{
    SDL_Event e{};
    e.type = type;
    e.key.key = code;
    input.process_event(e);
}
void pad(InputSystem &input, Uint32 type, Uint8 code)
{
    SDL_Event e{};
    e.type = type;
    e.gbutton.which = 7;
    e.gbutton.button = code;
    input.process_event(e);
}

namespace
{
using namespace elysia::scene;
using elysia::tests::InputSnapshotBuilder;
namespace actions = example::input::actions;

constexpr SceneKey FirstRoom = 995;
constexpr SceneKey SecondRoom = 996;
constexpr SceneKey FailingRoom = 997;
constexpr double Tick = 1.0 / 60.0;

struct KeyboardCapture : elysia::ui::UiElement
{
    InputCapture input_capture() const noexcept override
    {
        return enabled ? InputCapture::Keyboard : InputCapture::None;
    }
    bool enabled = false;
};

struct HandoffScene : GameplayScene
{
    explicit HandoffScene(HandoffScene** slot) { *slot = this; }
    void on_enter(const ScenePayload& payload) override
    {
        if (const auto* fail = try_scene_payload<bool>(payload); fail && *fail)
            throw std::runtime_error("Handoff test enter failure");
        actor = create_and_add_object<Actor>();
        capture = create_and_add_object<KeyboardCapture>();
        controller = elysia::tests::local_controller(*this, PrimaryLocalPlayer);
        require(ControllerService::instance()->bind_target(controller, control_context(), *actor).succeeded(),
                "Bind local controller during scene entry");
    }
    void on_exit() override
    {
        actor->destroy();
        capture->destroy();
    }
    void on_reset() override {}
    void on_shortcuts(const RawInputFrame& frame, const std::vector<RawInputEvent>& events) override
    {
        shortcuts = frame.state;
        for (const auto& event : events)
            if (event.control == consume_control && event.type == RawInputEventType::ControlPressed)
                consume_input(event);
        if (input_route)
        {
            request_scene_switch(*input_route);
            input_route.reset();
        }
    }

    Actor* actor = nullptr;
    KeyboardCapture* capture = nullptr;
    ControllerHandle controller;
    RawInputState shortcuts;
    RawInputControl consume_control = RawInputControl::None;
    std::optional<SceneRoute> input_route;
};

struct HandoffFixture
{
    HandoffFixture()
    {
        manager.initialize(context, [](const SceneBoundaryFailure&) { return SceneRoute{.target = SecondRoom}; });
        auto* service = ControllerService::instance();
        if (service->session_active())
            service->end_session();
        require(bool(service->begin_session()), "Begin handoff test session");
        manager.register_game_scene<HandoffScene>(FirstRoom, &first);
        manager.register_game_scene<HandoffScene>(SecondRoom, &second);
        manager.register_game_scene<HandoffScene>(FailingRoom, &failing);
        manager.start({.target = FirstRoom});
        require(bool(manager.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(7))),
                "Assign handoff test gamepad");
        frame();
    }

    void frame()
    {
        manager.on_input(devices.take());
        manager.on_update(Tick);
    }
    void switch_after_update(SceneRoute route)
    {
        manager.on_scene_request({.type = SceneRequestType::Switch, .route = std::move(route)});
        manager.on_update(Tick);
    }

    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context{nullptr, registry, 1280, 720};
    HandoffScene* first = nullptr;
    HandoffScene* second = nullptr;
    HandoffScene* failing = nullptr;
    SceneManager manager;
    InputSnapshotBuilder devices;
};

void test_held_input_handoff()
{
    HandoffFixture fixture;
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.devices.press(RawInputControl::KeySpace, true);
    fixture.frame();
    fixture.switch_after_update({.target = FirstRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    auto* room = fixture.first;
    require(room->actor->ticks == 1 && room->actor->move.x > 0.f,
            "Same-instance reuse must hand off keyboard movement before the first tick");
    require(room->actor->last.state.is_pressed(actions::Jump) && room->actor->presses == 0 &&
            !room->actor->last.state.is_just_pressed(actions::Jump) && room->actor->last.events.empty(),
            "Held actions must establish a baseline without emitting new edges");
    fixture.frame();
    require(!room->shortcuts.is_pressed(RawInputControl::KeySpace) &&
            !room->shortcuts.is_pressed(RawInputControl::KeyD),
            "Gameplay handoff must keep held keys isolated from shortcuts");
    fixture.devices.press(RawInputControl::KeySpace, false);
    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.devices.press(RawInputControl::GamepadDPadUp, true);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .reload_mode = SceneReloadMode::Reset,
                                 .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(fixture.second->actor->move.y < 0.f, "Reset must hand off D-pad state");

    fixture.devices.press(RawInputControl::GamepadDPadUp, false);
    fixture.devices.axis(RawInputAxis::GamepadLeftX, 0.75f);
    fixture.frame();
    const auto analog_move = fixture.second->actor->move;
    fixture.switch_after_update({.target = SecondRoom, .reload_mode = SceneReloadMode::Recreate,
                                 .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(analog_move.x > 0.f && fixture.second->actor->move == analog_move,
            "Recreate must hand off the analog axis value");
    fixture.switch_after_update({.target = FirstRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.devices.axis(RawInputAxis::GamepadLeftX, 0.f);
    fixture.frame();
    require(fixture.first->actor->move.is_zero(), "Centering the stick on the first routed frame must stop movement");

    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.devices.press(RawInputControl::KeySpace, true);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.devices.press(RawInputControl::KeySpace, false);
    fixture.devices.press(RawInputControl::KeySpace, true);
    fixture.frame();
    require(fixture.second->actor->move.is_zero() && fixture.second->actor->presses == 1 &&
            fixture.second->shortcuts.is_pressed(RawInputControl::KeySpace),
            "First-frame release must stop movement and a real re-press must produce a new edge");
    fixture.switch_after_update({.target = FirstRoom});
    fixture.frame();
    require(!fixture.first->actor->last.state.is_pressed(actions::Jump), "Default route must suppress held actions");
}

void test_handoff_respects_input_barriers()
{
    HandoffFixture fixture;
    fixture.first->consume_control = RawInputControl::KeySpace;
    fixture.devices.press(RawInputControl::KeySpace, true);
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(fixture.second->actor->move.x > 0.f && !fixture.second->actor->last.state.is_pressed(actions::Jump),
            "A source shortcut's consumed key must stay isolated during handoff");

    fixture.second->capture->enabled = true;
    fixture.switch_after_update({.target = FirstRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(fixture.first->actor->move.is_zero(), "Source capture enabled after routing must still bar handoff");
    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.frame();
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.second->capture->enabled = true;
    fixture.frame();
    fixture.second->capture->enabled = false;
    fixture.frame();
    require(fixture.second->actor->move.is_zero(), "Destination UI capture must latch handed-off keys until release");

    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.frame();
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    fixture.switch_after_update({.target = FirstRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    auto unfocused = fixture.devices.take();
    unfocused.focus_lost = true;
    fixture.manager.on_input(unfocused);
    fixture.frame();
    require(fixture.first->actor->move.is_zero(), "Focus loss must prevent a pending handoff");

    fixture.devices.axis(RawInputAxis::GamepadLeftX, 0.75f);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    auto& players = fixture.manager.local_players();
    auto second_player = players.create_player();
    require(bool(players.transfer_source(second_player, InputSourceId::gamepad(7))), "Transfer gamepad");
    auto controller = elysia::tests::local_controller(*fixture.second, second_player);
    auto* second_actor = fixture.second->create_and_add_object<Actor>();
    require(ControllerService::instance()->bind_target(controller, fixture.second->control_context(), *second_actor)
                .succeeded(), "Bind new gamepad owner");
    fixture.frame();
    require(second_actor->move.is_zero(), "A transferred gamepad must not hand off input to its new owner");
}

void test_handoff_respects_keyboard_partition_and_initial_rebind()
{
    HandoffFixture fixture;
    auto& players = fixture.manager.local_players();
    auto partition = players.create_partition("Handoff WASD", example::input::keyboard_keys(example::input::KeyboardScheme::Wasd));
    require(bool(partition), "Create replacement keyboard partition");
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    require(bool(players.bind_keyboard(PrimaryLocalPlayer, *partition)), "Switch keyboard partition before routing");
    fixture.frame();
    require(fixture.second->actor->move.is_zero(), "A changed keyboard partition must suppress the old hold");

    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.frame();
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    require(fixture.second->actor->move.x > 0.f, "Fresh movement under the new keyboard partition");
    fixture.switch_after_update({.target = FirstRoom, .reload_mode = SceneReloadMode::Recreate,
                                 .held_input_on_enter = HeldInputOnEnter::Preserve});
    auto* actor = fixture.first->create_and_add_object<Actor>();
    require(ControllerService::instance()
                ->bind_target(fixture.first->controller, fixture.first->control_context(), *actor).succeeded(),
            "Rebind local controller before the destination's first input");
    fixture.frame();
    require(actor->move.is_zero(), "A rebind before the first input must still require release");
}

void test_handoff_timing_and_failure()
{
    HandoffFixture fixture;
    fixture.first->input_route = SceneRoute{.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve};
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.devices.press(RawInputControl::KeySpace, true);
    fixture.frame();
    require(fixture.manager.current_scene_key() == SecondRoom && fixture.second->actor->move.is_zero(),
            "An input-phase switch must not replay the outgoing input before the first routed frame");
    fixture.frame();
    require(fixture.second->actor->move.x > 0.f && fixture.second->actor->presses == 0,
            "The next normal input frame must pick up state without replaying the opening event");

    fixture.switch_after_update({.target = FirstRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.switch_after_update({.target = SecondRoom, .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(fixture.second->actor->move.is_zero(), "A scene left before routing must not forward a pending handoff");

    fixture.devices.press(RawInputControl::KeyD, false);
    fixture.frame();
    fixture.devices.press(RawInputControl::KeyD, true);
    fixture.frame();
    require(fixture.second->actor->move.x > 0.f, "Fresh press before failed handoff");
    fixture.switch_after_update({.target = FailingRoom, .payload = true,
                                 .held_input_on_enter = HeldInputOnEnter::Preserve});
    fixture.frame();
    require(fixture.manager.current_scene_key() == SecondRoom && fixture.second->actor->move.is_zero(),
            "Failure recovery must enter with the recovery route's default suppression");
}
} // namespace

int main()
{
    auto* service=ControllerService::instance();
    elysia::tests::ControlSceneFixture<TestScene> scene_fixture;
    auto& scene=*scene_fixture.scene;
    InputSystem input;
    auto *a = scene.create_and_add_object<Actor>();
    auto *b = scene.create_and_add_object<Actor>();
    auto p2 = scene.local_players().create_player();
    require(bool(scene.local_players().bind_source(p2, InputSourceId::gamepad(7))), "Bind second player");
    require(bool(!scene.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(7))),
            "A source cannot own two players");
    auto first=elysia::tests::local_controller(scene,PrimaryLocalPlayer);
    auto second=elysia::tests::local_controller(scene,p2);
    require(service->bind_target(first,scene.control_context(),*a).succeeded() && service->bind_target(second,scene.control_context(),*b).succeeded(),
            "Bind independent targets");
    require(!service->bind_target(second,scene.control_context(),*a).succeeded(), "Target ownership must be exclusive");
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_D);
    key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    key(input, SDL_EVENT_KEY_UP, SDLK_SPACE);
    pad(input, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(0);
    require(a->presses == 0, "No tick must retain events");
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->presses == 1 && a->move.x > 0 && b->move.x < 0,
            "Ordered tap and independent movement must reach targets");
    scene_fixture.manager.on_update(3.0 / 60);
    require(a->presses == 1 && a->ticks == 4, "Catchup ticks must not repeat edges");
    input.begin_frame();
    for (int i = 0; i < 3; ++i)
    {
        key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
        key(input, SDL_EVENT_KEY_UP, SDLK_SPACE);
    }
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->presses == 4, "Multiple taps in one frame must survive");
    scene.set_all_gameplay_input_blocked(true);
    input.begin_frame();
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->move.is_zero(), "Blocking must cancel held movement");
    scene.set_all_gameplay_input_blocked(false);
    input.begin_frame();
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->move.is_zero(), "Held controls must require release");
    input.begin_frame();
    key(input, SDL_EVENT_KEY_UP, SDLK_D);
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_D);
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->move.x > 0, "Fresh press must work");
    scene.pause();
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    scene_fixture.manager.on_input(input.snapshot());
    scene.resume();
    scene_fixture.manager.on_update(1.0 / 60);
    require(a->presses == 4, "Paused commands must not accumulate");
    auto *c = scene.create_and_add_object<Actor>();
    require(bool(service->bind_target(first,scene.control_context(),*c).succeeded()), "Rebind target");
    input.begin_frame();
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    require(c->move.is_zero() && c->presses == 0, "New target must not inherit held input");
    c->destroy();
    scene_fixture.manager.on_update(1.0 / 60);
    input.begin_frame();
    scene_fixture.manager.on_input(input.snapshot());
    scene_fixture.manager.on_update(1.0 / 60);
    // No-body fixed step callbacks are part of normal scenes, not just physics participants.
    require(b->ticks > 4, "Non-physical actors must receive ticks");
    scene_fixture.manager.on_scene_request({
        .type = elysia::scene::SceneRequestType::Switch,
        .route = {.target = 998, .reload_mode = elysia::scene::SceneReloadMode::Reset}});
    scene_fixture.manager.on_update(0);
    // Removing only a pad must leave the same player's keyboard source usable.
    scene_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<TestScene> merged_fixture;
    auto& merged=*merged_fixture.scene;
    InputSystem mixed;
    auto *target = merged.create_and_add_object<Actor>();
    require(bool(merged.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(7))),
            "P1 can own keyboard plus pad");
    auto merged_controller=elysia::tests::local_controller(merged,PrimaryLocalPlayer);
    require(bool(service->bind_target(merged_controller,merged.control_context(),*target).succeeded()), "Bind merged player");
    mixed.begin_frame();
    key(mixed, SDL_EVENT_KEY_DOWN, SDLK_D);
    pad(mixed, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    merged_fixture.manager.on_input(mixed.snapshot());
    merged_fixture.manager.on_update(1.0 / 60);
    require(target->move.x == 0, "Opposing device contributions cancel within one player");
    mixed.begin_frame();
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = 7;
    mixed.process_event(removed);
    merged_fixture.manager.on_input(mixed.snapshot());
    merged_fixture.manager.on_update(1.0 / 60);
    require(target->move.x > 0, "Pad removal preserves keyboard movement");
    // Overflow cancels rather than executing a partial list of actions.
    mixed.begin_frame();
    for (int i = 0; i < 600; ++i)
    {
        key(mixed, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
        key(mixed, SDL_EVENT_KEY_UP, SDLK_SPACE);
    }
    merged_fixture.manager.on_input(mixed.snapshot());
    merged_fixture.manager.on_update(1.0 / 60);
    require(target->presses == 0 && target->move.is_zero(), "Overflow must cancel all queued actions");
    // Scene reset must detach targets even when the object remains cached.
    auto ticks = target->ticks;
    service->unbind_target(merged_controller).succeeded();
    merged_fixture.manager.on_update(1.0 / 60);
    require(target->ticks == ticks, "Explicit unbinding detaches the target");
    // Step hooks run before physics participants, once per simulated step.
    elysia::physics::PhysicsWorld world;
    Probe probe;
    int hook_count = 0, participant_count = 0;
    probe.tick = [&] {
        ++participant_count;
        require(hook_count == participant_count, "Input step hook precedes physics participant");
    };
    auto handle = probe.add(world);
    require(handle.is_valid(), "Register physics participant");
    elysia::scene::FixedStepRuntime physics_clock({});
    physics_clock.advance(3.0 / 60, [&](std::uint64_t, double delta) {
        ++hook_count;
        world.step(delta);
    });
    world.finalize_frame(physics_clock.stats().interpolation_alpha);
    require(hook_count == 3 && participant_count == 3, "One callback per physics step");
    world.unregister_object(handle);
    // Dropped catchup steps do not replay edges.
    require(bool(service->bind_target(merged_controller,merged.control_context(),*target).succeeded()), "Rebind cached target");
    mixed.begin_frame();
    key(mixed, SDL_EVENT_KEY_UP, SDLK_D);
    merged_fixture.manager.on_input(mixed.snapshot());
    mixed.begin_frame();
    key(mixed, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    key(mixed, SDL_EVENT_KEY_UP, SDLK_SPACE);
    merged_fixture.manager.on_input(mixed.snapshot());
    ticks = target->ticks;
    merged_fixture.manager.on_update(10);
    require(target->ticks - ticks == 8 && target->presses == 1,
            "Catchup limit must not replay dropped steps");

    merged_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<TestScene> twins_fixture;
    auto& twins=*twins_fixture.scene;
    InputSystem pads;
    const auto twin_player = twins.local_players().create_player();
    require(bool(twins.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(8))),
            "First pad binding");
    require(bool(twins.local_players().bind_source(twin_player, InputSourceId::gamepad(7))), "Second pad binding");
    auto *left = twins.create_and_add_object<Actor>();
    auto *right = twins.create_and_add_object<Actor>();
    auto left_controller=elysia::tests::local_controller(twins,PrimaryLocalPlayer);
    auto right_controller=elysia::tests::local_controller(twins,twin_player);
    require(service->bind_target(left_controller,twins.control_context(),*left).succeeded(), "Test controller binding completes successfully");
    require(service->bind_target(right_controller,twins.control_context(),*right).succeeded(), "Test controller binding completes successfully");
    pads.begin_frame();
    SDL_Event axis{};
    axis.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    axis.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTX;
    axis.gaxis.which = 8;
    axis.gaxis.value = 32767;
    pads.process_event(axis);
    axis.gaxis.which = 7;
    axis.gaxis.value = -32767;
    pads.process_event(axis);
    twins_fixture.manager.on_input(pads.snapshot());
    twins_fixture.manager.on_update(1.0 / 60);
    require(left->move.x > 0 && right->move.x < 0,
            "Two gamepads must drive different player command streams concurrently");
    left->set_active(false);
    pads.begin_frame();
    twins_fixture.manager.on_input(pads.snapshot());
    twins_fixture.manager.on_update(1.0 / 60);
    left->set_active(true);
    pads.begin_frame();
    twins_fixture.manager.on_input(pads.snapshot());
    twins_fixture.manager.on_update(1.0 / 60);
    require(left->move.is_zero() && right->move.x < 0,
            "Inactive target restoration must require neutral without blocking another player");
    axis.gaxis.which = 8;
    axis.gaxis.value = 0;
    pads.begin_frame();
    pads.process_event(axis);
    twins_fixture.manager.on_input(pads.snapshot());
    twins_fixture.manager.on_update(1.0 / 60);
    axis.gaxis.value = 32767;
    pads.begin_frame();
    pads.process_event(axis);
    twins_fixture.manager.on_input(pads.snapshot());
    twins_fixture.manager.on_update(1.0 / 60);
    require(left->move.x > 0, "Reactivated target resumes only after its own control returns to neutral");
    twins_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<TestScene> rebuilt_fixture;
    auto& rebuilt=*rebuilt_fixture.scene;
    auto *fresh = rebuilt.create_and_add_object<Actor>();
    require(bool(rebuilt.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(8))), "Rebuilt gamepad binding succeeds");
    auto fresh_controller=elysia::tests::local_controller(rebuilt,PrimaryLocalPlayer);
    require(service->bind_target(fresh_controller,rebuilt.control_context(),*fresh).succeeded(), "Test controller binding completes successfully");
    pads.begin_frame();
    rebuilt_fixture.manager.on_input(pads.snapshot());
    rebuilt_fixture.manager.on_update(1.0 / 60);
    require(fresh->move.is_zero(),
            "A newly constructed scene must not inherit a held control from the previous scene");
    rebuilt_fixture.manager.shutdown();
    test_held_input_handoff();
    test_handoff_respects_input_barriers();
    test_handoff_respects_keyboard_partition_and_initial_rebind();
    test_handoff_timing_and_failure();
    std::cout << "player routing tests passed\n";
}
