#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/core/time.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/effects/effect_service.h"
#include "engine/effects/number/floating_number_effect_factory.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"
#include "effect_request_assertions.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using elysia::tests::require;
using elysia::tests::require_effect_failure;

class TestScene final : public elysia::scene::Scene
{
public:
    TestScene() { instance = this; }
    ~TestScene() override { instance = nullptr; }
	inline static TestScene* instance = nullptr;

    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class SecondTestScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

struct FloatingNumberEffectFixture
{
    FloatingNumberEffectFixture()
    {
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
            "floating number tests must initialize SDL video and audio");
        require(TTF_Init(), "floating number tests must initialize SDL_ttf");
        require(elysia::tests::open_test_mixer(),
            "floating number tests must open SDL_mixer audio");
        surface = SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBA32);
        require(surface != nullptr, "floating number tests must create a surface");
        renderer = SDL_CreateSoftwareRenderer(surface);
        require(renderer != nullptr, "floating number tests must create a software renderer");
    }

    ~FloatingNumberEffectFixture()
    {
        elysia::effects::EffectManager::instance()->clear_content();
        elysia::localization::LocalizationManager::instance()->shutdown();
        elysia::resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        elysia::tests::close_test_mixer();
        TTF_Quit();

        SDL_Quit();
    }

    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
};
}

