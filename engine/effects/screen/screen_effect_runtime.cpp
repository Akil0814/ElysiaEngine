#include "screen_effect_runtime.h"
#include "../../core/render/sdl_render_command_executor.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace elysia::effects
{
namespace
{
// Screen overlays always alpha-blend, including opaque RGB/JPEG textures.
// Restore the borrowed texture's blend mode even if command execution fails.
class ScreenTextureBlendState
{
public:
    explicit ScreenTextureBlendState(SDL_Texture* texture) : _texture(texture)
    {
        elysia::core::detail::checked_render_operation("SDL_GetTextureBlendMode", [&] {
            return SDL_GetTextureBlendMode(texture, &_blend);
        });
    }
    void apply()
    {
        _changed = true;
        elysia::core::detail::checked_render_operation("SDL_SetTextureBlendMode", [&] {
            return SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_BLEND);
        });
    }
    template<typename Restore>
    void restore(Restore&& restore)
    {
        if (_changed) restore([&] {
            elysia::core::detail::checked_render_operation("restore.SDL_SetTextureBlendMode", [&] {
                return SDL_SetTextureBlendMode(_texture, _blend);
            });
        });
    }
private:
    SDL_Texture* _texture;
    SDL_BlendMode _blend{};
    bool _changed = false;
};
}
bool ScreenEffectRuntime::valid(const ScreenEffectPlayback& p) noexcept
{
    const auto time = [](double v) { return std::isfinite(v) && v >= 0; };
    return time(p.fade_in_seconds) && time(p.hold_seconds) && time(p.fade_out_seconds)
        && std::isfinite(p.target_opacity) && p.target_opacity >= 0 && p.target_opacity <= 1
        && (p.end == ScreenEffectEnd::Timed || p.end == ScreenEffectEnd::Manual)
        && (p.layer == ScreenEffectLayer::BeforeUi || p.layer == ScreenEffectLayer::AfterUi)
        && (p.clock == ScreenEffectClock::Unscaled || p.clock == ScreenEffectClock::Scene)
        && (p.end == ScreenEffectEnd::Manual || p.fade_in_seconds > 0 || p.hold_seconds > 0 || p.fade_out_seconds > 0);
}

ScreenEffectHandle ScreenEffectRuntime::insert(Effect effect)
{
    effect.order = ++_order;
    effect.opacity = effect.playback.fade_in_seconds == 0 ? effect.playback.target_opacity : 0;
    for (std::size_t i = 0; i < _slots.size(); ++i)
        if (!_slots[i].effect && _slots[i].generation != 0)
        {
            _slots[i].effect = std::move(effect);
            return {static_cast<std::uint32_t>(i), _slots[i].generation};
        }
    _slots.push_back({1, std::move(effect)});
    return {static_cast<std::uint32_t>(_slots.size() - 1), 1};
}

std::optional<ScreenEffectHandle> ScreenEffectRuntime::create(const ScreenColorEffectRequest& r, const void* scene,
    std::optional<std::size_t> frame)
{
    if (!scene || !valid(r.playback)) return {};
    Effect e;
    e.playback = r.playback; e.scene = scene; e.color = r.color;
    e.created_frame = frame;
    return insert(std::move(e));
}

std::optional<ScreenEffectHandle> ScreenEffectRuntime::create(const ScreenImageEffectRequest& r, SDL_Texture* texture, const void* scene,
    std::optional<std::size_t> frame)
{
    if (!scene || !texture || !valid(r.playback)
        || (r.fit != ScreenEffectFit::Stretch && r.fit != ScreenEffectFit::Cover && r.fit != ScreenEffectFit::Contain)) return {};
    Effect e;
    if (!SDL_GetTextureSize(texture, &e.width, &e.height) || e.width <= 0 || e.height <= 0) return {};
    e.playback = r.playback; e.scene = scene; e.texture = texture; e.fit = r.fit;
    e.created_frame = frame;
    return insert(std::move(e));
}

