#define SDL_MAIN_HANDLED

#include "engine/input/action/input_action_map.h"
#include "engine/input/input_system.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
using namespace elysia::input;
using elysia::tests::require;

SDL_Event key_event(Uint32 type, SDL_Keycode key)
{
    SDL_Event event{};
    event.type = type;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    return event;
}

SDL_Event controller_button_event(
    Uint32 type,
    SDL_JoystickID controller_id,
    SDL_GameControllerButton button)
{
    SDL_Event event{};
    event.type = type;
    event.cbutton.which = controller_id;
    event.cbutton.button = static_cast<Uint8>(button);
    return event;
}

SDL_Event controller_axis_event(
    SDL_JoystickID controller_id,
    SDL_GameControllerAxis axis,
    Sint16 value)
{
    SDL_Event event{};
    event.type = SDL_CONTROLLERAXISMOTION;
    event.caxis.which = controller_id;
    event.caxis.axis = static_cast<Uint8>(axis);
    event.caxis.value = value;
    return event;
}

SDL_Event controller_removed_event(SDL_JoystickID controller_id)
{
    SDL_Event event{};
    event.type = SDL_CONTROLLERDEVICEREMOVED;
    event.cdevice.which = controller_id;
    return event;
}

SDL_Event focus_lost_event()
{
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    return event;
}

std::size_t count_control_events(
    const InputSystem& input,
    RawInputControl control,
    RawInputEventType type)
{
    return static_cast<std::size_t>(std::count_if(
        input.events().begin(),
        input.events().end(),
        [&](const RawInputEvent& event)
        {
            return event.control == control && event.type == type;
        }));
}

std::size_t count_axis_events(
    const InputSystem& input,
    RawInputAxis axis,
    float value)
{
    return static_cast<std::size_t>(std::count_if(
        input.events().begin(),
        input.events().end(),
        [&](const RawInputEvent& event)
        {
            return event.axis == axis
                && event.type == RawInputEventType::AxisChanged
                && std::fabs(event.axis_value - value) < 0.001f;
        }));
}

void test_disconnect_releases_button_and_cancels_action()
{
    constexpr SDL_JoystickID Controller = 7;
    const InputActionId Confirm("test.confirm");

    InputSystem input;
    InputActionMap actions;
    require(actions.register_action(
        { Confirm, InputActionValueType::Button },
        { { Confirm, ButtonInputBinding{ RawInputControl::GamepadSouth } } }),
        "controller lifecycle test action must register");

    input.begin_frame();
    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        Controller,
        SDL_CONTROLLER_BUTTON_A));
    require(
        input.frame().state.is_pressed(RawInputControl::GamepadSouth),
        "the first controller button must become pressed");
    require(
        actions.resolve(input.frame()).frame.is_pressed(Confirm),
        "the controller button must actuate its action");

    input.begin_frame();
    input.process_event(controller_removed_event(Controller));

    const RawInputFrame disconnected = input.frame();
    require(
        !disconnected.state.is_pressed(RawInputControl::GamepadSouth)
            && disconnected.state.is_just_released(RawInputControl::GamepadSouth),
        "disconnecting the active controller must release held buttons");
    require(
        count_control_events(
            input,
            RawInputControl::GamepadSouth,
            RawInputEventType::ControlReleased) == 1,
        "disconnecting the active controller must emit one release event");
    require(
        disconnected.active_device == InputDevice::Unknown
            && disconnected.device_switched_this_frame,
        "disconnecting the active controller must deactivate the gamepad device");

    const ActionInputResult canceled = actions.resolve(disconnected);
    require(
        canceled.events.size() == 1
            && canceled.events.front().phase == ActionInputPhase::Canceled,
        "disconnecting the active controller must cancel active actions");
}

