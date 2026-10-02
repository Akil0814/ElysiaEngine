#define SDL_MAIN_HANDLED
#include "game/application/example_game_module.h"
#include "game/showcase/shared/showcase_catalog.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/scenarios/scenario_enter_payload.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "engine/loading/content_runtime_cleanup.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/audio/audio_service.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <chrono>
#include <array>
#include <thread>
#include <filesystem>
int main()
{
    using elysia::tests::require;
    SDL_setenv_unsafe("SDL_VIDEO_DRIVER","dummy",1);SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO),"SDL initializes");require(TTF_Init(),"fonts initialize");require(elysia::tests::open_test_mixer(),"dummy mixer opens");
    auto* surface=SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32);auto* renderer=SDL_CreateSoftwareRenderer(surface);require(renderer!=nullptr,"software renderer creates");
    auto* paths=elysia::io::PathManager::instance();require(paths->initialize(ELYSIA_SOURCE_DIR),"project paths initialize");
    auto registry=elysia::io::ContentRegistryLoader{}.load(paths->content_registry());require(registry.has_value(),"project registry loads");
    auto settings=elysia::typography::resolve_font_settings({});require(settings.has_value(),"font settings resolve");
    auto* builtin=elysia::builtin::BuiltinResources::instance();
    require(builtin->initialize(renderer,elysia::builtin::BuiltinAssetCatalog(ELYSIA_SOURCE_DIR),settings->engine_point_sizes(),{}).has_value(),"built-in assets initialize");
    {
        elysia::loading::GameContentLoader loader;const std::array sizes{10,20,30,40,50,60,70};
        require(loader.start(renderer,*registry,sizes).has_value(),"project assets start loading");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(!loader.is_finished() && !loader.has_failed() && std::chrono::steady_clock::now()<deadline){loader.update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(loader.is_finished() && !loader.has_failed(),"all project assets load");
    }
    elysia::typography::FontResolver fonts;const std::array<std::string,5> languages{"en","ja","ko","zh-Hans","zh-Hant"};
    require(fonts.configure(*settings,*ELYSIA_RESOURCES,languages).has_value(),"display fonts configure");
    auto* localization=elysia::localization::LocalizationManager::instance();require(localization->initialize(renderer,std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/configs/manifests/i18n_manifest.json","en",&fonts).has_value(),"localization initializes");
    auto* effects=elysia::effects::EffectManager::instance();effects->set_runtime_dependencies(renderer,&fonts);
    auto* audio=elysia::audio::AudioService::instance();require(audio->initialize({}),"audio initializes");
    {
        elysia::scene::SceneRuntimeContext context(renderer,*registry,1280,720,&fonts);
        elysia::scene::SceneManager manager;manager.initialize(context);example::application::GameModule{}.register_scenes(manager);
        const elysia::scene::SceneRoute gallery{.target=example::scene_keys::ShowcaseGallery,.payload=example::scene::ShowcaseEnterPayload{{.target=example::scene_keys::MainMenu}}};
        manager.start(gallery);
        const auto capture=[&](const std::string& name){
            require(manager.state()==elysia::scene::SceneManagerState::Running,"showcase entered without fault");
            manager.on_update(0);SDL_SetRenderDrawColor(renderer,20,24,32,255);SDL_RenderClear(renderer);manager.on_render(renderer);SDL_RenderPresent(renderer);
            if(const char* directory=SDL_getenv("ELYSIA_SHOWCASE_QA_DIR")){std::filesystem::create_directories(directory);auto* pixels=SDL_RenderReadPixels(renderer,nullptr);require(pixels!=nullptr,"capture pixels available");require(IMG_SavePNG(pixels,(std::filesystem::path(directory)/(name+".png")).string().c_str()),"capture saves");SDL_DestroySurface(pixels);}
        };
        capture("gallery");
        for(int visit=0;visit<2;++visit)for(const auto& entry:example::showcase::kShowcaseEntries){
            manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route={.target=entry.key,.payload=example::scene::ShowcaseEnterPayload{gallery},.reload_mode=entry.reload}});manager.on_update(0);
            require(manager.current_scene_key()==entry.key,"each registered module is reachable");capture("module_"+std::to_string(entry.key));
            manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route=gallery});manager.on_update(0);
        }
        for(const auto key:{example::scene_keys::ColliderCombatDemo,example::scene_keys::PlatformTileCombatDemo,example::scene_keys::TopDownTileCombatDemo}){
            manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route={.target=key,.payload=example::scene::ShowcaseEnterPayload{gallery},.reload_mode=elysia::scene::SceneReloadMode::Recreate}});manager.on_update(0);capture("gameplay_"+std::to_string(key));
        }
        for(const auto& pair:{std::pair{example::scene_keys::Box2DLab,"motion"},std::pair{example::scene_keys::GameplayVerification,"combat_collider"}}){
            manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route={.target=pair.first,.payload=example::scene::ScenarioEnterPayload{gallery,pair.second,0},.reload_mode=elysia::scene::SceneReloadMode::Recreate}});manager.on_update(0);capture("verification_"+std::to_string(pair.first));
            for(const auto reload:{elysia::scene::SceneReloadMode::Reuse,elysia::scene::SceneReloadMode::Reset}){
                manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route=gallery});manager.on_update(0);
                manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route={.target=pair.first,.payload=example::scene::ScenarioEnterPayload{gallery,pair.second,0},.reload_mode=reload}});manager.on_update(0);
                require(manager.current_scene_key()==pair.first,"verification accepts repeat and reset routes");capture("verification_"+std::to_string(pair.first));
            }
        }
        for(const auto& language:languages){
            require(elysia::localization::LocalizationService::instance()->set_language(language).has_value(),"display locale switches");
            manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route=gallery});manager.on_update(0);capture("gallery_"+language);
            for(const auto& entry:example::showcase::kShowcaseEntries){
                manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route={.target=entry.key,.payload=example::scene::ShowcaseEnterPayload{gallery},.reload_mode=entry.reload}});manager.on_update(0);capture("module_"+std::to_string(entry.key)+"_"+language);
            }
        }
        require(manager.shutdown(),"showcase shutdown succeeds");
    }
    audio->shutdown();effects->set_runtime_dependencies(nullptr,nullptr);localization->shutdown();fonts.shutdown();elysia::loading::clear_loaded_content();builtin->shutdown();SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);elysia::tests::close_test_mixer();TTF_Quit();SDL_Quit();
}
