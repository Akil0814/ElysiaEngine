#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/animation/animation_service.h"
#include "engine/config/config_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/effects/effect_service.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/test_assertions.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{
using elysia::tests::require;

constexpr std::array<std::string_view, 8> ryougi_animation_keys{
    "RyougiShiki.idle",
    "RyougiShiki.run_loop",
    "RyougiShiki.attack_normal.0",
    "RyougiShiki.attack_normal.1",
    "RyougiShiki.attack_normal.2",
    "RyougiShiki.attack_normal.3",
    "RyougiShiki.attack_normal.4",
    "RyougiShiki.attack_normal.5"
};

bool ryougi_sample_is_registered(
    const elysia::animation::AnimationService& animations,
    const elysia::resources::ResourceService& resources)
{
    return std::all_of(
        ryougi_animation_keys.begin(), ryougi_animation_keys.end(),
        [&animations, &resources](std::string_view key)
        {
            return animations.find_definition(key) != nullptr
                && resources.find_atlas(key) != nullptr;
        });
}

bool ryougi_sample_is_unavailable(
    const elysia::animation::AnimationService& animations,
    const elysia::resources::ResourceService& resources)
{
    return std::none_of(
        ryougi_animation_keys.begin(), ryougi_animation_keys.end(),
        [&animations, &resources](std::string_view key)
        {
            return animations.find_definition(key) != nullptr
                || resources.find_atlas(key) != nullptr;
        });
}

class ParticleScene final : public elysia::scene::Scene
{
public:
    ParticleScene() : Scene({.camera = elysia::scene::CameraSceneConfig{}}) {}
private:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

void run_to_completion(elysia::loading::GameContentLoader& loader)
{
    for (int update_count = 0; loader.is_running() && update_count < 10000; ++update_count)
    {
        loader.update();
        SDL_Delay(1);
    }
    require(loader.is_finished(), "game content loader must finish with the repository content");
}
}