void test_disconnect_zeros_axes_and_trigger_button()
{
    constexpr SDL_JoystickID Controller = 11;

    InputSystem input;
    input.begin_frame();
    input.process_event(controller_axis_event(
        Controller,
        SDL_CONTROLLER_AXIS_TRIGGERLEFT,
        26000));
    input.process_event(controller_axis_event(
        Controller,
        SDL_CONTROLLER_AXIS_LEFTX,
        20000));

    require(
        input.frame().state.axis_value(RawInputAxis::GamepadLeftTrigger) > 0.5f
            && input.frame().state.axis_value(RawInputAxis::GamepadLeftX) > 0.5f,
        "controller axes must be populated before disconnect");
    require(
        input.frame().state.is_pressed(RawInputControl::GamepadLeftTriggerButton),
        "a sufficiently actuated trigger must press its virtual button");

    input.begin_frame();
    input.process_event(controller_removed_event(Controller));

    require(
        input.frame().state.axis_value(RawInputAxis::GamepadLeftTrigger) == 0.0f
            && input.frame().state.axis_value(RawInputAxis::GamepadLeftX) == 0.0f,
        "disconnecting the active controller must zero all non-zero axes");
    require(
        !input.frame().state.is_pressed(RawInputControl::GamepadLeftTriggerButton),
        "disconnecting the active controller must release trigger virtual buttons");
    require(
        count_axis_events(input, RawInputAxis::GamepadLeftTrigger, 0.0f) == 1
            && count_axis_events(input, RawInputAxis::GamepadLeftX, 0.0f) == 1,
        "disconnecting the active controller must emit zero axis events");
    require(
        count_control_events(
            input,
            RawInputControl::GamepadLeftTriggerButton,
            RawInputEventType::ControlReleased) == 1,
        "disconnecting the active controller must emit a trigger release event");
}

void test_first_gamepad_input_survives_device_switch()
{
    constexpr SDL_JoystickID Controller = 17;

    InputSystem input;
    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_w));
    require(
        input.current_device() == InputDevice::Keyboard
            && input.frame().state.is_pressed(RawInputControl::KeyW),
        "keyboard input must establish the keyboard device");

    input.begin_frame();
    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        Controller,
        SDL_CONTROLLER_BUTTON_A));
    require(
        input.current_device() == InputDevice::Gamepad
            && input.frame().device_switched_this_frame,
        "a controller button must switch from keyboard to gamepad");
    require(
        input.frame().state.is_pressed(RawInputControl::GamepadSouth)
            && count_control_events(
                input,
                RawInputControl::GamepadSouth,
                RawInputEventType::ControlPressed) == 1,
        "the button that switches to gamepad must be translated in the same frame");

    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_k));
    require(
        input.current_device() == InputDevice::Keyboard
            && input.frame().device_switched_this_frame,
        "switching from gamepad back to keyboard must set the switch flag");
    require(
        !input.frame().state.is_pressed(RawInputControl::GamepadSouth)
            && input.frame().state.is_pressed(RawInputControl::KeyK),
        "switching back to keyboard must release gamepad state and translate the key");

    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 10;
    motion.motion.y = 20;
    input.process_event(motion);
    require(
        input.current_device() == InputDevice::Keyboard,
        "ordinary mouse motion must not change the active device");
}

void test_axis_dead_zone_and_single_active_controller()
{
    constexpr SDL_JoystickID First = 23;
    constexpr SDL_JoystickID Second = 29;
    constexpr SDL_JoystickID Remaining = 37;

    InputSystem input;
    input.begin_frame();
    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        Remaining,
        SDL_CONTROLLER_BUTTON_A));

    input.begin_frame();
    input.process_event(controller_axis_event(
        Second,
        SDL_CONTROLLER_AXIS_LEFTX,
        6000));
    require(
        input.events().empty()
            && input.frame().state.is_pressed(RawInputControl::GamepadSouth),
        "sub-dead-zone drift from an inactive controller must be ignored");

    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        Second,
        SDL_CONTROLLER_BUTTON_X));
    require(
        !input.frame().state.is_pressed(RawInputControl::GamepadSouth)
            && input.frame().state.is_pressed(RawInputControl::GamepadWest),
        "meaningful input from another controller must transfer active control");
    require(
        count_control_events(
            input,
            RawInputControl::GamepadSouth,
            RawInputEventType::ControlReleased) == 1,
        "transferring active control must release the previous controller state");

    input.begin_frame();
    input.process_event(controller_removed_event(First));
    require(
        input.events().empty()
            && input.frame().state.is_pressed(RawInputControl::GamepadWest)
            && input.current_device() == InputDevice::Gamepad,
        "disconnecting an inactive controller must not affect the active controller");

    input.begin_frame();
    input.process_event(controller_removed_event(Second));
    require(
        !input.frame().state.is_pressed(RawInputControl::GamepadWest)
            && input.current_device() == InputDevice::Unknown,
        "disconnecting the active controller must clear its state");

    input.begin_frame();
    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        First,
        SDL_CONTROLLER_BUTTON_A));
    require(
        input.frame().state.is_pressed(RawInputControl::GamepadSouth)
            && input.current_device() == InputDevice::Gamepad,
        "a remaining controller must reacquire control through meaningful input");
}

