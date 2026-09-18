#include "tests/support/control_scene_fixture.h"
#include "../../game/input/command_view.h"
#define SDL_MAIN_HANDLED
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/input/input_system.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "engine/ui/widgets/ui_button.h"
#include "tests/support/test_assertions.h"
using namespace elysia::input;
using namespace elysia::gameplay;
using elysia::tests::require;
class CaptureActor final : public elysia::core::GameObject, public ControlCommandReceiver
{
  public:
    CaptureActor() : GameObject(elysia::core::DepthLayer::Character)
    {
    }
    void on_control_command(const ControlCommand &c, double) override
    {
        move = example::input::CommandView(c).move();
        presses += example::input::CommandView(c).press_count(example::input::actions::Jump);
    }
    void on_control_cancelled(InputCancelReason) override
    {
        move = {};
        ++cancels;
    }
    elysia::core::Vector2 move;
    int presses = 0, cancels = 0;
};
class UiFrameProbe final : public elysia::ui::UiElement, public elysia::ui::UiInputFrameReceiver
{
  public:
    elysia::ui::UiInputFrame last;
    void on_ui_input_frame(const elysia::ui::UiInputFrame &frame) override
    {
        last = frame;
    }
};
class CaptureScene final : public GameplayScene
{
  public:
    explicit CaptureScene(CaptureScene** slot) { *slot=this; }
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
};
void key(InputSystem &input, Uint32 type, SDL_Keycode code)
{
    SDL_Event e{};
    e.type = type;
    e.key.key = code;
    input.process_event(e);
}
int main()
{
    auto* service=ControllerService::instance();
    elysia::tests::ControlSceneFixture<CaptureScene> scene_fixture;
    auto& scene=*scene_fixture.scene;
    InputSystem input;
    auto *actor = scene.create_and_add_object<CaptureActor>();
    auto controller=elysia::tests::local_controller(scene,PrimaryLocalPlayer);
    require(bool(service->bind_target(controller,scene.control_context(),*actor)), "Bind actor");
    auto step = [&] {
        scene.on_input(input.snapshot());
        scene.on_update(1.0 / 60);
    };
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_D);
    step();
    require(actor->move.x > 0, "Baseline movement");
    input.set_development_input_capture(DevelopmentInputCapture::Keyboard);
    input.begin_frame();
    step();
    require(actor->move.is_zero(), "Capture cancels held movement");
    key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    step();
    require(actor->presses == 0, "Captured attack must not enter command queue");
    input.set_development_input_capture(DevelopmentInputCapture::None);
    input.begin_frame();
    step();
    require(actor->move.is_zero() && actor->presses == 0, "Capture exit requires neutral");
    input.begin_frame();
    key(input, SDL_EVENT_KEY_UP, SDLK_D);
    key(input, SDL_EVENT_KEY_UP, SDLK_SPACE);
    step();
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_D);
    step();
    require(actor->move.x > 0, "Fresh movement works after release");
    auto *window = scene.create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{0, 0, 400, 300});
    auto *text = window->create_child<elysia::ui::UiTextInput>(elysia::core::Rect{0, 0, 200, 40});
    text->set_focused(true);
    require(captured(window->input_capture(), InputDevice::Keyboard),
            "Focused text input capture propagates through containers");
    input.begin_frame();
    step();
    require(actor->move.is_zero(), "Text editing suppresses gameplay keyboard state");
    text->set_focused(false);
    input.begin_frame();
    step();
    require(actor->move.is_zero(), "Text blur must not restore held movement");
    auto second = scene.local_players().create_player();
    auto *other = scene.create_and_add_object<CaptureActor>();
    auto second_controller=elysia::tests::local_controller(scene,second);
    require(scene.local_players().bind_source(second, InputSourceId::gamepad(9)) &&
                bool(service->bind_target(second_controller,scene.control_context(),*other)),
            "Second player");
    text->set_focused(true);
    input.begin_frame();
    SDL_Event pad{};
    pad.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    pad.gbutton.which = 9;
    pad.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    input.process_event(pad);
    step();
    require(other->move.x > 0, "Owner keyboard capture cannot block another player");
    SDL_Event lost{};
    lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input.begin_frame();
    input.process_event(lost);
    step();
    require(other->move.is_zero(), "Focus loss cancels all players");
    input.begin_frame();
    step();
    require(other->move.is_zero(), "Focus restore requires gamepad neutral too");

    text->set_focused(false);
    auto *button = scene.create_and_add_object<elysia::ui::UiButton>(elysia::core::Rect{500, 0, 100, 40});
    button->set_focused(true);
    int clicks = 0;
    button->set_on_click([&] { ++clicks; });
    auto south = [&](Uint32 type) {
        SDL_Event e{};
        e.type = type;
        e.gbutton.which = 9;
        e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
        input.process_event(e);
    };
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_DOWN);
    step();
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_UP);
    step();
    require(clicks == 0 && other->presses == 1, "Non-owner gamepad controls its actor, never the shared UI");
    scene.set_ui_interaction_mode(UiInteractionMode::Navigation);
    scene.set_ui_gamepad(InputSourceId::gamepad(9));
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_DOWN);
    step();
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_UP);
    step();
    require(clicks == 1 && other->presses == 1, "Owner UI confirm must not also trigger the gameplay jump");
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_DOWN);
    step();
    scene.set_ui_gamepad(InputSourceId::gamepad(7));
    input.begin_frame();
    south(SDL_EVENT_GAMEPAD_BUTTON_UP);
    step();
    require(clicks == 1, "Owner transfer must cancel an in-progress button interaction");
    // A consumed HUD operation must not discard unrelated same-frame gameplay events.
    input.begin_frame();
    key(input, SDL_EVENT_KEY_DOWN, SDLK_SPACE);
    key(input, SDL_EVENT_KEY_UP, SDLK_SPACE);
    key(input, SDL_EVENT_KEY_DOWN, SDLK_RETURN);
    step();
    require(actor->presses == 1, "HUD confirm must preserve a separate keyboard gameplay tap");

    scene_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<CaptureScene> partial_fixture;
    auto& partial=*partial_fixture.scene;
    InputSystem partial_input;
    auto *partial_actor = partial.create_and_add_object<CaptureActor>();
    partial.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(9));
    auto partial_controller=elysia::tests::local_controller(partial,PrimaryLocalPlayer);
    (void)service->bind_target(partial_controller,partial.control_context(),*partial_actor);
    partial_input.set_development_input_capture(DevelopmentInputCapture::Keyboard);
    partial_input.begin_frame();
    SDL_Event down{};
    down.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    down.gbutton.which = 9;
    down.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    partial_input.process_event(down);
    partial.on_input(partial_input.snapshot());
    partial_input.begin_frame();
    partial.on_input(partial_input.snapshot());
    partial.on_update(1.0 / 60);
    require(partial_actor->presses == 1,
            "Persistent keyboard capture must not discard another device's pending action across zero ticks");

    partial_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<CaptureScene> exact_fixture;
    auto& exact=*exact_fixture.scene;
    InputSystem exact_input;
    auto *exact_actor = exact.create_and_add_object<CaptureActor>();
    exact.local_players().bind_source(PrimaryLocalPlayer, InputSourceId::gamepad(9));
    auto exact_controller=elysia::tests::local_controller(exact,PrimaryLocalPlayer);
    (void)service->bind_target(exact_controller,exact.control_context(),*exact_actor);
    auto *once = exact.create_and_add_object<elysia::ui::UiButton>(elysia::core::Rect{0, 0, 100, 40});
    once->set_focused(true);
    once->set_on_click([&] {
        once->set_visible(false);
        once->set_active(false);
    });
    exact.set_ui_interaction_mode(UiInteractionMode::Navigation);
    exact.set_ui_gamepad(InputSourceId::gamepad(9));
    exact_input.begin_frame();
    for (int tap = 0; tap < 2; ++tap)
    {
        exact_input.process_event(down);
        auto up = down;
        up.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
        exact_input.process_event(up);
    }
    exact.on_input(exact_input.snapshot());
    exact.on_update(1.0 / 60);
    require(exact_actor->presses == 1,
            "Consumption belongs to one original operation, not every matching control in the frame");

    exact_fixture.manager.shutdown();
    elysia::tests::ControlSceneFixture<CaptureScene> frame_scene_fixture;
    auto& frame_scene=*frame_scene_fixture.scene;
    frame_scene.set_ui_interaction_mode(UiInteractionMode::Navigation);
    InputSystem frame_input;
    auto *probe = frame_scene.create_and_add_object<UiFrameProbe>();
    frame_input.begin_frame();
    key(frame_input, SDL_EVENT_KEY_DOWN, SDLK_RETURN);
    frame_scene.on_input(frame_input.snapshot());
    require(probe->last.state.is_just_pressed(elysia::ui::UiAction::Confirm),
            "UI frame has a first-press edge");
    frame_input.begin_frame();
    frame_scene.on_input(frame_input.snapshot());
    require(probe->last.state.is_pressed(elysia::ui::UiAction::Confirm) &&
                !probe->last.state.is_just_pressed(elysia::ui::UiAction::Confirm),
            "Held UI state must not repeat a press edge every frame");
    frame_input.begin_frame();
    key(frame_input, SDL_EVENT_KEY_UP, SDLK_RETURN);
    frame_scene.on_input(frame_input.snapshot());
    require(probe->last.state.is_just_released(elysia::ui::UiAction::Confirm),
            "UI frame preserves the release edge");
}
