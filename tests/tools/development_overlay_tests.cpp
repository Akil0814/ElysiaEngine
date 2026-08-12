#define SDL_MAIN_HANDLED

#include "engine/tools/development_overlay_host.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using elysia::input::DevelopmentInputCapture;
using elysia::tests::require;
using elysia::tools::DevelopmentOverlayHost;
using elysia::tools::DevelopmentPanelHandle;
using elysia::tools::IDevelopmentOverlay;
using elysia::tools::InvalidDevelopmentPanelHandle;

struct FakeOverlayState
{
    bool fail_initialize = false;
    bool throw_initialize = false;
    int initialize_calls = 0;
    int process_event_calls = 0;
    int begin_frame_calls = 0;
    int render_calls = 0;
    int shutdown_calls = 0;
    double last_delta = 0.0;
    DevelopmentInputCapture capture =
        DevelopmentInputCapture::Keyboard | DevelopmentInputCapture::Pointer;
};

class FakeOverlay final : public IDevelopmentOverlay
{
public:
    explicit FakeOverlay(std::shared_ptr<FakeOverlayState> state)
        : _state(std::move(state))
    {
    }

    std::expected<void, std::string> initialize(
        SDL_Window&,
        SDL_Renderer&) override
    {
        ++_state->initialize_calls;
        if (_state->throw_initialize)
            throw std::runtime_error("thrown initialization failure");
        if (_state->fail_initialize)
            return std::unexpected("expected initialization failure");
        return {};
    }

    void process_event(const SDL_Event&) override
    {
        ++_state->process_event_calls;
    }

    void begin_frame(double delta_seconds) override
    {
        ++_state->begin_frame_calls;
        _state->last_delta = delta_seconds;
    }

    void render(SDL_Renderer&) override
    {
        ++_state->render_calls;
    }

    void shutdown() noexcept override
    {
        ++_state->shutdown_calls;
    }

    DevelopmentInputCapture captured_input() const noexcept override
    {
        return _state->capture;
    }

    DevelopmentPanelHandle register_panel(
        std::string,
        DrawCallback) override
    {
        return InvalidDevelopmentPanelHandle;
    }

    bool unregister_panel(DevelopmentPanelHandle) override
    {
        return false;
    }

private:
    std::shared_ptr<FakeOverlayState> _state;
};

class SdlFixture
{
public:
    SdlFixture()
    {
        require(SDL_Init(SDL_INIT_VIDEO) == 0,
            "overlay host tests must initialize SDL");
        _window = SDL_CreateWindow(
            "Development overlay host tests",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            320,
            180,
            SDL_WINDOW_HIDDEN);
        require(_window != nullptr,
            "overlay host tests must create an SDL window");
        _renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_SOFTWARE);
        require(_renderer != nullptr,
            "overlay host tests must create an SDL renderer");
    }

    ~SdlFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_DestroyWindow(_window);
        SDL_Quit();
    }

    SDL_Window& window() const noexcept { return *_window; }
    SDL_Renderer& renderer() const noexcept { return *_renderer; }

private:
    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;
};

[[nodiscard]] SDL_Event key_event(Uint32 type, SDL_Keycode key, Uint8 repeat = 0)
{
    SDL_Event event{};
    event.type = type;
    event.key.keysym.sym = key;
    event.key.repeat = repeat;
    return event;
}

void require_empty_host_is_noop(SdlFixture& fixture)
{
    DevelopmentOverlayHost host;
    require(!host.configured(), "an empty overlay host must not be configured");
    require(host.initialize(fixture.window(), fixture.renderer()).has_value(),
        "an empty overlay host must initialize as a no-op");
    require(!host.initialized(), "an empty overlay host must remain uninitialized");
    require(host.panel_registry() == nullptr,
        "an empty overlay host must not publish a panel registry");
    require(host.captured_input() == DevelopmentInputCapture::None,
        "an empty overlay host must not capture input");
}