void test_axis_can_activate_and_focus_loss_resets_selection()
{
    constexpr SDL_JoystickID Controller = 31;

    InputSystem input;
    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_d));
    input.begin_frame();
    input.process_event(controller_axis_event(
        Controller,
        SDL_CONTROLLER_AXIS_LEFTX,
        8000));

    require(
        input.current_device() == InputDevice::Gamepad
            && input.frame().device_switched_this_frame
            && input.frame().state.axis_value(RawInputAxis::GamepadLeftX) > 0.22f,
        "the first axis value above the activation dead zone must switch and translate");

    input.process_event(focus_lost_event());
    require(
        input.current_device() == InputDevice::Unknown
            && !input.frame().state.is_pressed(RawInputControl::KeyD)
            && input.frame().state.axis_value(RawInputAxis::GamepadLeftX) == 0.0f,
        "focus loss must reset device and raw input state");

    input.begin_frame();
    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONUP,
        Controller,
        SDL_CONTROLLER_BUTTON_A));
    require(
        input.events().empty() && input.current_device() == InputDevice::Unknown,
        "a release after focus loss must not reactivate the controller");

    input.process_event(controller_button_event(
        SDL_CONTROLLERBUTTONDOWN,
        Controller,
        SDL_CONTROLLER_BUTTON_A));
    require(
        input.frame().state.is_pressed(RawInputControl::GamepadSouth)
            && input.current_device() == InputDevice::Gamepad,
        "a meaningful event after focus loss must reacquire the controller");
}

void test_input_and_controller_initialization_lifecycle()
{
    ControllerManager controllers;
    require(!controllers.is_initialized(),
        "controller manager must begin uninitialized");
    controllers.initialize();
    require(controllers.is_initialized(),
        "controller manager initialization must publish initialized state");
    controllers.initialize();
    require(controllers.is_initialized(),
        "controller manager initialization must be idempotent");
    controllers.shutdown();
    controllers.shutdown();
    require(!controllers.is_initialized(),
        "controller manager shutdown must be idempotent");
    controllers.initialize();
    require(controllers.is_initialized(),
        "controller manager must support initialization after shutdown");
    controllers.shutdown();

    InputSystem input;
    require(!input.is_initialized(),
        "input system must begin uninitialized");
    input.initialize();
    require(input.is_initialized(),
        "input system initialization must publish initialized state");
    input.initialize();
    require(input.is_initialized(),
        "input system repeated initialization must remain initialized");
    input.shutdown();
    input.shutdown();
    require(!input.is_initialized(),
        "input system shutdown must be idempotent");
    input.initialize();
    require(input.is_initialized(),
        "input system must support initialization after shutdown");
    input.shutdown();
}
}

int main()
{
    require(SDL_Init(SDL_INIT_GAMECONTROLLER) == 0,
        "controller lifecycle tests must initialize SDL game controller support");
    test_input_and_controller_initialization_lifecycle();
    test_disconnect_releases_button_and_cancels_action();
    test_disconnect_zeros_axes_and_trigger_button();
    test_first_gamepad_input_survives_device_switch();
    test_axis_dead_zone_and_single_active_controller();
    test_axis_can_activate_and_focus_loss_resets_selection();
    SDL_Quit();
    std::cout << "input system controller lifecycle tests passed\n";
    return EXIT_SUCCESS;
}
