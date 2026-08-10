#include "keyboard_mouse_input_translator.h"

#include <SDL.h>

namespace elysia::input
{
std::vector<RawInputEvent> KeyboardMouseInputTranslator::translate_event(const SDL_Event& event)
{
    std::vector<RawInputEvent> events;

    switch (event.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        if (event.type == SDL_KEYDOWN && event.key.repeat != 0)
        {
            return events;
        }

        RawInputEvent input_event;
        input_event.control =
            control_from_key(event.key.keysym.sym).value_or(RawInputControl::None);
        input_event.type = input_event_type(event.type == SDL_KEYDOWN);
        input_event.device = InputDevice::Keyboard;
        input_event.keycode = event.key.keysym.sym;
        input_event.scancode = event.key.keysym.scancode;
        events.push_back(input_event);
        break;
    }

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    {
        RawInputEvent input_event;
        input_event.control =
            control_from_mouse_button(event.button.button).value_or(RawInputControl::None);
        input_event.type = input_event_type(event.type == SDL_MOUSEBUTTONDOWN);
        input_event.device = InputDevice::Mouse;
        input_event.mouse_button = event.button.button;
        input_event.mouse_x = event.button.x;
        input_event.mouse_y = event.button.y;
        events.push_back(input_event);
        break;
    }

    case SDL_MOUSEMOTION:
    {
        RawInputEvent input_event;
        input_event.type = RawInputEventType::MouseMoved;
        input_event.device = InputDevice::Mouse;
        input_event.mouse_x = event.motion.x;
        input_event.mouse_y = event.motion.y;
        input_event.mouse_delta_x = event.motion.xrel;
        input_event.mouse_delta_y = event.motion.yrel;
        events.push_back(input_event);
        break;
    }

    case SDL_MOUSEWHEEL:
    {
        RawInputEvent input_event;
        input_event.type = RawInputEventType::MouseWheel;
        input_event.device = InputDevice::Mouse;
        input_event.wheel_x = event.wheel.x;
        input_event.wheel_y = event.wheel.y;
        SDL_GetMouseState(&input_event.mouse_x,&input_event.mouse_y);
        events.push_back(input_event);
        break;
    }

    case SDL_TEXTINPUT:
    {
        RawInputEvent input_event;
        input_event.type = RawInputEventType::TextInput;
        input_event.device = InputDevice::Keyboard;
        input_event.text = event.text.text;
        events.push_back(input_event);
        break;
    }

    case SDL_TEXTEDITING:
    {
        RawInputEvent input_event;
        input_event.type = RawInputEventType::TextEditing;
        input_event.device = InputDevice::Keyboard;
        input_event.text = event.edit.text;
        input_event.composition_start = event.edit.start;
        input_event.composition_length = event.edit.length;
        events.push_back(input_event);
        break;
    }

    default:
        break;
    }

    return events;
}