int main()
{
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
        "game content loader config test must initialize SDL");
    require(TTF_Init(),
        "game content loader config test must initialize SDL_ttf");
    require(elysia::tests::open_test_mixer(),
        "game content loader config test must open SDL_mixer audio");

    SDL_Surface* surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, "game content loader config test must create a target surface");
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr, "game content loader config test must create a software renderer");

    auto* paths = elysia::io::PathManager::instance();
    require(paths->initialize(), "game content loader config test must initialize PathManager");
    auto* configs = elysia::config::ConfigService::instance();
    elysia::loading::clear_loaded_content();
	elysia::resources::ResourceManager* resources = elysia::resources::ResourceManager::instance();
	elysia::resources::ResourceService* resource_service = ELYSIA_RESOURCES;
	elysia::animation::AnimationService* animations = ELYSIA_ANIMATIONS;
	elysia::effects::EffectManager* effects = elysia::effects::EffectManager::instance();
	auto content_registry = elysia::io::ContentRegistryLoader{}.load(paths->content_registry());
	require(content_registry,
		"game content loader config test must parse the content registry once before loading");

    elysia::loading::GameContentLoader loader;
    constexpr std::array project_font_point_sizes{10,20,30,40,50,60,70};
	require(loader.start(renderer, *content_registry, project_font_point_sizes), "game content loader must start with a valid deferred config snapshot");
    require(!configs->is_initialized(),
        "ConfigService must remain unavailable while resources are still loading");
	run_to_completion(loader);
    require(configs->is_initialized(),
        "ConfigService must publish the deferred snapshot only after content loading finishes");
	require(resources->resource_count() > 0
		&& animations->find_definition("test.animation") != nullptr
		&& ryougi_sample_is_registered(*animations, *resource_service)
		&& effects->find_animation_effect_definition("effect.test") != nullptr,
		"a finished content load must register core resources, all Ryougi clips, and effects together");
	require(resource_service->find_atlas("test.animation") != nullptr
		&& resource_service->find_texture("ui.moon") != nullptr
		&& resource_service->find_texture("demo.screen_effect") != nullptr
		&& resource_service->find_font("ui.latin.20") != nullptr
		&& resource_service->find_sound("system.button_click_down") != nullptr
		&& resource_service->find_sound("system.button_click_up") != nullptr
		&& resource_service->find_music("scene.main_menu") == nullptr,
		"ResourceService must expose the complete minimal runtime resource set");

    effects->set_runtime_dependencies(renderer, nullptr);
    elysia::scene::SceneManager manager;
    elysia::scene::SceneRuntimeContext context(renderer, *content_registry, 64, 64);
    manager.initialize(context);
    manager.register_game_scene<ParticleScene>(101);
    manager.start({.target = 101});
    const elysia::effects::ParticleEffectSpawnRequest sparks{.effect_key = "showcase.particles.sparks"};
    require(effects->particle_count() == 0, "content loading only registers particle definitions");
    const auto first_burst = ELYSIA_EFFECTS->request_particle_effect(sparks);
    require(first_burst && effects->particle_count() == 150, "the startup-loaded preset spawns the configured burst");

    loader.reset();
	require(configs->is_initialized()
		&& resources->resource_count() > 0
		&& animations->find_definition("test.animation") != nullptr
		&& ryougi_sample_is_registered(*animations, *resource_service)
		&& effects->find_animation_effect_definition("effect.test") != nullptr,
		"resetting a finished loader must preserve published content for the next scene");

	require(loader.start(renderer, *content_registry, project_font_point_sizes),
		"starting a new loading cycle must clear the previous published content first");
	require(!configs->is_initialized()
		&& resources->resource_count() == 0
		&& animations->find_definition("test.animation") == nullptr
		&& ryougi_sample_is_unavailable(*animations, *resource_service)
		&& effects->find_animation_effect_definition("effect.test") == nullptr,
		"a new loading cycle must not expose the old content while it is preparing");
	run_to_completion(loader);

    require(!ELYSIA_EFFECTS->is_particle_effect_active(*first_burst), "content reload invalidates old particle instances");
    require(ELYSIA_EFFECTS->request_particle_effect(sparks), "a subsequent load restores the particle preset");

    const auto invalid_manifest = std::filesystem::temp_directory_path() / "elysia_particle_out_of_bounds_manifest.json";
    std::ofstream(invalid_manifest) << R"({
        "effects": [{"key":"effect.test","animation_key":"test.animation"}],
        "particle_effects": [
            {"key":"effect.first","texture_key":"demo.particle_dot"},
            {"key":"effect.invalid","texture_key":"demo.particle_dot","source_rect":[0,0,999,999]}
        ]
    })";
    auto invalid_registry = *content_registry;
    invalid_registry.required.effects = invalid_manifest;
    const auto invalid_start = loader.start(renderer, invalid_registry, project_font_point_sizes);
    std::filesystem::remove(invalid_manifest);
    require(invalid_start, "source rectangle bounds must be checked after texture loading");
    for (int step = 0; loader.is_running() && step < 10000; ++step)
    {
        loader.update();
        SDL_Delay(1);
    }
    require(loader.has_failed() && loader.failure()->code == elysia::loading::ContentLoadError::Effect
        && loader.failure()->diagnostic.entries.front().declaration_path.filename() == invalid_manifest.filename()
        && loader.failure()->diagnostic.entries.front().declaration_pointer == "/particle_effects/1/source_rect",
        "runtime bounds failure reports the manifest and offending source rectangle");
    const auto rolled_back = ELYSIA_EFFECTS->request_particle_effect({.effect_key = "effect.first"});
    require(!rolled_back && rolled_back.error().code == elysia::effects::EffectRequestError::MissingEffectDefinition
        && effects->particle_count() == 0 && resources->resource_count() == 0 && !configs->is_initialized(),
        "failed content loading rolls back particle definitions, instances and resources");

    require(loader.start(renderer, *content_registry, project_font_point_sizes), "restart after a particle registration failure");
    run_to_completion(loader);
    require(ELYSIA_EFFECTS->request_particle_effect(sparks), "restart registers usable presets again");
    require(manager.shutdown(), "particle test scene shuts down");
    effects->set_runtime_dependencies(nullptr, nullptr);

	require(!loader.start(nullptr, *content_registry, project_font_point_sizes),
		"an invalid renderer must fail the new content loading cycle");
	require(loader.has_failed()
		&& loader.failure()
		&& loader.failure()->error_code() == "CONTENT-PLAN"
		&& !loader.failure()->diagnostic.message.empty()
		&& !configs->is_initialized()
		&& resources->resource_count() == 0
		&& animations->find_definition("test.animation") == nullptr
		&& ryougi_sample_is_unavailable(*animations, *resource_service)
		&& effects->find_animation_effect_definition("effect.test") == nullptr,
		"failed content loading must leave every runtime registry empty");

	loader.reset();
	require(!loader.has_failed() && loader.failure() == nullptr,
		"reset must clear the published structured failure object");

	elysia::loading::clear_loaded_content();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    elysia::tests::close_test_mixer();
    TTF_Quit();

    SDL_Quit();
    return 0;
}