void require_hidden_and_visible_lifecycle(SdlFixture& fixture)
{
    auto state = std::make_shared<FakeOverlayState>();
    DevelopmentOverlayHost host;
    host.set_overlay(std::make_unique<FakeOverlay>(state));
    require(host.configured(), "a host with an adapter must be configured");
    require(host.initialize(fixture.window(), fixture.renderer()).has_value(),
        "a fake overlay must initialize");
    require(host.initialized() && !host.visible(),
        "an initialized overlay must begin hidden");
    require(host.panel_registry() != nullptr,
        "an initialized overlay must publish its panel registry while hidden");

    const SDL_Event ordinary_event = key_event(SDL_KEYDOWN, SDLK_a);
    require(!host.process_event(ordinary_event),
        "ordinary hidden events must not be consumed");
    host.begin_frame(0.25);
    host.render(fixture.renderer());
    require(state->process_event_calls == 0
            && state->begin_frame_calls == 0
            && state->render_calls == 0,
        "a hidden overlay must perform no per-frame or backend event work");

    require(host.process_event(key_event(SDL_KEYDOWN, SDLK_F2)),
        "F2 down must be reserved by the overlay host");
    require(host.visible(), "F2 down must show the overlay");
    require(host.process_event(key_event(SDL_KEYUP, SDLK_F2)),
        "F2 up must also be reserved by the overlay host");
    require(host.captured_input()
            == (DevelopmentInputCapture::Keyboard
                | DevelopmentInputCapture::Pointer),
        "a visible host must publish the adapter's completed-frame capture");

    require(!host.process_event(ordinary_event),
        "visible ordinary events must still be available to the application host");
    host.begin_frame(1.0 / 120.0);
    host.render(fixture.renderer());
    require(state->process_event_calls == 1
            && state->begin_frame_calls == 1
            && state->render_calls == 1,
        "a visible overlay must receive events, frames, and rendering");
    require(state->last_delta == 1.0 / 120.0,
        "the overlay host must forward frame delta");

    require(host.process_event(key_event(SDL_KEYDOWN, SDLK_F2, 1)),
        "repeated F2 must remain consumed");
    require(host.visible(), "repeated F2 must not toggle visibility");
    require(host.process_event(key_event(SDL_KEYDOWN, SDLK_F2)),
        "a later F2 press must be consumed");
    require(!host.visible(), "a later F2 press must hide the overlay");
    require(host.captured_input() == DevelopmentInputCapture::None,
        "a hidden overlay must immediately release input capture");

    host.shutdown();
    host.shutdown();
    require(state->shutdown_calls == 1,
        "overlay host shutdown must be idempotent");
}

void require_failed_initialize_is_cleaned_up(SdlFixture& fixture)
{
    auto state = std::make_shared<FakeOverlayState>();
    state->fail_initialize = true;
    DevelopmentOverlayHost host;
    host.set_overlay(std::make_unique<FakeOverlay>(state));
    const auto result = host.initialize(fixture.window(), fixture.renderer());
    require(!result && result.error() == "expected initialization failure",
        "overlay host must preserve adapter initialization diagnostics");
    require(!host.initialized() && !host.visible(),
        "a failed adapter must not become active");
    require(state->shutdown_calls == 1,
        "a failed initialization must clean up partial adapter state");
}

void require_thrown_initialize_is_cleaned_up(SdlFixture& fixture)
{
    auto state = std::make_shared<FakeOverlayState>();
    state->throw_initialize = true;
    DevelopmentOverlayHost host;
    host.set_overlay(std::make_unique<FakeOverlay>(state));
    bool propagated = false;
    try
    {
        (void)host.initialize(fixture.window(), fixture.renderer());
    }
    catch (const std::runtime_error& error)
    {
        propagated = std::string(error.what()) == "thrown initialization failure";
    }
    require(propagated,
        "adapter initialization exceptions must remain visible to Application");
    require(state->shutdown_calls == 1 && !host.initialized(),
        "thrown initialization must clean up partial adapter state");
}
}

int main()
{
    SdlFixture fixture;
    require_empty_host_is_noop(fixture);
    require_hidden_and_visible_lifecycle(fixture);
    require_failed_initialize_is_cleaned_up(fixture);
    require_thrown_initialize_is_cleaned_up(fixture);
    return EXIT_SUCCESS;
}
