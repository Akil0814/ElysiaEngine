#define SDL_MAIN_HANDLED

#include "engine/builtin/scenes/startup_loading_scene.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/bootstrap/bootstrapper.h"
#include "engine/config/user_config_service.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/ui/widgets/image/ui_fade_image.h"
#include "engine/ui/widgets/label/ui_blink_label.h"
#include "engine/ui/widgets/ui_bar.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace elysia::builtin
{
class StartupLoadingSceneTestAccess
{
public:
    static void present(StartupLoadingScene& scene,
        const StartupLoadingScenePayload& payload)
    {
        scene._startup_payload = payload;
        scene._completion.reset(payload.wait_for_confirmation,
            payload.wait_for_logo_sequence);
        elysia::tests::require(scene.create_presentation(),
            "startup presentation must accept optional backgrounds");
        scene.begin_logo_sequence();
    }

    static elysia::ui::UiImage* background(StartupLoadingScene& scene)
    {
        return scene._background;
    }

    static SDL_Texture* engine_logo_texture(StartupLoadingScene& scene)
    {
        return scene._engine_logo->texture();
    }

    static void finish_engine_logo(StartupLoadingScene& scene)
    {
        for (int phase = 0; phase < 3; ++phase)
            scene._engine_logo->update(1.1);
    }

    static void finish_project_logo(StartupLoadingScene& scene)
    {
        for (int phase = 0; phase < 3; ++phase)
            scene._project_logo->update(1.1);
    }

    static void finish_loading(StartupLoadingScene& scene)
    {
        scene.handle_completion_action(scene._completion.mark_loading_finished());
    }

    static bool overlays_above_background(StartupLoadingScene& scene)
    {
        return scene._loading_bar->order() > scene._background->order()
            && scene._start_prompt->order() > scene._background->order();
    }

    static bool prompt_visible(StartupLoadingScene& scene)
    {
        return scene._start_prompt->is_visible();
    }
};
}

namespace
{
using elysia::tests::require;
using elysia::builtin::StartupLoadingScene;
using elysia::builtin::StartupLoadingScenePayload;
using Access = elysia::builtin::StartupLoadingSceneTestAccess;

class PresentationFixture
{
public:
    PresentationFixture()
    {
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) && TTF_Init()
            && elysia::tests::open_test_mixer(),
            "startup background tests must initialize SDL services");
        surface = SDL_CreateSurface(320,180,SDL_PIXELFORMAT_RGBA32);
        require(surface != nullptr,"startup background tests need a surface");
        renderer = SDL_CreateSoftwareRenderer(surface);
        require(renderer != nullptr,"startup background tests need a renderer");

        const std::filesystem::path source = ELYSIA_SOURCE_DIR;
        root = std::filesystem::temp_directory_path()
            / ("elysia_startup_background_" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        for (const char* directory : {"audio","textures","fonts","preload"})
            std::filesystem::create_directories(root / "assets" / directory);
        std::filesystem::copy_file(source / "assets/.elysia_root",root / "assets/.elysia_root");
        std::filesystem::copy_file(source / "assets/content_registry.json",root / "assets/content_registry.json");
        std::filesystem::copy(source / "assets/configs",root / "assets/configs",
            std::filesystem::copy_options::recursive);
        require(SDL_SaveBMP(surface,(root / "assets/preload/background.bmp").string().c_str()),
            "startup background tests must save a preload image");
        {
            std::ofstream manifest(root / "assets/configs/manifests/preload_manifest.json");
            manifest << R"({"textures":[{"key":"startup.background","file":"background.bmp"}]})";
        }
        auto* bootstrap = elysia::bootstrap::Bootstrapper::instance();
        const auto settings = bootstrap->parse_runtime_settings(root);
        require(settings.has_value(),settings ? "" : settings.error().diagnostic.message.c_str());
        const auto preload = bootstrap->preload_startup_resources(renderer);
        require(preload.has_value(),preload ? "" : preload.error().diagnostic.message.c_str());
        const std::array sizes{20};
        require(elysia::builtin::BuiltinResources::instance()->initialize(renderer,
            elysia::builtin::BuiltinAssetCatalog(source),sizes).has_value(),
            "startup presentation tests need the built-in engine logo");
    }

    ~PresentationFixture()
    {
        elysia::bootstrap::Bootstrapper::instance()->release_preload_textures();
        elysia::builtin::BuiltinResources::instance()->shutdown();
        elysia::config::UserConfigService::instance()->shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        elysia::tests::close_test_mixer();
        TTF_Quit();
        SDL_Quit();
        std::error_code error;
        std::filesystem::remove_all(root,error);
    }

    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::filesystem::path root;
};

