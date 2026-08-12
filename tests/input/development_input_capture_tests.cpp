#define SDL_MAIN_HANDLED

#include "engine/input/input_system.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdlib>
#include <cstring>

namespace
{
using elysia::input::DevelopmentInputCapture;
using elysia::input::InputSystem;
using elysia::input::RawInputControl;
using elysia::tests::require;

[[nodiscard]] SDL_Event key_event(Uint32 type, SDL_Keycode key)
{
    SDL_Event event{};
    event.type = type;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    return event;
}

[[nodiscard]] SDL_Event mouse_button_event(Uint32 type)
{
    SDL_Event event{};
    event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    return event;
}

void require_capture_filters_and_clears_state()
{
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0,
        "development input capture tests must initialize SDL");

    InputSystem input;
    input.initialize();
    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_a));
    input.process_event(mouse_button_event(SDL_MOUSEBUTTONDOWN));
    input.process_event(key_event(SDL_KEYDOWN, SDLK_a));
    require(input.frame().state.is_pressed(RawInputControl::KeyA),
        "the baseline keyboard event must create held state");
    require(input.frame().state.is_pressed(RawInputControl::MouseLeft),
        "the baseline pointer event must create held state");

    input.set_development_input_capture(
        DevelopmentInputCapture::Keyboard | DevelopmentInputCapture::Pointer);
    require(!input.frame().state.is_pressed(RawInputControl::KeyA)
            && !input.frame().state.is_pressed(RawInputControl::MouseLeft),
        "starting capture must clear held keyboard and pointer state");

    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_b));
    input.process_event(mouse_button_event(SDL_MOUSEBUTTONDOWN));
    SDL_Event text{};
    text.type = SDL_TEXTINPUT;
    std::strcpy(text.text.text, "x");
    input.process_event(text);
    require(input.events().empty(),
        "captured keyboard, text, and pointer events must not reach RawInput");
    require(input.current_device() == elysia::input::InputDevice::Keyboard,
        "captured pointer input must not change the game's active device");
    require(!input.frame().state.is_pressed(RawInputControl::KeyB)
            && !input.frame().state.is_pressed(RawInputControl::MouseLeft),
        "captured input must not rebuild held state");

    SDL_Event window_event{};
    window_event.type = SDL_WINDOWEVENT;
    window_event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    input.process_event(window_event);
    require(input.events().empty(),
        "window lifecycle events must remain safe while input is captured");

    input.set_development_input_capture(DevelopmentInputCapture::None);
    input.begin_frame();
    input.process_event(key_event(SDL_KEYDOWN, SDLK_c));
    require(input.frame().state.is_pressed(RawInputControl::KeyC)
            && input.events().size() == 1,
        "releasing capture must restore normal keyboard input");

    input.shutdown();
    SDL_Quit();
}
}

int main()
{
    require_capture_filters_and_clears_state();
    return EXIT_SUCCESS;
}