std::optional<RawInputControl> KeyboardMouseInputTranslator::control_from_key(SDL_Keycode key) const
{
    switch (key)
    {
    case SDLK_a: return RawInputControl::KeyA;
    case SDLK_b: return RawInputControl::KeyB;
    case SDLK_c: return RawInputControl::KeyC;
    case SDLK_d: return RawInputControl::KeyD;
    case SDLK_e: return RawInputControl::KeyE;
    case SDLK_f: return RawInputControl::KeyF;
    case SDLK_g: return RawInputControl::KeyG;
    case SDLK_h: return RawInputControl::KeyH;
    case SDLK_i: return RawInputControl::KeyI;
    case SDLK_j: return RawInputControl::KeyJ;
    case SDLK_k: return RawInputControl::KeyK;
    case SDLK_l: return RawInputControl::KeyL;
    case SDLK_m: return RawInputControl::KeyM;
    case SDLK_n: return RawInputControl::KeyN;
    case SDLK_o: return RawInputControl::KeyO;
    case SDLK_p: return RawInputControl::KeyP;
    case SDLK_q: return RawInputControl::KeyQ;
    case SDLK_r: return RawInputControl::KeyR;
    case SDLK_s: return RawInputControl::KeyS;
    case SDLK_t: return RawInputControl::KeyT;
    case SDLK_u: return RawInputControl::KeyU;
    case SDLK_v: return RawInputControl::KeyV;
    case SDLK_w: return RawInputControl::KeyW;
    case SDLK_x: return RawInputControl::KeyX;
    case SDLK_y: return RawInputControl::KeyY;
    case SDLK_z: return RawInputControl::KeyZ;

    case SDLK_0: return RawInputControl::Key0;
    case SDLK_1: return RawInputControl::Key1;
    case SDLK_2: return RawInputControl::Key2;
    case SDLK_3: return RawInputControl::Key3;
    case SDLK_4: return RawInputControl::Key4;
    case SDLK_5: return RawInputControl::Key5;
    case SDLK_6: return RawInputControl::Key6;
    case SDLK_7: return RawInputControl::Key7;
    case SDLK_8: return RawInputControl::Key8;
    case SDLK_9: return RawInputControl::Key9;

    case SDLK_LEFT: return RawInputControl::KeyLeft;
    case SDLK_RIGHT: return RawInputControl::KeyRight;
    case SDLK_UP: return RawInputControl::KeyUp;
    case SDLK_DOWN: return RawInputControl::KeyDown;
    case SDLK_RETURN: return RawInputControl::KeyEnter;
    case SDLK_KP_ENTER: return RawInputControl::KeyNumpadEnter;
    case SDLK_ESCAPE: return RawInputControl::KeyEscape;
    case SDLK_SPACE: return RawInputControl::KeySpace;
    case SDLK_INSERT: return RawInputControl::KeyInsert;
    case SDLK_PAGEUP: return RawInputControl::KeyPageUp;
    case SDLK_PAGEDOWN: return RawInputControl::KeyPageDown;
    case SDLK_LSHIFT: return RawInputControl::KeyLeftShift;
    case SDLK_RSHIFT: return RawInputControl::KeyRightShift;
    case SDLK_LCTRL: return RawInputControl::KeyLeftCtrl;
    case SDLK_RCTRL: return RawInputControl::KeyRightCtrl;
    case SDLK_LALT: return RawInputControl::KeyLeftAlt;
    case SDLK_RALT: return RawInputControl::KeyRightAlt;
    case SDLK_BACKSPACE: return RawInputControl::KeyBackspace;
    case SDLK_DELETE: return RawInputControl::KeyDelete;
    case SDLK_HOME: return RawInputControl::KeyHome;
    case SDLK_END: return RawInputControl::KeyEnd;
    case SDLK_TAB: return RawInputControl::KeyTab;
    case SDLK_MINUS: return RawInputControl::KeyMinus;
    case SDLK_EQUALS: return RawInputControl::KeyEquals;
    case SDLK_LEFTBRACKET: return RawInputControl::KeyLeftBracket;
    case SDLK_RIGHTBRACKET: return RawInputControl::KeyRightBracket;
    case SDLK_SEMICOLON: return RawInputControl::KeySemicolon;
    case SDLK_QUOTE: return RawInputControl::KeyApostrophe;
    case SDLK_COMMA: return RawInputControl::KeyComma;
    case SDLK_PERIOD: return RawInputControl::KeyPeriod;
    case SDLK_SLASH: return RawInputControl::KeySlash;
    case SDLK_BACKSLASH: return RawInputControl::KeyBackslash;
    case SDLK_BACKQUOTE: return RawInputControl::KeyGrave;
    case SDLK_F1: return RawInputControl::KeyF1;
    case SDLK_F2: return RawInputControl::KeyF2;
    case SDLK_F3: return RawInputControl::KeyF3;
    case SDLK_F4: return RawInputControl::KeyF4;
    case SDLK_F5: return RawInputControl::KeyF5;
    case SDLK_F6: return RawInputControl::KeyF6;
    case SDLK_F7: return RawInputControl::KeyF7;
    case SDLK_F8: return RawInputControl::KeyF8;
    case SDLK_F9: return RawInputControl::KeyF9;
    case SDLK_F10: return RawInputControl::KeyF10;
    case SDLK_F11: return RawInputControl::KeyF11;
    case SDLK_F12: return RawInputControl::KeyF12;
    default:
        return std::nullopt;
    }
}

std::optional<RawInputControl> KeyboardMouseInputTranslator::control_from_mouse_button(Uint8 button) const
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return RawInputControl::MouseLeft;
    case SDL_BUTTON_RIGHT:
        return RawInputControl::MouseRight;
    case SDL_BUTTON_MIDDLE:
        return RawInputControl::MouseMiddle;
    case SDL_BUTTON_X1:
        return RawInputControl::MouseX1;
    case SDL_BUTTON_X2:
        return RawInputControl::MouseX2;
    default:
        return std::nullopt;
    }
}

}
