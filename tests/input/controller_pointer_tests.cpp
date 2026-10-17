#define SDL_MAIN_HANDLED
#include "tests/support/control_scene_fixture.h"
#include "tests/support/input_snapshot_builder.h"
#include "engine/input/input_system.h"
#include "engine/ui/core/ui_element.h"
#include "engine/ui/input/contracts/ui_input_event_receiver.h"

#include <iostream>
#include <vector>

using namespace elysia::gameplay;
using namespace elysia::input;
using elysia::tests::require;
using elysia::core::Vector2;

namespace
{
constexpr double kStep = 1.0 / 60.0;

class PointerScene final : public GameplayScene
{
  public:
    explicit PointerScene(PointerScene **slot)
    {
        *slot = this;
        input_router().set_auto_claim_ui_gamepad(false);
    }
    void on_enter(const elysia::scene::ScenePayload &) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class Actor final : public elysia::core::GameObject, public ControlCommandReceiver
{
  public:
    Actor() : GameObject(elysia::core::DepthLayer::Character) {}
    void on_control_command(const ControlCommand &command, double) override { commands.push_back(command); }
    void on_control_cancelled(InputCancelReason) override { ++cancellations; }
    std::vector<ControlCommand> commands;
    int cancellations = 0;
};

class PointerSink final : public elysia::ui::UiElement, public elysia::ui::UiInputEventReceiver
{
  public:
    bool on_ui_input_event(const elysia::ui::UiInputEvent &event) override
    {
        using elysia::ui::UiInputEventType;
        return event.mouse_x == 900 &&
               (event.type == UiInputEventType::MouseMoved ||
                event.type == UiInputEventType::PointerPressed ||
                event.type == UiInputEventType::PointerReleased);
    }
};

class PointerCapture final : public elysia::ui::UiElement
{
  public:
    InputCapture input_capture() const noexcept override { return InputCapture::Pointer; }
};

struct Harness
{
    elysia::tests::ControlSceneFixture<PointerScene> fixture;
    PointerScene &scene = *fixture.scene;
    ControllerService &service = *ControllerService::instance();
    elysia::tests::InputSnapshotBuilder devices;
    Actor *actor = scene.create_and_add_object<Actor>();
    ControllerHandle controller = bind(PrimaryLocalPlayer, *actor);

