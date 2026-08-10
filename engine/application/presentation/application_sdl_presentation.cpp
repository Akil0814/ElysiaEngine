#include "application_sdl_presentation.h"

#include <SDL.h>

#include <string_view>

namespace elysia::application::detail
{
namespace
{
[[nodiscard]] std::expected<std::string_view,std::string>
resolve_scale_quality(ApplicationTextureFilter filter)
{
    switch (filter)
    {
    case ApplicationTextureFilter::Nearest:
        return "nearest";
    case ApplicationTextureFilter::Linear:
        return "linear";
    }

    return std::unexpected("Unsupported application texture filter.");
}

[[nodiscard]] std::string sdl_failure(
    std::string_view operation,
    const char* error)
{
    std::string message(operation);
    message += " failed";
    if (error && *error != '\0')
    {
        message += ": ";
        message += error;
    }
    message += '.';
    return message;
}
}

std::expected<void,std::string> configure_sdl_render_hints(
    const ApplicationRenderSettings& settings)
{
    const auto scale_quality = resolve_scale_quality(settings.texture_filter);
    if (!scale_quality)
        return std::unexpected(scale_quality.error());

    if (SDL_SetHintWithPriority(
            SDL_HINT_RENDER_SCALE_QUALITY,
            scale_quality->data(),
            SDL_HINT_OVERRIDE) != SDL_TRUE)
    {
        return std::unexpected(
            sdl_failure("SDL render scale quality hint",SDL_GetError()));
    }

    if (SDL_SetHintWithPriority(
            SDL_HINT_RENDER_LOGICAL_SIZE_MODE,
            "letterbox",
            SDL_HINT_OVERRIDE) != SDL_TRUE)
    {
        return std::unexpected(
            sdl_failure("SDL logical size mode hint",SDL_GetError()));
    }

    return {};
}

std::expected<void,std::string> configure_sdl_renderer_presentation(
    SDL_Renderer* renderer,
    int logical_width,
    int logical_height)
{
    if (!renderer)
        return std::unexpected("SDL renderer presentation requires a renderer.");

    if (SDL_RenderSetLogicalSize(
            renderer,
            logical_width,
            logical_height) != 0)
    {
        return std::unexpected(
            sdl_failure("SDL_RenderSetLogicalSize",SDL_GetError()));
    }

    if (SDL_RenderSetIntegerScale(renderer,SDL_FALSE) != 0)
    {
        return std::unexpected(
            sdl_failure("SDL_RenderSetIntegerScale",SDL_GetError()));
    }

    return {};
}
}
