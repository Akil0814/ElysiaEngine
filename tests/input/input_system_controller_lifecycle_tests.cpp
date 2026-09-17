#define SDL_MAIN_HANDLED
#include "engine/input/input_system.h"
#include "tests/support/test_assertions.h"
#include <iostream>
using namespace elysia::input;
using elysia::tests::require;
SDL_Event button(SDL_JoystickID id, Uint32 type, Uint8 value)
{
    SDL_Event e{};
    e.type = type;
    e.gbutton.which = id;
    e.gbutton.button = value;
    return e;
}
SDL_Event axis(SDL_JoystickID id, Uint8 value, Sint16 amount)
{
    SDL_Event e{};
    e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.which = id;
    e.gaxis.axis = value;
    e.gaxis.value = amount;
    return e;
}
int main()
{
    InputSystem input;
    input.begin_frame();
    input.process_event(button(7, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_SOUTH));
    input.process_event(button(8, SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_EAST));
    input.process_event(axis(7, SDL_GAMEPAD_AXIS_LEFTX, 25000));
    input.process_event(axis(8, SDL_GAMEPAD_AXIS_LEFTX, -25000));
    input.process_event(axis(7, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 30000));
    input.process_event(axis(8, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0));
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.key = SDLK_A;
    key.key.scancode = SDL_SCANCODE_A;
    input.process_event(key);
    auto s = input.snapshot();
    auto *a = s.find(InputSourceId::gamepad(7));
    auto *b = s.find(InputSourceId::gamepad(8));
    require(a && b, "Each controller needs an independent frame");
    require(a->frame.state.is_pressed(RawInputControl::GamepadSouth) &&
                !b->frame.state.is_pressed(RawInputControl::GamepadSouth),
            "Buttons must be isolated");
    require(a->frame.state.axis_value(RawInputAxis::GamepadLeftX) > 0 &&
                b->frame.state.axis_value(RawInputAxis::GamepadLeftX) < 0,
            "Opposing sticks must coexist");
    require(a->frame.state.is_pressed(RawInputControl::GamepadLeftTriggerButton) &&
                !b->frame.state.is_pressed(RawInputControl::GamepadLeftTriggerButton),
            "Trigger hysteresis must be per device");
    require(s.find(InputSourceId::keyboard_mouse())->frame.state.is_pressed(RawInputControl::KeyA),
            "Keyboard must coexist with gamepads");
    input.begin_frame();
    SDL_Event removed{};
    removed.type = SDL_EVENT_GAMEPAD_REMOVED;
    removed.gdevice.which = 7;
    input.process_event(removed);
    s = input.snapshot();
    require(!s.find(InputSourceId::gamepad(7)) && s.find(InputSourceId::gamepad(8)),
            "Removal must affect only its source");
    require(s.removed.size() == 1 && s.removed[0] == InputSourceId::gamepad(7),
            "Removal must be explicit, not a normal gameplay release");
    require(s.find(InputSourceId::gamepad(8))->initial_state.is_pressed(RawInputControl::GamepadEast),
            "Frame boundary must retain other held controls");
    SDL_Event focus{};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input.process_event(focus);
    require(input.snapshot().focus_lost, "Focus loss must notify routing");
    input.shutdown();
    require(input.snapshot().events.empty(), "Shutdown must clear events");
    std::cout << "isolated controller lifecycle tests passed\n";
}