    Harness()
    {
        input();
        tick();
    }
    ControllerHandle bind(LocalPlayerId player, Actor &target)
    {
        // Absolute position does not require a pointer action binding.
        auto result = service.create<LocalPlayerController>(
            {ControllerScope::Scene, scene.control_context().token()}, player, InputActionMap{});
        require(bool(result), "Create pointer controller with an empty action map");
        require(service.bind_target(*result, scene.control_context(), target).succeeded(), "Bind pointer target");
        return *result;
    }
    void input() { fixture.manager.on_input(devices.take()); }
    void tick(double delta = kStep) { fixture.manager.on_update(delta); }
    void move(int x, int y)
    {
        devices.event({.type = RawInputEventType::MouseMoved, .mouse_x = x, .mouse_y = y});
    }
    void button(bool pressed, int x, int y)
    {
        devices.event({.control = RawInputControl::MouseLeft,
                       .type = pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased,
                       .mouse_button = SDL_BUTTON_LEFT, .mouse_x = x, .mouse_y = y});
    }
    const ControlCommand &last() const { return actor->commands.back(); }
    void seed()
    {
        move(812, 302);
        input();
        tick();
        require(last().pointer_position == Vector2{812, 302} && last().pointer_updated,
                "Fresh routed mouse motion restores pointer input");
    }
    void require_cleared() const
    {
        require(!last().pointer_position && !last().pointer_updated && last().input_device == InputDevice::Unknown,
                "Cancellation clears pointer position, pending update and device");
    }
};

void test_buffering_and_samples()
{
    Harness h;
    h.require_cleared();
    const auto count = h.actor->commands.size();
    h.move(400, 200);
    h.input();
    h.move(1800, -45);
    h.input();
    h.input();
    h.tick(0);
    require(h.actor->commands.size() == count, "Zero fixed steps retain samples without delivery");
    h.tick();
    require(h.last().pointer_position == Vector2{1800, -45} && h.last().pointer_updated &&
                h.last().input_device == InputDevice::Mouse,
            "Latest logical position survives idle frames without clamping to action axes or viewport");
    h.tick(3 * kStep);
    require(h.actor->commands.size() == count + 4, "Catch-up delivers one command per fixed step");
    for (auto index = count + 1; index < h.actor->commands.size(); ++index)
        require(h.actor->commands[index].pointer_position == Vector2{1800, -45} &&
                    !h.actor->commands[index].pointer_updated,
                "Catch-up retains position without replaying the update marker");

    h.button(true, 640, 360);
    h.input();
    h.tick();
    require(h.last().pointer_position == Vector2{640, 360} && h.last().pointer_updated,
            "A mouse press supplies position without a motion event");
    h.button(false, 700, 400);
    h.input();
    h.tick();
    require(h.last().pointer_position == Vector2{700, 400} && h.last().pointer_updated,
            "A mouse release carries its own position sample");
    h.devices.event({.type = RawInputEventType::MouseWheel, .mouse_x = 0, .mouse_y = 0, .wheel_y = 1});
    h.input();
    h.tick();
    require(h.last().pointer_position == Vector2{700, 400} && !h.last().pointer_updated,
            "Wheel input and idle source caches do not manufacture a new position");
}

void test_consumption_order()
{
    Harness h;
    h.seed();
    h.scene.create_and_add_object<PointerSink>();
    h.move(500, 100);
    h.input(); // Pending position from an earlier zero-tick frame.
    h.move(600, 100);
    h.move(900, 100);
    h.input();
    h.tick();
    require(!h.last().pointer_position && !h.last().pointer_updated,
            "Later UI consumption clears both pending and earlier same-frame positions");
    h.move(900, 100);
    h.move(700, 100);
    h.input();
    h.tick();
    require(h.last().pointer_position == Vector2{700, 100} && h.last().pointer_updated,
            "A later unconsumed sample can restore position in the same snapshot");
    h.button(true, 900, 100);
    h.input();
    h.tick();
    require(!h.last().pointer_position && !h.last().pointer_updated,
            "Consumed mouse presses invalidate cached gameplay position");
}

void test_capture_and_cancellation()
{
    Harness h;
    h.seed();
    h.move(1000, 500);
    h.input();
    auto captured = h.devices.take();
    captured.capture = InputCapture::Pointer;
    h.fixture.manager.on_input(captured);
    h.tick();
    h.require_cleared();
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
    auto *capture = h.scene.create_and_add_object<PointerCapture>();
    h.move(1000, 500);
    h.input();
    h.tick();
    h.require_cleared();
    capture->set_active(false);
    capture->set_visible(false);
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
    auto unfocused = h.devices.take();
    unfocused.focus_lost = true;
    h.fixture.manager.on_input(unfocused);
    h.tick();
    h.require_cleared();
    h.input();
    h.seed();
    h.scene.pause();
    h.scene.resume();
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
    h.scene.set_all_gameplay_input_blocked(true);
    h.input();
    h.tick();
    h.require_cleared();
    h.scene.set_all_gameplay_input_blocked(false);
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
}

void test_player_ownership_and_devices()
{
    Harness h;
    const auto player = h.scene.local_players().create_player();
    require(bool(h.scene.local_players().bind_source(player, InputSourceId::gamepad(7))), "Bind second player's pad");
    auto *other = h.scene.create_and_add_object<Actor>();
    h.bind(player, *other);
    h.input();
    h.tick();
    h.devices.press(RawInputControl::GamepadSouth, true);
    h.move(812, 302);
    h.devices.press(RawInputControl::KeyD, true);
    h.input();
    h.tick();
    require(h.last().pointer_position == Vector2{812, 302} && h.last().input_device == InputDevice::Keyboard,
            "Pointer position is independent of the last routed device");
    require(!other->commands.back().pointer_position && other->commands.back().input_device == InputDevice::Gamepad,
            "Another player's device and pointer cannot leak into a command");
    auto idle = h.devices.take();
    for (auto &source : idle.sources)
        source.frame.active_device = InputDevice::Gamepad;
    h.fixture.manager.on_input(idle);
    h.tick();
    require(h.last().input_device == InputDevice::Keyboard && !h.last().pointer_updated,
            "Idle source ordering cannot overwrite the last routed device");
    require(bool(h.scene.local_players().transfer_source(player, InputSourceId::mouse())), "Transfer pointer ownership");
    h.input();
    h.tick();
    h.require_cleared();
    require(!other->commands.back().pointer_position, "New pointer owner cannot inherit the old cached position");
    h.input();
    h.move(450, 250);
    h.input();
    h.tick();
    require(!h.last().pointer_position && other->commands.back().pointer_position == Vector2{450, 250},
            "Fresh pointer samples reach only the new owner");
}

void test_binding_and_map_reset()
{
    Harness h;
    h.seed();
    require(h.service.replace_input_map(h.controller, InputActionMap{}).succeeded(), "Replace pointer action map");
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
    require(h.service.unbind_target(h.controller).succeeded(), "Unbind pointer target");
    auto *replacement = h.scene.create_and_add_object<Actor>();
    require(h.service.bind_target(h.controller, h.scene.control_context(), *replacement).succeeded(), "Rebind pointer target");
    h.actor = replacement;
    h.input();
    h.tick();
    h.require_cleared();
    h.seed();
}

void test_sdl_input_path()
{
    Harness h;
    InputSystem input;
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 321;
    event.motion.y = 123;
    input.begin_frame();
    input.process_event(event);
    h.fixture.manager.on_input(input.snapshot());
    h.tick();
    require(h.last().pointer_position == Vector2{321, 123} && h.last().pointer_updated,
            "SDL motion reaches the command through InputSystem and scene routing");
    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 640;
    event.button.y = 360;
    input.begin_frame();
    input.process_event(event);
    h.fixture.manager.on_input(input.snapshot());
    h.tick();
    require(h.last().pointer_position == Vector2{640, 360} && h.last().pointer_updated,
            "SDL button position reaches the command without separate motion");
}
} // namespace

int main()
{
    test_buffering_and_samples();
    test_consumption_order();
    test_capture_and_cancellation();
    test_player_ownership_and_devices();
    test_binding_and_map_reset();
    test_sdl_input_path();
    std::cout << "controller pointer tests passed\n";
}