ScreenEffectRuntime::Effect* ScreenEffectRuntime::find(ScreenEffectHandle h) noexcept
{
    if (!active(h)) return nullptr;
    return &*_slots[h.slot].effect;
}
bool ScreenEffectRuntime::active(ScreenEffectHandle h) const noexcept
{
    return h.generation != 0 && h.slot < _slots.size() && _slots[h.slot].generation == h.generation && _slots[h.slot].effect.has_value();
}
void ScreenEffectRuntime::retire(Slot& slot) noexcept
{
    slot.effect.reset();
    // A wrapped generation permanently retires the slot.
    ++slot.generation;
}
bool ScreenEffectRuntime::cancel(ScreenEffectHandle h) noexcept
{
    if (!active(h)) return false;
    retire(_slots[h.slot]); return true;
}
bool ScreenEffectRuntime::stop(ScreenEffectHandle h) noexcept
{
    auto* e = find(h);
    if (!e || e->stopping) return false;
    if (e->playback.end == ScreenEffectEnd::Timed
        && e->elapsed >= e->playback.fade_in_seconds
        && e->elapsed - e->playback.fade_in_seconds >= e->playback.hold_seconds) return false;
    if (e->playback.fade_out_seconds == 0) return cancel(h);
    e->stopping = true; e->stop_opacity = e->opacity; e->elapsed = 0;
    return true;
}
void ScreenEffectRuntime::update(double raw, double scaled, bool paused, std::optional<std::size_t> frame) noexcept
{
    for (auto& slot : _slots)
    {
        if (!slot.effect) continue;
        auto& e = *slot.effect;
        if (frame && e.created_frame == frame) continue;
        const auto& p = e.playback;
        double delta = p.clock == ScreenEffectClock::Unscaled ? raw : (paused ? 0 : scaled);
        if (!std::isfinite(delta) || delta < 0) delta = 0;
        e.elapsed += delta;
        if (e.stopping)
        {
            if (e.elapsed >= p.fade_out_seconds) { retire(slot); continue; }
            e.opacity = e.stop_opacity * (1 - e.elapsed / p.fade_out_seconds);
        }
        else if (e.elapsed < p.fade_in_seconds)
            e.opacity = p.target_opacity * e.elapsed / p.fade_in_seconds;
        else
        {
            const double after_in = e.elapsed - p.fade_in_seconds;
            if (p.end == ScreenEffectEnd::Manual || after_in < p.hold_seconds)
                e.opacity = p.target_opacity;
            else
            {
                const double after_hold = after_in - p.hold_seconds;
                if (after_hold >= p.fade_out_seconds) { retire(slot); continue; }
                e.opacity = p.target_opacity * (1 - after_hold / p.fade_out_seconds);
            }
        }
    }
}
void ScreenEffectRuntime::unbind(const void* scene) noexcept
{
    for (auto& slot : _slots)
        if (slot.effect && slot.effect->scene == scene)
        {
            retire(slot);
        }
}
void ScreenEffectRuntime::clear() noexcept
{
    for (auto& slot : _slots) if (slot.effect) retire(slot);
}
void ScreenEffectRuntime::append_commands(ScreenEffectLayer layer, const elysia::core::Rect& viewport,
    std::vector<elysia::core::UiRenderCommand>& out) const
{
    if (viewport.width() <= 0 || viewport.height() <= 0) return;
    std::vector<const Effect*> ordered;
    for (const auto& slot : _slots)
        if (slot.effect && slot.effect->playback.layer == layer) ordered.push_back(&*slot.effect);
    std::ranges::sort(ordered, {}, &Effect::order);
    for (const auto* e : ordered)
    {
        const auto alpha = static_cast<std::uint8_t>(std::lround(std::clamp(e->opacity, 0.0, 1.0) * 255));
        if (!e->texture)
        {
            auto color = e->color;
            color.a = static_cast<std::uint8_t>(std::lround(color.a * std::clamp(e->opacity, 0.0, 1.0)));
            out.push_back(elysia::core::make_ui_fill_rect_command(viewport, color));
        }
        else
        {
            auto rect = viewport;
            if (e->fit != ScreenEffectFit::Stretch)
            {
                const float sx = viewport.width() / e->width, sy = viewport.height() / e->height;
                const float scale = e->fit == ScreenEffectFit::Cover ? std::max(sx, sy) : std::min(sx, sy);
                rect.set_width(e->width * scale); rect.set_height(e->height * scale);
                rect.set_x(viewport.x() + (viewport.width() - rect.width()) / 2); rect.set_y(viewport.y() + (viewport.height() - rect.height()) / 2);
            }
            out.push_back(elysia::core::make_ui_texture_command(e->texture, rect, viewport, alpha));
        }
    }
}
elysia::core::RenderResult ScreenEffectRuntime::render(SDL_Renderer* renderer, ScreenEffectLayer layer,
    const elysia::core::Rect& viewport) const
{
    return elysia::core::detail::render_boundary(renderer, [&] {
        std::vector<elysia::core::UiRenderCommand> commands;
        append_commands(layer, viewport, commands);
        for (const auto& command : commands)
        {
            if (command.texture)
            {
                ScreenTextureBlendState state(command.texture);
                elysia::core::detail::with_render_state(state, [&] {
                    state.apply();
                    elysia::core::require_render_success(elysia::core::execute_render_command(renderer, command));
                });
            }
            else elysia::core::require_render_success(elysia::core::execute_render_command(renderer, command));
        }
    });
}
}

