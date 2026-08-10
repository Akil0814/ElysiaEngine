#define SDL_MAIN_HANDLED

#include "engine/application/presentation/application_sdl_presentation.h"
#include "engine/input/input_system.h"
#include "engine/ui/input/ui_input_router.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdlib>

namespace
{
using elysia::application::detail::configure_sdl_renderer_presentation;
using elysia::input::InputSystem;
using elysia::input::RawInputEvent;
using elysia::input::RawInputEventType;
using elysia::ui::UiInputEventType;
using elysia::ui::UiInputRouter;
using elysia::tests::require;

class InputSystemFixture
{
public:
    InputSystemFixture()
    {
        require(
            SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0,
            "pointer coordinate tests must initialize SDL");

        _window = SDL_CreateWindow(
            "InputSystem pointer coordinate tests",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            1280,
            720,
            SDL_WINDOW_HIDDEN);
        require(
            _window != nullptr,
            "pointer coordinate tests must create an SDL window");

        _renderer = SDL_CreateRenderer(
            _window,
            -1,
            SDL_RENDERER_SOFTWARE);
        require(
            _renderer != nullptr,
            "pointer coordinate tests must create an SDL renderer");
        require(
            configure_sdl_renderer_presentation(
                _renderer,
                1280,
                720).has_value(),
            "pointer coordinate tests must configure logical presentation");

        _input_system.initialize();
        _input_system.set_renderer(_renderer);
    }

    ~InputSystemFixture()
    {
        _input_system.shutdown();
        _input_system.set_renderer(nullptr);
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    [[nodiscard]] InputSystem& input_system() noexcept
    {
        return _input_system;
    }

    [[nodiscard]] SDL_Event filtered_mouse_motion(int window_x,int window_y)
    {
        drain_events();

        SDL_Event event{};
        event.type = SDL_MOUSEMOTION;
        event.motion.windowID = SDL_GetWindowID(_window);
        event.motion.x = window_x;
        event.motion.y = window_y;
        require(
            SDL_PushEvent(&event) == 1,
            "pointer coordinate tests must enqueue mouse motion");

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_MOUSEMOTION
                && event.motion.windowID == SDL_GetWindowID(_window))
            {
                return event;
            }
        }

        require(false, "pointer coordinate tests must receive mouse motion");
        return {};
    }

    [[nodiscard]] SDL_Event filtered_mouse_button(
        Uint32 type,
        int window_x,
        int window_y)
    {
        drain_events();

        SDL_Event event{};
        event.type = type;
        event.button.windowID = SDL_GetWindowID(_window);
        event.button.button = SDL_BUTTON_LEFT;
        event.button.x = window_x;
        event.button.y = window_y;
        require(
            SDL_PushEvent(&event) == 1,
            "pointer coordinate tests must enqueue a mouse button event");

        while (SDL_PollEvent(&event))
        {
            if (event.type == type
                && event.button.windowID == SDL_GetWindowID(_window))
            {
                return event;
            }
        }

        require(false, "pointer coordinate tests must receive a mouse button event");
        return {};
    }

    void set_window_size(int width,int height)
    {
        SDL_SetWindowSize(_window,width,height);
        SDL_PumpEvents();

        int output_width = 0;
        int output_height = 0;
        require(
            SDL_GetRendererOutputSize(
                _renderer,
                &output_width,
                &output_height) == 0,
            "pointer coordinate tests must query renderer output size");
        require(
            output_width == width && output_height == height,
            "renderer output size must follow the test window size");
    }

private:
    void drain_events()
    {
        SDL_Event event{};
        while (SDL_PollEvent(&event))
        {
        }
    }

private:
    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;
    InputSystem _input_system;
};

[[nodiscard]] SDL_Event mouse_motion_event(int x,int y)
{
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = x;
    event.motion.y = y;
    return event;
}

[[nodiscard]] SDL_Event mouse_button_event(Uint32 type,int x,int y)
{
    SDL_Event event{};
    event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    return event;
}

void require_single_pointer_event(
    const InputSystem& input_system,
    RawInputEventType type,
    int expected_x,
    int expected_y,
    const char* message)
{
    const auto& events = input_system.events();
    require(events.size() == 1, message);
    require(events.front().type == type, message);
    require(
        events.front().mouse_x == expected_x
            && events.front().mouse_y == expected_y,
        message);
}

void require_native_size_coordinates(InputSystemFixture& fixture)
{
    InputSystem& input_system = fixture.input_system();

    input_system.begin_frame();
    input_system.process_event(mouse_motion_event(320,180));
    require_single_pointer_event(
        input_system,
        RawInputEventType::MouseMoved,
        320,
        180,
        "native-size mouse motion must retain its coordinates");

    input_system.begin_frame();
    input_system.process_event(
        mouse_button_event(SDL_MOUSEBUTTONDOWN,640,360));
    require_single_pointer_event(
        input_system,
        RawInputEventType::ControlPressed,
        640,
        360,
        "native-size mouse clicks must retain their coordinates");
}