void test_floating_number_validation_motion_timing_and_scene_lifecycle(FloatingNumberEffectFixture& fixture)
{
    using namespace elysia;

    SDL_Renderer* renderer = fixture.renderer;

    effects::EffectManager* effect_manager = effects::EffectManager::instance();
    effects::EffectService* effect_service = ELYSIA_EFFECTS;
    effects::FloatingNumberEffectFactory effect_factory;
    localization::LocalizationManager* localization_manager = localization::LocalizationManager::instance();
    localization::LocalizationService* localization_service = ELYSIA_LOCALIZATION;
    resources::ResourceManager* resource_manager = resources::ResourceManager::instance();
    io::PathManager* path_manager = io::PathManager::instance();

    effect_manager->clear_content();
    localization_manager->shutdown();
    resource_manager->clear();
    require(path_manager->initialize(), "floating number tests must initialize the project path manager");
    const auto resolved_font_settings =
        typography::resolve_font_settings(typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "floating number default font settings must resolve");
    auto& builtin_resources = *elysia::builtin::BuiltinResources::instance();
    require(builtin_resources.initialize(
        renderer,
        builtin::BuiltinAssetCatalog(*path_manager),
        resolved_font_settings->engine_point_sizes()).has_value(),
        "floating number tests must initialize built-in fonts");
    typography::FontResolver font_resolver;
    require(localization_manager->initialize(
        renderer,
        path_manager->configs() / "manifests" / "i18n_manifest.json",
        "en",
        &font_resolver), "floating number tests must initialize localization");
    require(font_resolver.configure(
        *resolved_font_settings,
        *resources::ResourceService::instance(),
        localization_service->supported_languages()).has_value(),
        "floating number tests must configure Engine fonts");
    effect_manager->set_runtime_dependencies(renderer,&font_resolver);

    effects::FloatingNumberEffectSpawnRequest request;
    request.text = "12%";
    request.color = effects::FloatingNumberColor::Yellow;
    request.position = core::Vector2(50.0f, 50.0f);
    request.target_height = 20.0f;
    request.text.clear();
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::InvalidRequest);
    request.text = "12A";
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::InvalidRequest);
    request.text = "12+";
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::InvalidRequest);
    request.text = "12%";
    request.lifetime_seconds = 0.0;
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::InvalidRequest);
    request.lifetime_seconds = 0.6;
    request.effects.scale = effects::FloatingNumberScale{
        .from_scale = 1.0f,
        .to_scale = 2.0f,
        .time_range = { .start_progress = 0.8f, .end_progress = 0.2f }
    };
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::InvalidRequest);

    request.effects.motion = effects::FloatingNumberLinearMotion{
        .offset = core::Vector2(10.0f, -20.0f)
    };
    request.effects.scale = effects::FloatingNumberScale{
        .from_scale = 0.5f,
        .to_scale = 1.5f
    };
    request.effects.fade = effects::FloatingNumberFade{
        .from_alpha = 255,
        .to_alpha = 55
    };
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::RuntimeUnavailable);
    effect_factory.set_runtime_dependencies(renderer,nullptr);
    require_effect_failure(effect_factory.create(request), effects::EffectRequestError::RuntimeUnavailable);
    typography::FontResolver unconfigured_font_resolver;
    effect_factory.set_runtime_dependencies(renderer, &unconfigured_font_resolver);
    const auto missing_font = effect_factory.create(request);
    require_effect_failure(missing_font, effects::EffectRequestError::RuntimeUnavailable);
    require(missing_font.error().diagnostic.message.find("not configured") != std::string::npos,
        "factory preserves font resolver failure details");
    effect_factory.set_runtime_dependencies(renderer,&font_resolver);
    request.color = effects::FloatingNumberColor::Count;
    const auto invalid_color = effect_factory.create(request);
    require_effect_failure(invalid_color, effects::EffectRequestError::InvalidRequest);
    require(std::string(invalid_color.error().diagnostic.origin.file_name()).find("floating_number_glyph_cache.cpp") != std::string::npos,
        "factory preserves glyph-cache diagnostic origin");
    request.color = effects::FloatingNumberColor::Yellow;
    const auto renderer_properties = SDL_GetRendererProperties(renderer);
    const auto max_texture_size = SDL_GetNumberProperty(renderer_properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    require(SDL_SetNumberProperty(renderer_properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 1),
        "force factory glyph creation failure");
    const auto failed_creation = effect_factory.create(request);
    require(SDL_SetNumberProperty(renderer_properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, max_texture_size),
        "restore factory renderer texture dimensions");
    require_effect_failure(failed_creation, effects::EffectRequestError::CreationFailed);
    require(failed_creation.error().diagnostic.message.find("limited to 1x1") != std::string::npos,
        "factory propagates the originating SDL glyph creation error");
    auto effect_result = effect_factory.create(request);
    require(effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> effect =
        std::move(*effect_result);
    require(effect != nullptr, "valid floating number request must create an effect");
    std::vector<core::RenderCommand> commands;
    effect->submit_render_commands(commands);
    require(commands.size() == 3 && commands.front().alpha == 255,
        "floating numbers must render each supported glyph at their initial alpha");
    require(std::fabs((commands.front().command_rect.left() + commands.back().command_rect.right()) * 0.5f
            - request.position.x) < 0.001f,
        "center-aligned floating numbers must be centered on their world position");

    effects::FloatingNumberEffectSpawnRequest left_request = request;
    left_request.alignment = effects::FloatingNumberAlignment::Left;
    left_request.start_delay_seconds = -1;
    auto left_effect_result = effect_factory.create(left_request);
    require(left_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> left_effect =
        std::move(*left_effect_result);
    require(left_effect != nullptr,"left-aligned floating number request must be created");
    commands.clear();
    left_effect->update(0);
    require(left_effect->is_started(), "negative start delays still clamp to zero");
    left_effect->submit_render_commands(commands);
    require(!commands.empty() && std::fabs(commands.front().command_rect.left() - request.position.x) < 0.001f,
        "left-aligned floating numbers must begin at their world position");

    effects::FloatingNumberEffectSpawnRequest right_request = request;
    right_request.alignment = effects::FloatingNumberAlignment::Right;
    auto right_effect_result = effect_factory.create(right_request);
    require(right_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> right_effect =
        std::move(*right_effect_result);
    require(right_effect != nullptr,"right-aligned floating number request must be created");
    commands.clear();
    right_effect->submit_render_commands(commands);
    require(!commands.empty() && std::fabs(commands.back().command_rect.right() - request.position.x) < 0.001f,
        "right-aligned floating numbers must end at their world position");

    commands.clear();
    effect->update(0.3);
    require(effect->position().nearly_equals(core::Vector2(55.0f, 40.0f)),
        "linear motion must update the number position over its shared lifetime");
    commands.clear();
    effect->submit_render_commands(commands);
    require(!commands.empty() && commands.front().alpha == 155,
        "fade must combine with motion and update glyph alpha");

    effects::FloatingNumberEffectSpawnRequest arc_request;
    arc_request.text = "8";
    arc_request.position = core::Vector2(50.0f, 50.0f);
    arc_request.effects.motion = effects::FloatingNumberArcMotion{
        .offset = core::Vector2(20.0f, -20.0f),
        .arc_height = 10.0f
    };
    auto arc_effect_result = effect_factory.create(arc_request);
    require(arc_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> arc_effect =
        std::move(*arc_effect_result);
    require(arc_effect != nullptr, "arc motion request must create an effect");
    arc_effect->update(0.3);
    require(arc_effect->position().nearly_equals(core::Vector2(60.0f, 30.0f)),
        "positive arc height must bend the movement upward");

    int finished = 0;
    effects::FloatingNumberEffectSpawnRequest delayed_request;
    delayed_request.text = "5";
    delayed_request.start_delay_seconds = 0.1;
    delayed_request.lifetime_seconds = 0.2;
    delayed_request.on_finished = [&finished](effects::FloatingNumberEffect&) { ++finished; };
    auto delayed_effect_result = effect_factory.create(delayed_request);
    require(delayed_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> delayed_effect =
        std::move(*delayed_effect_result);
    require(delayed_effect != nullptr, "delayed floating number request must create an effect");
    commands.clear();
    delayed_effect->submit_render_commands(commands);
    require(commands.empty(), "floating numbers must be invisible during their start delay");
    delayed_effect->update(0.1);
    require(delayed_effect->is_started() && !delayed_effect->is_destroyed(),
        "floating number playback must begin after its delay");
    delayed_effect->update(0.2);
    require(delayed_effect->is_destroyed() && finished == 1,
        "natural completion must invoke the finished callback once and destroy the number");
    delayed_effect->update(1.0);
    require(finished == 1, "finished callback must not run more than once");

    effects::FloatingNumberEffectSpawnRequest scaled_request;
    scaled_request.text = "7";
    scaled_request.lifetime_seconds = 0.2;
    scaled_request.time_scale = 0.5;
    auto scaled_effect_result = effect_factory.create(scaled_request);
    require(scaled_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> scaled_effect =
        std::move(*scaled_effect_result);
    require(scaled_effect != nullptr, "scaled floating number request must create an effect");
    scaled_effect->update(0.2);
    require(!scaled_effect->is_destroyed(), "local time scale must slow floating number lifetime");
    scaled_effect->update(0.2);
    require(scaled_effect->is_destroyed(), "local time scale must still allow floating number completion");

    effects::FloatingNumberEffectSpawnRequest global_scaled_request;
    global_scaled_request.text = "6";
    global_scaled_request.lifetime_seconds = 0.2;
    auto global_scaled_effect_result = effect_factory.create(global_scaled_request);
    require(global_scaled_effect_result, "factory request must succeed");
    std::unique_ptr<effects::FloatingNumberEffect> global_scaled_effect =
        std::move(*global_scaled_effect_result);
    require(global_scaled_effect != nullptr, "global scaled floating number request must create an effect");
    core::Time::instance()->reset();
    core::Time::instance()->set_time_scale(0.5);
    core::Time::instance()->begin_frame(0.2);
    global_scaled_effect->update(core::Time::instance()->delta());
    require(!global_scaled_effect->is_destroyed(), "global time scale must slow floating number lifetime");
    core::Time::instance()->begin_frame(0.2);
    global_scaled_effect->update(core::Time::instance()->delta());
    require(global_scaled_effect->is_destroyed(), "global time scale must flow through the floating number update delta");
    core::Time::instance()->reset();

    require(!effect_service->request_floating_number_effect(request),
        "floating number spawning must fail without an active scene");
    scene::SceneManager scene_manager;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(fixture.renderer, registry, 1280, 720);
    scene_manager.initialize(context);
    constexpr scene::SceneKey first_scene_key = 111;
    constexpr scene::SceneKey second_scene_key = 112;
    scene_manager.register_game_scene<TestScene>(first_scene_key);
    scene_manager.register_game_scene<SecondTestScene>(second_scene_key);
    scene_manager.start(scene::SceneRoute{
        .target = first_scene_key,
        .reload_mode = scene::SceneReloadMode::Reuse
    });

    require_effect_failure(effect_service->request_floating_number_effect({.text = "12A"}),
        effects::EffectRequestError::InvalidRequest);
    require(SDL_SetNumberProperty(renderer_properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 1),
        "force service glyph creation failure");
    const auto failed_spawn = effect_service->request_floating_number_effect({.text = "9"});
    require(SDL_SetNumberProperty(renderer_properties, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, max_texture_size),
        "restore service renderer texture dimensions");
    require_effect_failure(failed_spawn, effects::EffectRequestError::CreationFailed);
    require(failed_spawn.error().diagnostic.message.find("limited to 1x1") != std::string::npos,
        "service preserves SDL details through the factory and manager");
    std::size_t object_count = 0;
    static_cast<const object_query::IGameObjectQueryRuntime&>(*TestScene::instance).visit_game_objects(
        core::DepthLayerMask::all(), [&](core::GameObject&) { ++object_count; return true; });
    require(object_count == 0, "failed glyph creation leaves no scene object");

    int scene_finished = 0;
    effects::FloatingNumberEffectSpawnRequest scene_request;
    scene_request.text = "9";
    scene_request.lifetime_seconds = 0.2;
    scene_request.on_finished = [&scene_finished](effects::FloatingNumberEffect&) { ++scene_finished; };
    require(effect_service->request_floating_number_effect(scene_request),
        "effect service must attach floating numbers to the active scene");
    scene_manager.on_update(0.0);
    require(TestScene::instance != nullptr, "first test scene must be active");
    TestScene::instance->pause();
    scene_manager.on_update(1.0);
    require(scene_finished == 0, "scene pause must suspend floating number lifetimes");
    TestScene::instance->resume();
    scene_manager.on_update(0.2);
    require(scene_finished == 1, "resumed scenes must complete attached floating numbers");

    int switch_finished = 0;
    effects::FloatingNumberEffectSpawnRequest switch_pending_request;
    switch_pending_request.text = "4";
    switch_pending_request.lifetime_seconds = 2.0;
    switch_pending_request.on_finished = [&switch_finished](effects::FloatingNumberEffect&) { ++switch_finished; };
    require(effect_service->request_floating_number_effect(switch_pending_request),
        "a pending floating number must attach before a scene switch");
    scene::SceneRequest switch_request;
    switch_request.type = scene::SceneRequestType::Switch;
    switch_request.route.target = second_scene_key;
    scene_manager.on_scene_request(switch_request);
    scene_manager.on_update(0.0);
    require(switch_finished == 0, "scene switching must not invoke pending floating number callbacks");

    int shutdown_finished = 0;
    effects::FloatingNumberEffectSpawnRequest shutdown_request;
    shutdown_request.text = "3";
    shutdown_request.lifetime_seconds = 2.0;
    shutdown_request.on_finished = [&shutdown_finished](effects::FloatingNumberEffect&) { ++shutdown_finished; };
    require(effect_service->request_floating_number_effect(scene_request),
        "floating number effects must rebind to the switched scene");
    require(effect_service->request_floating_number_effect(shutdown_request),
        "pending floating number effects must attach to the switched scene");
    scene_manager.shutdown();
    require(shutdown_finished == 0, "scene shutdown must not invoke pending floating number callbacks");
    require(!effect_service->request_floating_number_effect(scene_request),
        "floating number spawning must fail after active scene shutdown");

    effect_manager->set_runtime_dependencies(nullptr,nullptr);
    localization_manager->shutdown();
    font_resolver.shutdown();
    builtin_resources.shutdown();
}

int main()
{
    FloatingNumberEffectFixture fixture;
    test_floating_number_validation_motion_timing_and_scene_lifecycle(fixture);
    return EXIT_SUCCESS;
}
