#include "particle_runtime.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace elysia::effects
{
std::expected<ParticleEffectHandle, EffectRequestFailure> ParticleRuntime::create(
    const ParticleEffectDefinition& definition, SDL_Texture* texture,
    const ParticleEffectSpawnRequest& request, const void* scene, std::optional<std::size_t> frame)
{
    if (!scene) return std::unexpected(make_effect_request_failure(EffectRequestError::NoActiveScene,
        "Particle effect requires an active scene.", request.effect_key));
    if (!std::isfinite(request.position.x) || !std::isfinite(request.position.y))
        return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
            "Particle emission position must be finite.", request.effect_key));
    if (!texture) return std::unexpected(make_effect_request_failure(EffectRequestError::MissingTexture,
        "Particle texture does not exist: " + definition.texture_key, request.effect_key));
    auto size = ParticleEffect::validate(definition, texture);
    if (!size) return std::unexpected(EffectRequestFailure{EffectRequestError::CreationFailed, std::move(size.error().diagnostic)});
    const auto seed = request.seed ? *request.seed : (_next_seed += 0x9e3779b9);
    // Construct before changing the registry, so failed allocations leave no active slot.
    ParticleEffect effect(definition, texture, *size, request.position, seed);
    std::size_t index = 0;
    while (index < _slots.size() && (_slots[index].effect || _slots[index].generation == 0)) ++index;
    if (index == _slots.size())
    {
        if (index >= std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(make_effect_request_failure(EffectRequestError::CreationFailed, "Particle handle slots exhausted."));
        _slots.emplace_back();
    }
    _order.push_back(static_cast<std::uint32_t>(index));
    auto& slot = _slots[index];
    slot.scene = scene; slot.created_frame = frame; slot.effect.emplace(std::move(effect));
    return ParticleEffectHandle{static_cast<std::uint32_t>(index), slot.generation};
}

bool ParticleRuntime::active(ParticleEffectHandle h) const noexcept
{
    return h.generation != 0 && h.slot < _slots.size() && _slots[h.slot].generation == h.generation && _slots[h.slot].effect.has_value();
}
void ParticleRuntime::retire(Slot& slot) noexcept
{
    slot.effect.reset(); slot.scene = nullptr; slot.created_frame.reset(); ++slot.generation;
}
bool ParticleRuntime::stop(ParticleEffectHandle h) noexcept
{
    return active(h) && _slots[h.slot].effect->stop();
}
bool ParticleRuntime::cancel(ParticleEffectHandle h) noexcept
{
    if (!active(h)) return false;
    retire(_slots[h.slot]); std::erase(_order, h.slot); return true;
}
std::expected<void, EffectRequestFailure> ParticleRuntime::set_position(ParticleEffectHandle h, elysia::core::Vector2 p)
{
    if (!active(h)) return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidHandle, "Particle handle is inactive."));
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest, "Particle emission position must be finite."));
    _slots[h.slot].effect->set_position(p); return {};
}
void ParticleRuntime::update(double delta, bool paused, std::optional<std::size_t> frame)
{
    if (paused) return;
    for (const auto index : _order)
    {
        auto& slot = _slots[index];
        if (frame && slot.created_frame == frame) continue;
        slot.effect->update(delta);
        if (slot.effect->finished()) retire(slot);
    }
    std::erase_if(_order, [&](auto index) { return !_slots[index].effect; });
}
void ParticleRuntime::unbind(const void* scene) noexcept
{
    for (auto& slot : _slots) if (slot.effect && slot.scene == scene) retire(slot);
    std::erase_if(_order, [&](auto index) { return !_slots[index].effect; });
}
void ParticleRuntime::clear() noexcept
{
    for (auto& slot : _slots) if (slot.effect) retire(slot);
    _order.clear();
}
elysia::core::RenderResult ParticleRuntime::render(SDL_Renderer* renderer, elysia::core::DepthLayer layer,
    const elysia::camera::Camera& camera)
{
    for (const auto index : _order)
        if (_slots[index].effect->layer() == layer)
            if (auto result = _slots[index].effect->render(renderer, camera); !result) return result;
    return {};
}
std::size_t ParticleRuntime::particle_count() const noexcept
{
    std::size_t count = 0;
    for (auto index : _order) count += _slots[index].effect->particles().size();
    return count;
}
}
