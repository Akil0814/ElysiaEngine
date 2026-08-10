#define SDL_MAIN_HANDLED

#include "engine/animation/animation_service.h"
#include "engine/config/config_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/loading/game_content_loader.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <array>
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
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
        "game content loader config test must initialize SDL");
    require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
        "game content loader config test must initialize SDL_image");
    require(TTF_Init() == 0,
        "game content loader config test must initialize SDL_ttf");
    require(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0,
        "game content loader config test must open SDL_mixer audio");

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_RGBA32);
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
		&& resource_service->find_font("ui.latin.20") != nullptr
		&& resource_service->find_sound("system.button_click_down") != nullptr
		&& resource_service->find_sound("system.button_click_up") != nullptr
		&& resource_service->find_music("scene.main_menu") == nullptr,
		"ResourceService must expose the complete minimal runtime resource set");

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
    SDL_FreeSurface(surface);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
