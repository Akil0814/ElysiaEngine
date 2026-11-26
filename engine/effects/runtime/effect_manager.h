#pragma once

#include "../effect_request_failure.h"
#include "../animation/animation_effect_factory.h"
#include "../effect_types.h"
#include "../particle/particle_runtime.h"
#include "../screen/screen_effect_runtime.h"
#include "../number/floating_number_effect_factory.h"
#include "../../resources/resource_types.h"
#include "../../tools/singleton.h"
#include "../effect_registration_failure.h"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::scene
{
class Scene;
class SceneManager;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::effects
{
class EffectService;

class EffectManager : public elysia::tools::Singleton<EffectManager>
{
	friend elysia::tools::Singleton<EffectManager>;
	friend class EffectService;
	friend class elysia::scene::SceneManager;

public:
	void clear_screen_effects() noexcept { _screen_effects.clear(); }
    void clear_transient_effects() noexcept { _screen_effects.clear(); _particles.clear(); }
    [[nodiscard]] elysia::core::RenderResult render_particle_effects(SDL_Renderer* renderer,
        elysia::core::DepthLayer layer, const elysia::camera::Camera& camera) { return _particles.render(renderer, layer, camera); }
    [[nodiscard]] std::size_t particle_count() const noexcept { return _particles.particle_count(); }
    [[nodiscard]] std::expected<void, EffectRegistrationFailure> register_particle_effect(const ParticleEffectDefinition& definition);
	void append_screen_effect_commands(ScreenEffectLayer layer, const elysia::core::Rect& viewport,
		std::vector<elysia::core::UiRenderCommand>& out) const { _screen_effects.append_commands(layer, viewport, out); }
	[[nodiscard]] elysia::core::RenderResult render_screen_effects(SDL_Renderer* renderer, ScreenEffectLayer layer,
		const elysia::core::Rect& viewport) const { return _screen_effects.render(renderer, layer, viewport); }
	void set_runtime_dependencies(
		SDL_Renderer* renderer,
		const elysia::typography::FontResolver* font_resolver) noexcept;

	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const elysia::resources::AnimationEffectBuildRequest& request);
	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const std::vector<elysia::resources::AnimationEffectBuildRequest>& requests);

	[[nodiscard]] const AnimationEffectDefinition* find_animation_effect_definition(
		std::string_view key) const;
	void clear_content() noexcept;

private:
    std::expected<ParticleEffectHandle, EffectRequestFailure> dispatch(const ParticleEffectSpawnRequest& request);
    ParticleRuntime _particles;
    std::unordered_map<std::string, ParticleEffectDefinition> _particle_definitions;
    SDL_Renderer* _renderer = nullptr;
	std::expected<ScreenEffectHandle, EffectRequestFailure> dispatch(const ScreenColorEffectRequest& request);
	std::expected<ScreenEffectHandle, EffectRequestFailure> dispatch(const ScreenImageEffectRequest& request);
	std::expected<ScreenEffectHandle, EffectRequestFailure> dispatch(const ScreenAnimationEffectRequest& request);
	ScreenEffectRuntime _screen_effects;
	[[nodiscard]] std::expected<void, EffectRequestFailure> dispatch(const AnimationEffectSpawnRequest& request);
	[[nodiscard]] std::expected<void, EffectRequestFailure> dispatch(const FloatingNumberEffectSpawnRequest& request);

	void bind_active_scene(elysia::scene::Scene& scene) noexcept;
	void unbind_active_scene(const elysia::scene::Scene& scene) noexcept;

	std::unordered_map<std::string,AnimationEffectDefinition> _animation_effect_definitions;
	AnimationEffectFactory _animation_effect_factory;
	FloatingNumberEffectFactory _floating_number_effect_factory;
	elysia::scene::Scene* _active_scene = nullptr;
};
}
