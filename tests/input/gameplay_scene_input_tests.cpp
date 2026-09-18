#include "tests/support/control_scene_fixture.h"
#include "../../game/input/command_view.h"
#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/input/input_system.h"
#include "tests/physics/physics_test_support.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"
#include <iostream>
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
        presses += example::input::CommandView(c).press_count(example::input::actions::Jump);
        move = example::input::CommandView(c).move();
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
    void reset() override
    {
        reset_input_routing();
    }
    void step(double dt = 1.0 / 60)
    {
        on_update(dt);
    }
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
    scene.on_input(input.snapshot());
    scene.step(0);
    require(a->presses == 0, "No tick must retain events");
    scene.step();
    require(a->presses == 1 && a->move.x > 0 && b->move.x < 0,
            "Ordered tap and independent movement must reach targets");
    scene.step(3.0 / 60);
    require(a->presses == 1 && a->ticks == 4, "Catchup ticks must not repeat edges");
    input.begin_frame();
    for (int i = 0; i < 3; ++i)
    {
        key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
        key(input, SDL_EVENT_KEY_UP, SDLK_SPACE);
    }
    scene.on_input(input.snapshot());
    scene.step();
    require(a->presses == 4, "Multiple taps in one frame must survive");
    scene.set_all_gameplay_input_blocked(true);
    input.begin_frame();
    scene.on_input(input.snapshot());
    scene.step();
    require(a->move.is_zero(), "Blocking must cancel held movement");
    scene.set_all_gameplay_input_blocked(false);
    input.begin_frame();
    scene.on_input(input.snapshot());
    scene.step();
    require(a->move.is_zero(), "Held controls must require release");
    input.begin_frame();
    key(input, SDL_EVENT_KEY_UP, SDLK_D);
    scene.on_input(input.snapshot());
    scene.step();
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_D);
    scene.on_input(input.snapshot());
    scene.step();
    require(a->move.x > 0, "Fresh press must work");
    scene.pause();
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    scene.on_input(input.snapshot());
    scene.resume();
    scene.step();
    require(a->presses == 4, "Paused commands must not accumulate");
    auto *c = scene.create_and_add_object<Actor>();
    require(bool(service->bind_target(first,scene.control_context(),*c).succeeded()), "Rebind target");
    input.begin_frame();
    scene.on_input(input.snapshot());
    scene.step();
    require(c->move.is_zero() && c->presses == 0, "New target must not inherit held input");
    c->destroy();
    scene.step();
    input.begin_frame();
    scene.on_input(input.snapshot());
    scene.step();
    // No-body fixed step callbacks are part of normal scenes, not just physics participants.
    require(b->ticks > 4, "Non-physical actors must receive ticks");
    scene.reset_input_routing();
    scene.step();
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
    merged.on_input(mixed.snapshot());
    merged.step();
    require(target->move.x == 0, "Opposing device contributions cancel within one player");
    mixed.begin_frame();
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = 7;
    mixed.process_event(removed);
    merged.on_input(mixed.snapshot());
    merged.step();
    require(target->move.x > 0, "Pad removal preserves keyboard movement");
    // Overflow cancels rather than executing a partial list of actions.
    mixed.begin_frame();
    for (int i = 0; i < 600; ++i)
    {
        key(mixed, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
        key(mixed, SDL_EVENT_KEY_UP, SDLK_SPACE);
    }
    merged.on_input(mixed.snapshot());
    merged.step();
    require(target->presses == 0 && target->move.is_zero(), "Overflow must cancel all queued actions");
    // Scene reset must detach targets even when the object remains cached.
    auto ticks = target->ticks;
    service->unbind_target(merged_controller).succeeded();
    merged.step();
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
    world.advance(3.0 / 60, [&](double) { ++hook_count; });
    require(hook_count == 3 && participant_count == 3, "One callback per physics step");
    world.unregister_object(handle);
    // Dropped catchup steps do not replay edges.
    require(bool(service->bind_target(merged_controller,merged.control_context(),*target).succeeded()), "Rebind cached target");
    mixed.begin_frame();
    key(mixed, SDL_EVENT_KEY_UP, SDLK_D);
    merged.on_input(mixed.snapshot());
    mixed.begin_frame();
    key(mixed, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    key(mixed, SDL_EVENT_KEY_UP, SDLK_SPACE);
    merged.on_input(mixed.snapshot());
    ticks = target->ticks;
    merged.step(10);
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
    twins.on_input(pads.snapshot());
    twins.step();
    require(left->move.x > 0 && right->move.x < 0,
            "Two gamepads must drive different player command streams concurrently");
    left->set_active(false);
    pads.begin_frame();
    twins.on_input(pads.snapshot());
    twins.step();
    left->set_active(true);
    pads.begin_frame();
    twins.on_input(pads.snapshot());
    twins.step();
    require(left->move.is_zero() && right->move.x < 0,
            "Inactive target restoration must require neutral without blocking another player");
    axis.gaxis.which = 8;
    axis.gaxis.value = 0;
    pads.begin_frame();
    pads.process_event(axis);
    twins.on_input(pads.snapshot());
    twins.step();
    axis.gaxis.value = 32767;
    pads.begin_frame();
    pads.process_event(axis);
    twins.on_input(pads.snapshot());
    twins.step();
    require(left->move.x > 0, "Reactivated target resumes only after its own control returns to neutral");
    twins_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<TestScene> rebuilt_fixture;
    auto& rebuilt=*rebuilt_fixture.scene;
    auto *fresh = rebuilt.create_and_add_object<Actor>();
    require(bool(rebuilt.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(8))), "Rebuilt gamepad binding succeeds");
    auto fresh_controller=elysia::tests::local_controller(rebuilt,PrimaryLocalPlayer);
    require(service->bind_target(fresh_controller,rebuilt.control_context(),*fresh).succeeded(), "Test controller binding completes successfully");
    pads.begin_frame();
    rebuilt.on_input(pads.snapshot());
    rebuilt.step();
    require(fresh->move.is_zero(),
            "A newly constructed scene must not inherit a held control from the previous scene");
    std::cout << "player routing tests passed\n";
}
