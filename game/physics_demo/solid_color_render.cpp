#include "solid_color_render.h"

#include "../../engine/core/render/render_command.h"
#include "../../engine/core/render/colors.h"

#include <SDL.h>
#include <algorithm>

namespace example::physics_demo
{
bool SolidColorTexture::ensure(SDL_Renderer* renderer)
{
    if (!renderer)
        return false;
    if (_texture && _renderer == renderer)
        return true;

    reset();
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface)
        return false;
    const Uint32 white = SDL_MapRGBA(surface->format, 255, 255, 255, 255);
    *static_cast<Uint32*>(surface->pixels) = white;
    SDL_Texture* raw = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!raw)
        return false;
    SDL_SetTextureBlendMode(raw, SDL_BLENDMODE_BLEND);
    _texture.reset(raw);
    _renderer = renderer;
    return true;
}

void SolidColorTexture::reset() noexcept
{
    _texture.reset();
    _renderer = nullptr;
}

ColoredBlockObject::ColoredBlockObject(
    SolidColorTexture& texture,
    elysia::core::DepthLayer layer,
    const elysia::core::Rect& rect,
    elysia::core::Color color,
    int order) noexcept
    : GameObject(layer, order), _texture(&texture), _base_color(color)
{
    set_world_rect(rect);
}

void ColoredBlockObject::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    if (!_texture || !_texture->get() || world_rect().is_empty())
        return;
    const auto color = display_color();
    elysia::core::RenderCommand command;
    command.texture = _texture->get();
    command.command_rect = render_rect();
    command.alpha = static_cast<std::uint8_t>(
        (static_cast<unsigned>(_alpha) * color.a) / 255u);
    command.texture_color_modulation = elysia::core::TextureColorModulation{
        color.r, color.g, color.b};
    out_commands.push_back(command);
}

void ColoredBlockObject::flash(double seconds) noexcept
{
    _flash_remaining = std::max(_flash_remaining, std::max(0.0, seconds));
}

void ColoredBlockObject::update_visual(double delta) noexcept
{
    _flash_remaining = std::max(0.0, _flash_remaining - std::max(0.0, delta));
}

elysia::core::Color ColoredBlockObject::display_color() const noexcept
{
    if (_dead_visual)
        return elysia::core::colors::gray_500;
    if (_flash_remaining > 0.0)
        return elysia::core::colors::white;
    return _base_color;
}
}
