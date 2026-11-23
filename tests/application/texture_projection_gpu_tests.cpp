#define SDL_MAIN_HANDLED

#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
using namespace elysia::core;
using elysia::tests::require;
using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

class Fixture
{
public:
    Fixture()
    {
        require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        window = SDL_CreateWindow("Texture projection regression", 320, 240, SDL_WINDOW_HIDDEN);
        require(window != nullptr, SDL_GetError());
        renderer = SDL_CreateGPURenderer(nullptr, window);
        require(renderer != nullptr, SDL_GetError());
        Surface source(SDL_CreateSurface(40, 40, SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        require(source != nullptr, SDL_GetError());
        require(SDL_FillSurfaceRect(source.get(), nullptr,
            SDL_MapSurfaceRGBA(source.get(), 255, 255, 255, 255)), SDL_GetError());
        texture = SDL_CreateTextureFromSurface(renderer, source.get());
        require(texture != nullptr, SDL_GetError());
        require(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST), SDL_GetError());
    }

    ~Fixture()
    {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void clear()
    {
        require(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255), SDL_GetError());
        require(SDL_RenderClear(renderer), SDL_GetError());
    }

    Surface capture()
    {
        Surface pixels(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
        require(pixels != nullptr, SDL_GetError());
        require(pixels->w == 320 && pixels->h == 240, "readback must cover the output");
        return pixels;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
};

Color pixel(SDL_Surface* surface, int x, int y)
{
    Color result;
    require(SDL_ReadSurfacePixel(surface, x, y,
        &result.r, &result.g, &result.b, &result.a), SDL_GetError());
    return result;
}

void test_adjacent_tiles(Fixture& fixture)
{
    for (const float zoom : {0.63f, 0.77f, 1.13f})
    for (const float shift : {0.0f, 0.37f, -0.41f})
    {
        fixture.clear();
        const elysia::camera::Camera camera({200 + shift, 160 + shift}, {320, 240}, zoom);
        for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 10; ++x)
        {
            RenderCommand tile;
            tile.texture = fixture.texture;
            tile.command_rect = Rect{float(x * 40), float(y * 40), 40, 40};
            // Exercise the sprite-sheet path too, using an integer source region.
            tile.use_src_rect = true;
            tile.src_rect = Rect{0, 0, 40, 40};
            require_render_success(execute_render_command(fixture.renderer,
                project_render_command_to_screen(tile, camera)));
        }
        const auto pixels = fixture.capture();
        const auto bounds = camera.world_to_screen(Rect{0, 0, 400, 320});
        int holes = 0;
        // Exclude the map's outer rasterization boundary, retaining every internal seam.
        for (int y = std::max(0, int(std::ceil(bounds.top())) + 2);
            y < std::min(240, int(std::floor(bounds.bottom())) - 2); ++y)
        for (int x = std::max(0, int(std::ceil(bounds.left())) + 2);
            x < std::min(320, int(std::floor(bounds.right())) - 2); ++x)
        {
            const auto color = pixel(pixels.get(), x, y);
            holes += color.r != 255 || color.g != 255 || color.b != 255;
        }
        std::cout << "zoom=" << zoom << " shift=" << shift << " holes=" << holes << std::endl;
        require(holes == 0, "adjacent opaque tiles must completely cover the map interior");
    }
}

void test_fractional_rotation_origin(Fixture& fixture)
{
    // Compare pixel coverage with SDL's floating-point contract. Noncentral origins
    // and fractional sizes expose pivot truncation even after destination truncation is fixed.
    for (const double angle : {31.0, 90.0, 180.0})
    for (const bool ui : {false, true})
    {
        fixture.clear();
        ScreenRenderCommand command;
        command.texture = fixture.texture;
        command.screen_rect = Rect{100.25f, 80.75f, 51.5f, 33.5f};
        command.rotation_origin = {0.37f, 0.61f};
        command.rotation_degrees = angle;
        if (ui)
        {
            UiRenderCommand ui_command;
            ui_command.texture = command.texture;
            ui_command.screen_rect = command.screen_rect;
            ui_command.rotation_origin = command.rotation_origin;
            ui_command.rotation_degrees = angle;
            require_render_success(execute_render_command(fixture.renderer, ui_command));
        }
        else
            require_render_success(execute_render_command(fixture.renderer, command));
        const auto actual = fixture.capture();

        fixture.clear();
        const SDL_FRect rect{100.25f, 80.75f, 51.5f, 33.5f};
        const SDL_FPoint origin{0.37f * 51.5f, 0.61f * 33.5f};
        require(SDL_RenderTextureRotated(fixture.renderer, fixture.texture, nullptr,
            &rect, angle, &origin, SDL_FLIP_NONE), SDL_GetError());
        const auto expected = fixture.capture();
        int differences = 0;
        int foreground = 0;
        for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 320; ++x)
        {
            const auto reference = pixel(expected.get(), x, y);
            differences += pixel(actual.get(), x, y) != reference;
            foreground += reference.r != 0;
        }
        std::cout << "angle=" << angle << " ui=" << ui
            << " differing_pixels=" << differences << std::endl;
        require(foreground > 1000, "rotation comparison must include a visible texture");
        require(differences == 0, "texture destination and rotation origin must preserve subpixels");
    }
}
}

int main(int argc, char** argv)
{
    Fixture fixture;
    if (argc == 1 || std::string(argv[1]) != "--rotation-only")
        test_adjacent_tiles(fixture);
    test_fractional_rotation_origin(fixture);
    return EXIT_SUCCESS;
}
