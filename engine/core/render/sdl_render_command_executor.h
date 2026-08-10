#pragma once

#include <SDL.h>
#include <SDL2_gfxPrimitives.h>

#include "render_command.h"
#include "sdl_convert.h"
#include "sdl_ui_stroke_renderer.h"

#include <cmath>
#include <cassert>
#include <limits>
#include <vector>

namespace elysia::core
{
[[nodiscard]] inline Sint16 clamp_circle_component(float value) noexcept
{
    const long rounded = std::lround(value);
    if (rounded < static_cast<long>(std::numeric_limits<Sint16>::min()))
        return std::numeric_limits<Sint16>::min();
    if (rounded > static_cast<long>(std::numeric_limits<Sint16>::max()))
        return std::numeric_limits<Sint16>::max();
    return static_cast<Sint16>(rounded);
}

[[nodiscard]] inline SDL_RendererFlip to_sdl_renderer_flip(SpriteFlip flip) noexcept
{
    switch (flip)
    {
    case SpriteFlip::Horizontal:
        return SDL_FLIP_HORIZONTAL;

    case SpriteFlip::Vertical:
        return SDL_FLIP_VERTICAL;

    case SpriteFlip::Both:
        return static_cast<SDL_RendererFlip>(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);

    case SpriteFlip::None:
    default:
        return SDL_FLIP_NONE;
    }
}

inline void execute_textured_render_command(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const Rect& destination_rect_value,
    Uint8 alpha,
    const std::optional<TextureColorModulation>& texture_color_modulation,
    bool use_src_rect,
    const Rect& src_rect_value,
    double rotation_degrees,
    const Vector2& rotation_origin,
    SpriteFlip flip
) noexcept
{
    if (!renderer || !texture)
        return;

    SDL_Rect destination_rect = to_sdl_rect(destination_rect_value);
    SDL_Point sdl_rotation_origin{
        static_cast<int>(rotation_origin.x * destination_rect.w),
        static_cast<int>(rotation_origin.y * destination_rect.h)
    };

    const SDL_Rect* src_rect = nullptr;
    SDL_Rect converted_src_rect{};
    if (use_src_rect)
    {
        converted_src_rect = to_sdl_rect(src_rect_value);
        src_rect = &converted_src_rect;
    }

    Uint8 previous_alpha = 255;
    SDL_GetTextureAlphaMod(texture, &previous_alpha);
    SDL_SetTextureAlphaMod(texture, alpha);
    Uint8 previous_red = 255;
    Uint8 previous_green = 255;
    Uint8 previous_blue = 255;
    if (texture_color_modulation)
    {
        SDL_GetTextureColorMod(
            texture,
            &previous_red,
            &previous_green,
            &previous_blue);
        SDL_SetTextureColorMod(
            texture,
            texture_color_modulation->r,
            texture_color_modulation->g,
            texture_color_modulation->b);
    }

    SDL_RenderCopyEx(
        renderer,
        texture,
        src_rect,
        &destination_rect,
        rotation_degrees,
        &sdl_rotation_origin,
        to_sdl_renderer_flip(flip)
    );

    if (texture_color_modulation)
    {
        SDL_SetTextureColorMod(
            texture,
            previous_red,
            previous_green,
            previous_blue);
    }
    SDL_SetTextureAlphaMod(texture, previous_alpha);
}

inline void execute_render_command(SDL_Renderer* renderer, const ScreenRenderCommand& render_command) noexcept
{
    execute_textured_render_command(
        renderer,
        render_command.texture,
        render_command.screen_rect,
        render_command.alpha,
        render_command.texture_color_modulation,
        render_command.use_src_rect,
        render_command.src_rect,
        render_command.rotation_degrees,
        render_command.rotation_origin,
        render_command.flip
    );
}

inline void execute_render_command(SDL_Renderer* renderer, const UiRenderCommand& render_command) noexcept
{
    if (!renderer)
        return;

    SDL_Rect previous_clip_rect{};
    const bool had_clip_rect = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    if (had_clip_rect)
        SDL_RenderGetClipRect(renderer, &previous_clip_rect);

    if (render_command.use_clip_rect)
    {
        const SDL_Rect clip_rect = to_sdl_covering_rect(render_command.clip_rect);
        SDL_RenderSetClipRect(renderer, &clip_rect);
    }
    else if (had_clip_rect)
    {
        SDL_RenderSetClipRect(renderer, nullptr);
    }

    switch (render_command.type)
    {
    case UiRenderCommandType::Texture:
        execute_textured_render_command(
            renderer,
            render_command.texture,
            render_command.screen_rect,
            render_command.alpha,
            render_command.texture_color_modulation,
            render_command.use_src_rect,
            render_command.src_rect,
            render_command.rotation_degrees,
            render_command.rotation_origin,
            render_command.flip
        );
        break;

    case UiRenderCommandType::FillRect:
    {
        if (render_command.screen_rect.is_empty())
            break;

        SDL_Rect rect = to_sdl_rect(render_command.screen_rect);
        const SDL_Color color = to_sdl_color(render_command.color);

        Uint8 old_r = 0;
        Uint8 old_g = 0;
        Uint8 old_b = 0;
        Uint8 old_a = 0;
        SDL_GetRenderDrawColor(renderer, &old_r, &old_g, &old_b, &old_a);

        SDL_SetRenderDrawColor(
            renderer,
            color.r,
            color.g,
            color.b,
            color.a
        );

        SDL_RenderFillRect(renderer, &rect);

        SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
        break;
    }

    case UiRenderCommandType::DrawRect:
        detail::render_ui_rect_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::FillRoundedRect:
    {
        if (render_command.screen_rect.is_empty())
            break;

        assert(std::isfinite(render_command.corner_radius));
        assert(render_command.corner_radius > 0.0f);
        assert(render_command.corner_radius <= 0.5f * std::min(
            render_command.screen_rect.width(),render_command.screen_rect.height()));

        const SDL_Rect rect = to_sdl_rect(render_command.screen_rect);
        const SDL_Color color = to_sdl_color(render_command.color);
        const Sint16 x1 = clamp_circle_component(static_cast<float>(rect.x));
        const Sint16 y1 = clamp_circle_component(static_cast<float>(rect.y));
        const Sint16 x2 = clamp_circle_component(static_cast<float>(rect.x + rect.w - 1));
        const Sint16 y2 = clamp_circle_component(static_cast<float>(rect.y + rect.h - 1));
        const Sint16 radius = clamp_circle_component(render_command.corner_radius);

        roundedBoxRGBA(renderer,x1,y1,x2,y2,radius,color.r,color.g,color.b,color.a);
        break;
    }

    case UiRenderCommandType::DrawRoundedRect:
        detail::render_ui_rounded_rect_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::DrawLine:
        detail::render_ui_line_stroke(renderer,render_command);
        break;

    case UiRenderCommandType::FillCircle:
    {
        const Sint16 radius = clamp_circle_component(render_command.circle_radius);
        if (radius <= 0)
            break;

        const SDL_Color color = to_sdl_color(render_command.color);
        const Sint16 x = clamp_circle_component(render_command.circle_center.x);
        const Sint16 y = clamp_circle_component(render_command.circle_center.y);

        filledCircleRGBA(renderer, x, y, radius, color.r, color.g, color.b, color.a);
        break;
    }

    case UiRenderCommandType::DrawCircle:
        detail::render_ui_circle_stroke(renderer,render_command);
        break;
    }

    if (had_clip_rect)
        SDL_RenderSetClipRect(renderer, &previous_clip_rect);
    else
        SDL_RenderSetClipRect(renderer, nullptr);
}

inline void execute_render_commands(
    SDL_Renderer* renderer,
    const std::vector<ScreenRenderCommand>& render_commands
) noexcept
{
    for (const ScreenRenderCommand& render_command : render_commands)
        execute_render_command(renderer, render_command);
}

inline void execute_render_commands(
    SDL_Renderer* renderer,
    const std::vector<UiRenderCommand>& render_commands
) noexcept
{
    for (const UiRenderCommand& render_command : render_commands)
        execute_render_command(renderer, render_command);
}

}