void require_scaled_coordinates_and_deltas(InputSystemFixture& fixture)
{
    fixture.set_window_size(960,540);
    InputSystem& input_system = fixture.input_system();

    input_system.begin_frame();
    const SDL_Event first_motion =
        fixture.filtered_mouse_motion(240,135);
    require(
        first_motion.motion.x == 320
            && first_motion.motion.y == 180,
        "SDL must filter scaled mouse motion into logical coordinates");
    input_system.process_event(first_motion);
    require_single_pointer_event(
        input_system,
        RawInputEventType::MouseMoved,
        320,
        180,
        "InputSystem must not scale SDL logical mouse motion a second time");

    input_system.begin_frame();
    const SDL_Event second_motion =
        fixture.filtered_mouse_motion(480,270);
    input_system.process_event(second_motion);
    const auto& events = input_system.events();
    require(
        events.size() == 1
            && events.front().mouse_x == 640
            && events.front().mouse_y == 360,
        "subsequent scaled mouse motion must retain logical coordinates");
    require(
        events.back().mouse_delta_x == 320
            && events.back().mouse_delta_y == 180,
        "mouse deltas must be calculated in logical coordinates");
    const auto frame = input_system.frame();
    require(
        frame.mouse_delta_x == 320
            && frame.mouse_delta_y == 180,
        "the input frame must accumulate logical mouse deltas");

    input_system.begin_frame();
    const SDL_Event button =
        fixture.filtered_mouse_button(SDL_MOUSEBUTTONDOWN,480,270);
    require(
        button.button.x == 640
            && button.button.y == 360,
        "SDL must filter scaled mouse button positions into logical coordinates");
    input_system.process_event(button);
    require_single_pointer_event(
        input_system,
        RawInputEventType::ControlPressed,
        640,
        360,
        "InputSystem must not scale SDL logical mouse clicks a second time");
}

void require_letterbox_coordinates(InputSystemFixture& fixture)
{
    fixture.set_window_size(1024,768);
    InputSystem& input_system = fixture.input_system();

    input_system.begin_frame();
    const SDL_Event viewport_edge =
        fixture.filtered_mouse_motion(512,96);
    input_system.process_event(viewport_edge);
    require_single_pointer_event(
        input_system,
        RawInputEventType::MouseMoved,
        640,
        0,
        "letterbox viewport origin must map to the logical top edge");

    input_system.begin_frame();
    const SDL_Event black_bar =
        fixture.filtered_mouse_motion(512,48);
    input_system.process_event(black_bar);
    require_single_pointer_event(
        input_system,
        RawInputEventType::MouseMoved,
        640,
        -60,
        "letterbox black bars must remain outside the logical canvas");
}

void require_wheel_coordinates(InputSystemFixture& fixture)
{
    InputSystem& input_system = fixture.input_system();
    const auto previous_frame = input_system.frame();

    SDL_Event event{};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.y = 1;

    input_system.begin_frame();
    input_system.process_event(event);
    require_single_pointer_event(
        input_system,
        RawInputEventType::MouseWheel,
        previous_frame.mouse_x,
        previous_frame.mouse_y,
        "mouse wheel events must use the cached logical pointer position");
    require(
        input_system.frame().mouse_delta_x == 0
            && input_system.frame().mouse_delta_y == 0,
        "mouse wheel events must not create mouse movement deltas");
}

void require_size_change_refresh(InputSystemFixture& fixture)
{
    InputSystem& input_system = fixture.input_system();

    input_system.begin_frame();
    input_system.process_event(mouse_motion_event(777,555));

    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;

    input_system.begin_frame();
    input_system.process_event(event);

    const auto& events = input_system.events();
    require(
        events.size() == 1
            && events.front().type == RawInputEventType::MouseMoved,
        "window size changes must synthesize one logical mouse move");
    require(
        events.front().mouse_delta_x == 0
            && events.front().mouse_delta_y == 0,
        "viewport remapping must not create physical mouse deltas");

    const auto frame = input_system.frame();
    require(
        frame.mouse_x == events.front().mouse_x
            && frame.mouse_y == events.front().mouse_y,
        "window size changes must refresh the frame mouse cache");
    require(
        frame.mouse_delta_x == 0 && frame.mouse_delta_y == 0,
        "window size changes must leave frame mouse deltas at zero");
}

void require_ui_router_preserves_pointer_coordinates()
{
    UiInputRouter router;

    const RawInputEvent movement{
        .type = RawInputEventType::MouseMoved,
        .device = elysia::input::InputDevice::Mouse,
        .mouse_x = 321,
        .mouse_y = -45
    };
    const auto movement_events = router.route_event(movement);
    require(
        movement_events.size() == 1
            && movement_events.front().type == UiInputEventType::MouseMoved
            && movement_events.front().mouse_x == 321
            && movement_events.front().mouse_y == -45,
        "UI routing must preserve logical mouse movement coordinates");

    const RawInputEvent press{
        .control = elysia::input::RawInputControl::MouseLeft,
        .type = RawInputEventType::ControlPressed,
        .device = elysia::input::InputDevice::Mouse,
        .mouse_button = SDL_BUTTON_LEFT,
        .mouse_x = 640,
        .mouse_y = 360
    };
    const auto press_events = router.route_event(press);
    require(
        press_events.size() == 1
            && press_events.front().type == UiInputEventType::PointerPressed
            && press_events.front().mouse_x == 640
            && press_events.front().mouse_y == 360,
        "UI routing must preserve logical mouse button coordinates");

    const RawInputEvent wheel{
        .type = RawInputEventType::MouseWheel,
        .device = elysia::input::InputDevice::Mouse,
        .mouse_x = 800,
        .mouse_y = 450,
        .wheel_y = -1
    };
    const auto wheel_events = router.route_event(wheel);
    require(
        wheel_events.size() == 1
            && wheel_events.front().type == UiInputEventType::MouseWheel
            && wheel_events.front().mouse_x == 800
            && wheel_events.front().mouse_y == 450
            && wheel_events.front().wheel_y == -1,
        "UI routing must preserve logical mouse wheel coordinates");
}
}

int main()
{
    InputSystemFixture fixture;
    require_native_size_coordinates(fixture);
    require_scaled_coordinates_and_deltas(fixture);
    require_letterbox_coordinates(fixture);
    require_wheel_coordinates(fixture);
    require_size_change_refresh(fixture);
    require_ui_router_preserves_pointer_coordinates();
    return EXIT_SUCCESS;
}
