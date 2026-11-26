#include "effect_service.h"

#include "runtime/effect_manager.h"

namespace elysia::effects
{
std::expected<ParticleEffectHandle, EffectRequestFailure> EffectService::request_particle_effect(const ParticleEffectSpawnRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
std::expected<void, EffectRequestFailure> EffectService::set_particle_effect_position(ParticleEffectHandle handle, elysia::core::Vector2 position)
{
    return EffectManager::instance()->_particles.set_position(handle, position);
}
bool EffectService::stop_particle_effect(ParticleEffectHandle handle) noexcept { return EffectManager::instance()->_particles.stop(handle); }
bool EffectService::cancel_particle_effect(ParticleEffectHandle handle) noexcept { return EffectManager::instance()->_particles.cancel(handle); }
bool EffectService::is_particle_effect_active(ParticleEffectHandle handle) const noexcept { return EffectManager::instance()->_particles.active(handle); }

std::expected<ScreenEffectHandle, EffectRequestFailure> EffectService::request_screen_color_effect(const ScreenColorEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
std::expected<ScreenEffectHandle, EffectRequestFailure> EffectService::request_screen_image_effect(const ScreenImageEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
bool EffectService::stop_screen_effect(ScreenEffectHandle handle) noexcept
{
    return EffectManager::instance()->_screen_effects.stop(handle);
}
std::expected<ScreenEffectHandle, EffectRequestFailure> EffectService::request_screen_animation_effect(const ScreenAnimationEffectRequest& request)
{
    return EffectManager::instance()->dispatch(request);
}
bool EffectService::cancel_screen_effect(ScreenEffectHandle handle) noexcept
{
    return EffectManager::instance()->_screen_effects.cancel(handle);
}
bool EffectService::is_screen_effect_active(ScreenEffectHandle handle) const noexcept
{
    return EffectManager::instance()->_screen_effects.active(handle);
}
std::expected<void, EffectRequestFailure> EffectService::request_animation_effect(
	const AnimationEffectSpawnRequest& request)
{
	return EffectManager::instance()->dispatch(request);
}

std::expected<void, EffectRequestFailure> EffectService::request_floating_number_effect(
	const FloatingNumberEffectSpawnRequest& request)
{
	return EffectManager::instance()->dispatch(request);
}
}
