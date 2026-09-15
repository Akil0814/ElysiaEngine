#define SDL_MAIN_HANDLED

#include "engine/tools/imgui/imgui_development_overlay.h"
#include "tests/support/test_assertions.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cstdlib>
#include <stdexcept>

namespace
{
using elysia::input::DevelopmentInputCapture;
using elysia::tests::require;
using elysia::tools::IDevelopmentPanelRegistry;
using elysia::tools::ImGuiDevelopmentOverlay;

class SdlFixture
{
public:
    SdlFixture()
    {
        require(SDL_Init(SDL_INIT_VIDEO),
            "ImGui integration tests must initialize SDL");
        _window = SDL_CreateWindow("ImGui development overlay tests", 640, 360, SDL_WINDOW_HIDDEN);
        require(_window != nullptr,
            "ImGui integration tests must create an SDL window");
        _renderer = SDL_CreateRenderer(_window, "software");
        require(_renderer != nullptr,
            "ImGui integration tests must create an SDL renderer");
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

void draw_test_window(const char* title)
{
    ImGui::Begin(title);
    ImGui::TextUnformatted("Development overlay integration test");
    ImGui::End();
}

void require_context_configuration_and_registry(SdlFixture& fixture)
{
    ImGuiDevelopmentOverlay overlay;
    require(overlay.initialize(fixture.window(), fixture.renderer()).has_value(),
        "the ImGui adapter must initialize on a hidden SDL renderer");

    const ImGuiIO& io = ImGui::GetIO();
    require((io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0,
        "the adapter must enable keyboard navigation");
    require((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0,
        "the adapter must leave gamepad navigation disabled");
    require(!overlay.register_panel("", [] {}).is_valid(),
        "an empty panel ID must be rejected");
    require(!overlay.register_panel(
            "empty.callback", IDevelopmentPanelRegistry::DrawCallback{}).is_valid(),
        "an empty panel callback must be rejected");

    int first_draws = 0;
    int second_draws = 0;
    int pending_draws = 0;
    float observed_delta = 0.0f;
    bool changed_during_draw = false;
    elysia::tools::DevelopmentPanelHandle second_handle{};

    const auto first_handle = overlay.register_panel(
        "first",
        [&]
        {
            ++first_draws;
            observed_delta = ImGui::GetIO().DeltaTime;
            ImGui::GetIO().WantCaptureKeyboard = true;
            ImGui::GetIO().WantCaptureMouse = true;
            draw_test_window("First panel");
            if (!changed_during_draw)
            {
                changed_during_draw = true;
                require(overlay.unregister_panel(second_handle),
                    "drawing code must be able to queue panel removal");
                const auto pending_handle = overlay.register_panel(
                    "pending",
                    [&]
                    {
                        ++pending_draws;
                        draw_test_window("Pending panel");
                    });
                require(pending_handle.is_valid(),
                    "drawing code must be able to queue panel registration");
            }
        });
    second_handle = overlay.register_panel(
        "second",
        [&]
        {
            ++second_draws;
            draw_test_window("Second panel");
        });
    require(first_handle.is_valid() && second_handle.is_valid()
            && second_handle.value > first_handle.value,
        "panel handles must be valid, monotonic, and not reused");
    require(!overlay.register_panel("first", [] {}).is_valid(),
        "duplicate stable panel IDs must be rejected");

    overlay.begin_frame(1.0 / 60.0);
    overlay.render(fixture.renderer());
    require(first_draws == 1 && second_draws == 1 && pending_draws == 0,
        "draw-time registry mutations must not change the current snapshot");
    require(observed_delta == static_cast<float>(1.0 / 60.0),
        "the adapter must apply the host frame delta before panel drawing");
    require(elysia::input::captures_development_input(
                overlay.captured_input(), DevelopmentInputCapture::Keyboard)
            && elysia::input::captures_development_input(
                overlay.captured_input(), DevelopmentInputCapture::Pointer),
        "completed ImGui frames must publish keyboard and pointer capture");

    overlay.begin_frame(1.0 / 120.0);
    overlay.render(fixture.renderer());
    require(first_draws == 2 && second_draws == 1 && pending_draws == 1,
        "draw-time registry mutations must become visible on the next frame");
    require(!elysia::input::captures_development_input(
            overlay.captured_input(), DevelopmentInputCapture::Gamepad),
        "the ImGui adapter must never capture gamepad input");

    require(overlay.unregister_panel(first_handle),
        "registered panels must be removable");
    require(!overlay.unregister_panel(first_handle),
        "removing the same panel twice must fail safely");
    overlay.shutdown();
    overlay.shutdown();
}

void require_renderer_state_restoration(SdlFixture& fixture)
{
    ImGuiDevelopmentOverlay overlay;
    require(overlay.initialize(fixture.window(), fixture.renderer()).has_value(),
        "renderer-state test adapter must initialize");
    const auto panel = overlay.register_panel(
        "renderer.state", [] { draw_test_window("Renderer state"); });
    require(panel.is_valid(), "renderer-state test panel must register");

    SDL_Renderer& renderer = fixture.renderer();
    SDL_Texture* target = SDL_CreateTexture(
        &renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        320,
        180);
    require(target != nullptr, "renderer-state test must create a target texture");
    require(SDL_SetRenderTarget(&renderer, target),
        "renderer-state test must bind its target texture");
    require(SDL_SetRenderLogicalPresentation(&renderer, 320, 180, SDL_LOGICAL_PRESENTATION_LETTERBOX),
        "renderer-state test must set logical size");
    require(SDL_SetRenderLogicalPresentation(&renderer,320,180,SDL_LOGICAL_PRESENTATION_INTEGER_SCALE),
        "renderer-state test must set integer scale");
    const SDL_Rect viewport{10, 12, 200, 100};
    const SDL_Rect clip{20, 22, 40, 30};
    SDL_SetRenderViewport(&renderer, &viewport);
    SDL_SetRenderClipRect(&renderer, &clip);
    require(SDL_SetRenderScale(&renderer, 1.5f, 0.75f),
        "renderer-state test must set renderer scale");
    require(SDL_SetRenderDrawColor(&renderer, 12, 34, 56, 78),
        "renderer-state test must set draw color");
    require(SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_ADD),
        "renderer-state test must set blend mode");

    SDL_Rect expected_viewport{};
    SDL_GetRenderViewport(&renderer, &expected_viewport);
    SDL_Rect expected_clip{};
    SDL_GetRenderClipRect(&renderer, &expected_clip);
    const bool expected_clip_enabled =
        SDL_RenderClipEnabled(&renderer) == true;

    overlay.begin_frame(1.0 / 60.0);
    overlay.render(renderer);

    require(SDL_GetRenderTarget(&renderer) == target,
        "ImGui rendering must restore the SDL render target");
    int logical_width = 0;
    int logical_height = 0;
    SDL_GetRenderLogicalPresentation(&renderer,&logical_width,&logical_height,nullptr);
    require(logical_width == 320 && logical_height == 180,
        "ImGui rendering must restore logical size");
    SDL_RendererLogicalPresentation mode{};
    SDL_GetRenderLogicalPresentation(&renderer,nullptr,nullptr,&mode);
    require(mode == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE,
        "ImGui rendering must restore integer scale");
    SDL_Rect actual_viewport{};
    SDL_GetRenderViewport(&renderer, &actual_viewport);
    require(actual_viewport.x == expected_viewport.x
            && actual_viewport.y == expected_viewport.y
            && actual_viewport.w == expected_viewport.w
            && actual_viewport.h == expected_viewport.h,
        "ImGui rendering must restore viewport");
    SDL_Rect actual_clip{};
    SDL_GetRenderClipRect(&renderer, &actual_clip);
    require((SDL_RenderClipEnabled(&renderer) == true)
                == expected_clip_enabled
            && actual_clip.x == expected_clip.x
            && actual_clip.y == expected_clip.y
            && actual_clip.w == expected_clip.w
            && actual_clip.h == expected_clip.h,
        "ImGui rendering must restore clipping");
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    SDL_GetRenderScale(&renderer, &scale_x, &scale_y);
    require(scale_x == 1.5f && scale_y == 0.75f,
        "ImGui rendering must restore renderer scale");
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    Uint8 alpha = 0;
    SDL_GetRenderDrawColor(&renderer, &red, &green, &blue, &alpha);
    require(red == 12 && green == 34 && blue == 56 && alpha == 78,
        "ImGui rendering must restore draw color");
    SDL_BlendMode blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(&renderer, &blend);
    require(blend == SDL_BLENDMODE_ADD,
        "ImGui rendering must restore blend mode");

    overlay.shutdown();
    SDL_SetRenderTarget(&renderer, nullptr);
    SDL_DestroyTexture(target);
}

void require_panel_exception_propagates(SdlFixture& fixture)
{
    ImGuiDevelopmentOverlay overlay;
    require(overlay.initialize(fixture.window(), fixture.renderer()).has_value(),
        "exception propagation adapter must initialize");
    require(overlay.register_panel(
            "throwing", [] { throw std::runtime_error("panel failure"); }).is_valid(),
        "throwing panel must register normally");
    overlay.begin_frame(1.0 / 60.0);
    bool propagated = false;
    try
    {
        overlay.render(fixture.renderer());
    }
    catch (const std::runtime_error& error)
    {
        propagated = std::string(error.what()) == "panel failure";
    }
    require(propagated,
        "panel callback exceptions must propagate to the Application render boundary");
    overlay.shutdown();
}

void require_window_coordinate_rendering_after_resize(SdlFixture& fixture)
{
    ImGuiDevelopmentOverlay overlay;
    auto& renderer = fixture.renderer();
    require(overlay.initialize(fixture.window(), renderer).has_value(),
        "resize test adapter must initialize");
    bool hovered = false;
    require(overlay.register_panel("resize.hit_target", [&]
    {
        ImGui::SetNextWindowPos(ImVec2(40, 40));
        ImGui::SetNextWindowSize(ImVec2(200, 140));
        ImGui::Begin("Resize target", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorScreenPos(ImVec2(80, 80));
        ImGui::InvisibleButton("target", ImVec2(40, 40));
        hovered = ImGui::IsItemHovered();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(80, 80), ImVec2(120, 120), IM_COL32(255, 0, 0, 255));
        ImGui::End();
    }).is_valid(), "resize test panel must register");

    const SDL_Point sizes[] = {{640, 360}, {960, 540}, {800, 600}, {480, 270}, {640, 360}};
    for (const auto size : sizes)
    {
        require(SDL_SetWindowSize(&fixture.window(), size.x, size.y),
            "resize test must change window size");
        SDL_SyncWindow(&fixture.window());
        SDL_PumpEvents();
        require(SDL_SetRenderLogicalPresentation(&renderer, 320, 180,
                SDL_LOGICAL_PRESENTATION_LETTERBOX),
            "resize test must configure game logical presentation");
        SDL_SetRenderScale(&renderer, 1.25f, 0.75f);
        SDL_SetRenderViewport(&renderer, nullptr);
        SDL_SetRenderClipRect(&renderer, nullptr);

        // The first frame establishes the window for ImGui's next hit test.
        for (int frame = 0; frame < 2; ++frame)
        {
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = SDL_GetWindowID(&fixture.window());
            motion.motion.x = 100.0f;
            motion.motion.y = 100.0f;
            overlay.process_event(motion);
            SDL_SetRenderDrawColor(&renderer, 0, 0, 0, 255);
            SDL_RenderClear(&renderer);
            overlay.begin_frame(1.0 / 60.0);
            overlay.render(renderer);
        }
        require(hovered, "window-coordinate mouse input must hit the overlay target");
        require(!SDL_RenderViewportSet(&renderer),
            "the automatic viewport must remain automatic after overlay rendering");

        // Read the physical framebuffer independently of the restored game viewport.
        const ImVec2 framebuffer_scale = ImGui::GetIO().DisplayFramebufferScale;
        SDL_SetRenderLogicalPresentation(&renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_SetRenderScale(&renderer, 1.0f, 1.0f);
        SDL_Surface* pixels = SDL_RenderReadPixels(&renderer, nullptr);
        require(pixels != nullptr, "resize test must read rendered pixels");
        Uint8 red = 0, green = 0, blue = 0, alpha = 0;
        require(SDL_ReadSurfacePixel(pixels,
                static_cast<int>(100 * framebuffer_scale.x),
                static_cast<int>(100 * framebuffer_scale.y),
                &red, &green, &blue, &alpha),
            "resize test must sample the mouse target position");
        SDL_DestroySurface(pixels);
        require(red == 255 && green == 0 && blue == 0,
            "the visible overlay target must coincide with its mouse hit position after resizing");
    }
}
}

int main()
{
    SdlFixture fixture;
    require_context_configuration_and_registry(fixture);
    require_renderer_state_restoration(fixture);
    require_panel_exception_propagates(fixture);
    require_window_coordinate_rendering_after_resize(fixture);
    return EXIT_SUCCESS;
}
