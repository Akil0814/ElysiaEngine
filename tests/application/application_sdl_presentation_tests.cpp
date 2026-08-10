#define SDL_MAIN_HANDLED

#include "engine/application/presentation/application_sdl_presentation.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <cstdlib>
#include <string>

namespace
{
using elysia::application::ApplicationRenderSettings;
using elysia::application::ApplicationTextureFilter;
using elysia::application::detail::configure_sdl_render_hints;
using elysia::application::detail::configure_sdl_renderer_presentation;
using elysia::tests::require;

class SdlPresentationFixture
{
public:
    SdlPresentationFixture()
    {
        require(
            SDL_Init(SDL_INIT_VIDEO) == 0,
            "SDL presentation tests must initialize SDL video");

        _surface = SDL_CreateRGBSurfaceWithFormat(
            0,
            1280,
            720,
            32,
            SDL_PIXELFORMAT_RGBA32);
        require(
            _surface != nullptr,
            "SDL presentation tests must create a software surface");

        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(
            _renderer != nullptr,
            "SDL presentation tests must create a software renderer");
    }

    ~SdlPresentationFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept
    {
        return _renderer;
    }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};

void require_aspect_fit(SDL_Renderer* renderer)
{
    const auto result = configure_sdl_renderer_presentation(
        renderer,
        640,
        360);
    require(result.has_value(), "SDL renderer presentation must be applied");

    int logical_width = 0;
    int logical_height = 0;
    SDL_RenderGetLogicalSize(renderer,&logical_width,&logical_height);
    require(
        logical_width == 640 && logical_height == 360,
        "SDL renderer presentation must retain the logical size");
    require(
        SDL_RenderGetIntegerScale(renderer) == SDL_FALSE,
        "SDL renderer presentation must use aspect-fit scaling");
}

void require_texture_filter(
    SDL_Renderer* renderer,
    ApplicationTextureFilter filter,
    SDL_ScaleMode expected_mode)
{
    const ApplicationRenderSettings settings{
        .texture_filter = filter
    };
    const auto hint_result = configure_sdl_render_hints(settings);
    require(hint_result.has_value(), "SDL render hints must be applied");

    const char* logical_size_mode =
        SDL_GetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE);
    require(
        logical_size_mode
            && std::string(logical_size_mode) == "letterbox",
        "SDL logical size mode must be letterbox");

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        8,
        8);
    require(texture != nullptr, "SDL presentation tests must create a texture");

    SDL_ScaleMode actual_mode = SDL_ScaleModeNearest;
    require(
        SDL_GetTextureScaleMode(texture,&actual_mode) == 0,
        "SDL presentation tests must query texture scale mode");
    require(
        actual_mode == expected_mode,
        "new SDL textures must inherit the requested scale mode");

    SDL_DestroyTexture(texture);
}

void require_invalid_settings_fail()
{
    const ApplicationRenderSettings invalid_filter{
        .texture_filter = static_cast<ApplicationTextureFilter>(255)
    };
    require(
        !configure_sdl_render_hints(invalid_filter).has_value(),
        "unknown texture filters must fail SDL hint configuration");
}
}

int main()
{
    SdlPresentationFixture fixture;

    require_aspect_fit(fixture.renderer());

    require_texture_filter(
        fixture.renderer(),
        ApplicationTextureFilter::Nearest,
        SDL_ScaleModeNearest);
    require_texture_filter(
        fixture.renderer(),
        ApplicationTextureFilter::Linear,
        SDL_ScaleModeLinear);

    require_invalid_settings_fail();
    return EXIT_SUCCESS;
}
