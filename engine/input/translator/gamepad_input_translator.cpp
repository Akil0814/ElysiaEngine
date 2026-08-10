#include "gamepad_input_translator.h"

#include <SDL.h>

#include <algorithm>

namespace elysia::input
{
namespace
{
constexpr float k_trigger_pressed_threshold = 0.5f;
constexpr float k_trigger_released_threshold = 0.4f;
}

std::vector<RawInputEvent> GamepadInputTranslator::translate_event(const SDL_Event& event)
{
    std::vector<RawInputEvent> events;

    switch (event.type)
    {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        append_controller_button_events(
            events,
            event.cbutton.button,
            input_event_type(event.type == SDL_CONTROLLERBUTTONDOWN)
        );
        break;

    case SDL_CONTROLLERAXISMOTION:
        switch (event.caxis.axis)
        {
        case SDL_CONTROLLER_AXIS_LEFTX:
            append_axis_event(
                events,
                RawInputAxis::GamepadLeftX,
                normalize_stick_axis(event.caxis.value)
            );
            break;

        case SDL_CONTROLLER_AXIS_LEFTY:
            append_axis_event(
                events,
                RawInputAxis::GamepadLeftY,
                normalize_stick_axis(event.caxis.value)
            );
            break;

        case SDL_CONTROLLER_AXIS_RIGHTX:
            append_axis_event(
                events,
                RawInputAxis::GamepadRightX,
                normalize_stick_axis(event.caxis.value)
            );
            break;

        case SDL_CONTROLLER_AXIS_RIGHTY:
            append_axis_event(
                events,
                RawInputAxis::GamepadRightY,
                normalize_stick_axis(event.caxis.value)
            );
            break;

        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        {
            const float normalized_value = normalize_trigger_axis(event.caxis.value);
            append_axis_event(events, RawInputAxis::GamepadLeftTrigger, normalized_value);

            if (!_left_trigger_pressed && normalized_value >= k_trigger_pressed_threshold)
            {
                _left_trigger_pressed = true;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadLeftTriggerButton,
                    true
                );
            }
            else if (_left_trigger_pressed && normalized_value <= k_trigger_released_threshold)
            {
                _left_trigger_pressed = false;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadLeftTriggerButton,
                    false
                );
            }
            break;
        }

        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        {
            const float normalized_value = normalize_trigger_axis(event.caxis.value);
            append_axis_event(events, RawInputAxis::GamepadRightTrigger, normalized_value);

            if (!_right_trigger_pressed && normalized_value >= k_trigger_pressed_threshold)
            {
                _right_trigger_pressed = true;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadRightTriggerButton,
                    true
                );
            }
            else if (_right_trigger_pressed && normalized_value <= k_trigger_released_threshold)
            {
                _right_trigger_pressed = false;
                append_trigger_virtual_button_event(
                    events,
                    RawInputControl::GamepadRightTriggerButton,
                    false
                );
            }
            break;
        }

        default:
            break;
        }
        break;

    default:
        break;
    }

    return events;
}

void GamepadInputTranslator::reset()
{
    _left_trigger_pressed = false;
    _right_trigger_pressed = false;
}

void GamepadInputTranslator::append_controller_button_events(
    std::vector<RawInputEvent>& events,
    Uint8 button,
    RawInputEventType type
) const
{
    switch (static_cast<SDL_GameControllerButton>(button))
    {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        append_event(events, RawInputControl::GamepadDPadLeft, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        append_event(events, RawInputControl::GamepadDPadRight, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        append_event(events, RawInputControl::GamepadDPadUp, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        append_event(events, RawInputControl::GamepadDPadDown, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_A:
        append_event(events, RawInputControl::GamepadSouth, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_B:
        append_event(events, RawInputControl::GamepadEast, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_X:
        append_event(events, RawInputControl::GamepadWest, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_Y:
        append_event(events, RawInputControl::GamepadNorth, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_BACK:
        append_event(events, RawInputControl::GamepadBack, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_GUIDE:
        append_event(events, RawInputControl::GamepadGuide, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_START:
        append_event(events, RawInputControl::GamepadStart, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        append_event(events, RawInputControl::GamepadLeftStick, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        append_event(events, RawInputControl::GamepadRightStick, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        append_event(events, RawInputControl::GamepadLeftShoulder, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        append_event(events, RawInputControl::GamepadRightShoulder, type, InputDevice::Gamepad);
        break;
#if SDL_VERSION_ATLEAST(2, 0, 14)
    case SDL_CONTROLLER_BUTTON_MISC1:
        append_event(events, RawInputControl::GamepadMisc1, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_PADDLE1:
        append_event(events, RawInputControl::GamepadPaddle1, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_PADDLE2:
        append_event(events, RawInputControl::GamepadPaddle2, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_PADDLE3:
        append_event(events, RawInputControl::GamepadPaddle3, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_PADDLE4:
        append_event(events, RawInputControl::GamepadPaddle4, type, InputDevice::Gamepad);
        break;
    case SDL_CONTROLLER_BUTTON_TOUCHPAD:
        append_event(events, RawInputControl::GamepadTouchpad, type, InputDevice::Gamepad);
        break;
#endif
    default:
        break;
    }
}

void GamepadInputTranslator::append_axis_event(
    std::vector<RawInputEvent>& events,
    RawInputAxis axis,
    float normalized_value
) const
{
    RawInputEvent input_event;
    input_event.type = RawInputEventType::AxisChanged;
    input_event.device = InputDevice::Gamepad;
    input_event.axis = axis;
    input_event.axis_value = normalized_value;
    events.push_back(input_event);
}

void GamepadInputTranslator::append_trigger_virtual_button_event(
    std::vector<RawInputEvent>& events,
    RawInputControl control,
    bool pressed
) const
{
    append_event(
        events,
        control,
        pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased,
        InputDevice::Gamepad
    );
}

float GamepadInputTranslator::normalize_stick_axis(Sint16 value) const
{
    return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
}

float GamepadInputTranslator::normalize_trigger_axis(Sint16 value) const
{
    return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
}

}