void test_background_waits_for_entire_logo_sequence(PresentationFixture& fixture)
{
    for (const bool project_logo : {false,true})
    {
        const elysia::io::ContentRegistry registry{};
        const elysia::scene::SceneRuntimeContext context{
            fixture.renderer,registry,1280,720};
        StartupLoadingScene scene;
        elysia::scene::SceneTestAccess::bind(scene,context);
        StartupLoadingScenePayload payload;
        payload.background = elysia::builtin::StartupBackgroundSlot{"startup.background"};
        if (project_logo)
            payload.project_logo = elysia::builtin::StartupLogoSlot{"startup.background"};
        Access::present(scene,payload);
        auto* background = Access::background(scene);
        require(background && !background->is_visible(),
            "background must stay hidden while the engine logo plays");
        require(background->texture()
                == elysia::bootstrap::Bootstrapper::instance()->find_preload_texture("startup.background")
            && background->screen_rect().nearly_equals({0,0,1280,720})
            && Access::overlays_above_background(scene),
            "background must fill the logical viewport below loading and confirmation UI");
        Access::finish_engine_logo(scene);
        require(background->is_visible() == !project_logo,
            "background must wait for the project logo when configured");
        if (project_logo)
            Access::finish_project_logo(scene);
        std::vector<elysia::core::UiRenderCommand> commands;
        background->submit_ui_render_commands(commands);
        require(background->is_visible() && commands.size() == 1
            && commands.front().texture == background->texture(),
            "finished logo sequence must display the background even while loading continues");
        Access::finish_loading(scene);
        require(background->is_visible() && Access::prompt_visible(scene),
            "background must remain visible during confirmation");
        elysia::scene::SceneTestAccess::reset(scene);
        require(Access::background(scene) == nullptr,
            "reset must retire the background widget");
        Access::present(scene,{});
        require(Access::background(scene) == nullptr,
            "reused presentation must not retain an old background");
        elysia::scene::SceneTestAccess::reset(scene);
    }
}

void test_optional_backgrounds_and_early_confirmation(PresentationFixture& fixture)
{
    const elysia::io::ContentRegistry registry{};
    const elysia::scene::SceneRuntimeContext context{fixture.renderer,registry,640,480};
    for (const char* key : {"","missing.background","startup.background"})
    {
        StartupLoadingScene scene;
        elysia::scene::SceneTestAccess::bind(scene,context);
        StartupLoadingScenePayload payload;
        payload.wait_for_logo_sequence = false;
        payload.background = elysia::builtin::StartupBackgroundSlot{key};
        Access::present(scene,payload);
        Access::finish_loading(scene);
        require(Access::prompt_visible(scene),
            "background configuration must preserve early confirmation policy");
        auto* background = Access::background(scene);
        require(key == std::string_view("startup.background")
                ? background && !background->is_visible()
                : background == nullptr,
            "unavailable backgrounds must be skipped and valid backgrounds must still wait for logos");
        Access::finish_engine_logo(scene);
        require(!background || background->is_visible(),
            "early confirmation must still reveal the background once logos finish");
        elysia::scene::SceneTestAccess::reset(scene);
    }
}

void test_engine_logo_variants_select_actual_textures(PresentationFixture& fixture)
{
    using elysia::application::ApplicationEngineLogoVariant;
    using elysia::builtin::BuiltinTextureId;
    const std::array variants{
        std::pair{ApplicationEngineLogoVariant::Default,BuiltinTextureId::ElysiaDefault},
        std::pair{ApplicationEngineLogoVariant::Black,BuiltinTextureId::ElysiaBlack},
        std::pair{ApplicationEngineLogoVariant::BlackAlphaInverse,BuiltinTextureId::ElysiaBlackAlphaInverse},
        std::pair{ApplicationEngineLogoVariant::LightEdge,BuiltinTextureId::ElysiaLightEdge},
        std::pair{ApplicationEngineLogoVariant::White,BuiltinTextureId::ElysiaWhite}
    };
    const elysia::io::ContentRegistry registry{};
    const elysia::scene::SceneRuntimeContext context{fixture.renderer,registry,640,480};
    for (const auto& [variant,texture_id] : variants)
    {
        StartupLoadingScene scene(variant);
        elysia::scene::SceneTestAccess::bind(scene,context);
        SDL_Texture* expected = elysia::builtin::BuiltinResources::instance()->find_texture(texture_id);
        require(expected != nullptr,"each startup logo variant must have a built-in texture");
        for (int entry = 0; entry < 2; ++entry)
        {
            Access::present(scene,{});
            require(Access::engine_logo_texture(scene) == expected,
                "startup logo must use the configured texture, including after reset");
            elysia::scene::SceneTestAccess::reset(scene);
        }
    }
    StartupLoadingScene default_scene;
    elysia::scene::SceneTestAccess::bind(default_scene,context);
    Access::present(default_scene,{});
    require(Access::engine_logo_texture(default_scene)
            == elysia::builtin::BuiltinResources::instance()->find_texture(BuiltinTextureId::ElysiaWhite),
        "default startup scene construction must preserve the white logo");
    elysia::scene::SceneTestAccess::reset(default_scene);
}
}

int main()
{
    PresentationFixture fixture;
    test_background_waits_for_entire_logo_sequence(fixture);
    test_optional_backgrounds_and_early_confirmation(fixture);
    test_engine_logo_variants_select_actual_textures(fixture);
    return EXIT_SUCCESS;
}
