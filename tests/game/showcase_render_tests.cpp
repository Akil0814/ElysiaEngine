#define SDL_MAIN_HANDLED
#include "game/application/example_game_module.h"
#include "game/showcase/shared/showcase_catalog.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/scenarios/scenario_enter_payload.h"
#include "game/showcase/ui/ui_component_gallery_scene.h"
#include "engine/ui/composites/ui_option_selector.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/scene_test_access.h"
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
#include "tests/support/input_snapshot_builder.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <chrono>
#include <array>
#include <thread>
#include <filesystem>

namespace
{
using namespace elysia;
using tests::require;

ui::UiElement* find_gallery_element(ui::UiElement& element,const std::function<bool(ui::UiElement&)>& predicate)
{
    if (predicate(element)) return &element;
    if (auto* host = dynamic_cast<ui::UiChildHost*>(&element))
        for (std::size_t index = 0; index < host->child_count(); ++index)
            if (auto* found = find_gallery_element(*host->child_at(index),predicate)) return found;
    return nullptr;
}

void capture_selector_gallery(scene::Scene& gallery,SDL_Renderer* renderer,const std::string& name)
{
    SDL_SetRenderDrawColor(renderer,20,24,32,255);
    require(SDL_RenderClear(renderer),"gallery selector canvas clears");
    scene::SceneTestAccess::render(gallery,renderer);
    require(SDL_RenderPresent(renderer),"gallery selector frame presents");
    if (const auto* directory = SDL_getenv("ELYSIA_SHOWCASE_QA_DIR"))
    {
        std::filesystem::create_directories(directory);
        auto* pixels = SDL_RenderReadPixels(renderer,nullptr);
        require(pixels != nullptr,"gallery selector pixels read");
        require(IMG_SavePNG(pixels,(std::filesystem::path(directory)/(name+".png")).string().c_str()),"gallery selector screenshot saves");
        SDL_DestroySurface(pixels);
    }
}

void exercise_selector_gallery(scene::SceneRuntimeContext& context,SDL_Renderer* renderer)
{
    example::scene::UiComponentGalleryScene gallery;
    scene::SceneTestAccess::bind(gallery,context);
    const auto payload = example::scene::ShowcaseEnterPayload{{.target=example::scene_keys::ShowcaseGallery}};
    const std::array<const char*,7> theme_names{
        "Blue Glass Moon","Elysia Light","Elysia Dark","EVA-00","EVA-01","EVA-02","Quiet Slate"};
    for (const auto language : {"en","ja","ko","zh-Hans","zh-Hant"})
    for (int visit = 0; visit < 2; ++visit)
    {
        require(ELYSIA_LOCALIZATION->set_language(language).has_value(),"selector gallery language switches");
        scene::SceneTestAccess::enter(gallery,payload);
        const auto root = [&]() -> ui::UiWindow&
        {
            return dynamic_cast<ui::UiWindow&>(*scene::SceneTestAccess::ui_root(gallery,0));
        };
        const auto select_page = [&](const char* key)
        {
            auto* tabs = dynamic_cast<ui::UiTabContainer*>(find_gallery_element(root(),[](auto& element)
                { return dynamic_cast<ui::UiTabContainer*>(&element) != nullptr; }));
            require(tabs != nullptr,"gallery exposes its main tabs");
            auto* bar = dynamic_cast<ui::UiChildHost*>(tabs->child_at(0));
            require(bar != nullptr,"gallery tab buttons exist");
            bool selected = false;
            for (std::size_t index = 0; index < bar->child_count(); ++index)
                if (auto* button = dynamic_cast<ui::UiButton*>(bar->child_at(index)); button && button->text_content().value == key)
                    selected = tabs->set_selected_index(index);
            require(selected,"gallery page is selected by its content key");
            scene::SceneTestAccess::update(gallery,0);
        };
        const auto click = [&](ui::UiElement& element,bool next_arrow = false)
        {
            scene::SceneTestAccess::update(gallery,0);
            for (auto* parent = element.layout_parent(); parent; parent = parent->layout_parent())
                if (auto* scroll = dynamic_cast<ui::UiScrollContainer*>(parent))
                {
                    scroll->ensure_visible(element.screen_rect());
                    break;
                }
            scene::SceneTestAccess::update(gallery,0);
            const auto rect = element.presentation_screen_rect();
            const int x = static_cast<int>(next_arrow ? rect.right() - 24 : rect.center().x);
            const int y = static_cast<int>(rect.center().y);
            for (const auto type : {ui::UiInputEventType::PointerPressed,ui::UiInputEventType::PointerReleased})
                (void)root().on_ui_input_event({.type=type,.device=input::InputDevice::Mouse,
                    .control=input::RawInputControl::MouseLeft,.mouse_x=x,.mouse_y=y});
        };
        const auto key = [&](ui::UiAction action)
        {
            return root().on_ui_input_event({.action=action,.type=ui::UiInputEventType::ActionPressed,
                .device=input::InputDevice::Keyboard});
        };
        select_page("ui_component_gallery.pages.controls");
        auto* horizontal = dynamic_cast<ui::UiOptionSelector*>(find_gallery_element(root(),[](auto& element)
        {
            auto* selector = dynamic_cast<ui::UiOptionSelector*>(&element);
            return selector && selector->is_enabled() && selector->options().size() == 4
                && selector->orientation() == ui::UiOptionSelectorOrientation::Horizontal;
        }));
        auto* vertical = dynamic_cast<ui::UiOptionSelector*>(find_gallery_element(root(),[](auto& element)
        {
            auto* selector = dynamic_cast<ui::UiOptionSelector*>(&element);
            return selector && selector->orientation() == ui::UiOptionSelectorOrientation::Vertical;
        }));
        require(horizontal && vertical && horizontal->selected_index() == 0 && vertical->selected_index() == 0,
            "each gallery entry must rebuild both selectors with their initial selections");
        click(*horizontal,true);
        if (horizontal->selected_index() != 2)
        {
            const auto r = horizontal->screen_rect();
            const auto p = horizontal->presentation_screen_rect();
            std::cerr << "Selector click: language=" << language << " visit=" << visit << " layout=" << r.x() << ',' << r.y() << ',' << r.width() << ',' << r.height()
                << " presentation=" << p.x() << ',' << p.y() << " focused=" << horizontal->is_focused()
                << " selected=" << horizontal->selected_index().value_or(999) << '\n';
            scene::SceneTestAccess::render(gallery,renderer);
            if (const auto* directory = SDL_getenv("ELYSIA_SHOWCASE_QA_DIR"))
            {
                auto* pixels = SDL_RenderReadPixels(renderer,nullptr);
                if (pixels) { (void)IMG_SavePNG(pixels,(std::filesystem::path(directory)/"selector-click-failure.png").string().c_str()); SDL_DestroySurface(pixels); }
            }
        }
        require(horizontal->selected_index() == 2 && !horizontal->is_adjusting(),"gallery arrow clicks must skip the disabled option");
        click(*horizontal);
        require(horizontal->is_adjusting() && key(ui::UiAction::NavigateRight) && horizontal->selected_index() == 3,
            "gallery center clicks must enter keyboard adjustment");
        if (visit == 0) capture_selector_gallery(gallery,renderer,std::string("selectors_adjusting_horizontal_")+language);
        require(key(ui::UiAction::Cancel) && !horizontal->is_adjusting(),"gallery Cancel must exit adjustment without leaving the scene");
        click(*vertical);
        require(vertical->is_adjusting() && key(ui::UiAction::NavigateDown) && vertical->selected_index() == 2,
            "gallery vertical selectors must use down for the next enabled option");
        if (visit == 0) capture_selector_gallery(gallery,renderer,std::string("selectors_adjusting_vertical_")+language);
        require(vertical->set_selected_index(3) && key(ui::UiAction::NavigateDown) && vertical->selected_index() == 0,
            "gallery vertical wrapping must cycle from last to first");
        require(key(ui::UiAction::Cancel) && !vertical->is_adjusting(),"vertical adjustment must cancel locally");
        if (visit == 0)
            for (std::size_t index = 0; index < theme_names.size(); ++index)
            {
                select_page("ui_component_gallery.pages.appearance");
                auto* button = find_gallery_element(root(),[&](auto& element)
                {
                    auto* value = dynamic_cast<ui::UiButton*>(&element);
                    return value && value->text_content().value == theme_names[index];
                });
                require(button != nullptr,"gallery exposes every theme action");
                click(*button);
                select_page("ui_component_gallery.pages.controls");
                require(horizontal->selected_index() == 3 && vertical->selected_index() == 0,
                    "theme and page changes must preserve selector values");
                capture_selector_gallery(gallery,renderer,std::string("selectors_")+language+"_"+std::to_string(index));
            }
        scene::SceneTestAccess::exit(gallery);
        scene::SceneTestAccess::reset(gallery);
    }
}
}

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
    require(builtin->initialize(renderer,elysia::builtin::BuiltinAssetCatalog(ELYSIA_SOURCE_DIR),settings->engine_point_sizes()).has_value(),"built-in assets initialize");
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
        exercise_selector_gallery(context,renderer);
        require(ELYSIA_LOCALIZATION->set_language("en").has_value(),"baseline showcase language restores");
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
                if(entry.key==example::scene_keys::CameraShowcase) {
                    elysia::tests::InputSnapshotBuilder keys;
                    for(const auto key:{elysia::input::RawInputControl::KeyF2,elysia::input::RawInputControl::KeyF3}) {
                        keys.press(key,true);manager.on_input(keys.take());keys.press(key,false);manager.on_input(keys.take());manager.on_update(0);
                        capture(std::string(key==elysia::input::RawInputControl::KeyF2?"camera_motion_":"camera_cinematic_")+language);
                        if(key==elysia::input::RawInputControl::KeyF3) {
                            const auto click=[&](int x,int y){
                                for(const auto type:{elysia::input::RawInputEventType::ControlPressed,elysia::input::RawInputEventType::ControlReleased})
                                    manager.on_input(elysia::tests::events_snapshot({{.control=elysia::input::RawInputControl::MouseLeft,.type=type,.device=elysia::input::InputDevice::Mouse,.mouse_x=x,.mouse_y=y}}));
                            };
                            click(880,154);manager.on_update(.3);capture("camera_blend_"+language);
                            click(1116,154);manager.on_update(.5);capture("camera_paused_"+language);
                            click(145,202);manager.on_update(.4);manager.on_update(.5);capture("camera_tour_"+language);
                        }

                    }
                }

            }
        }
        require(manager.shutdown(),"showcase shutdown succeeds");
    }
    audio->shutdown();effects->set_runtime_dependencies(nullptr,nullptr);localization->shutdown();fonts.shutdown();elysia::loading::clear_loaded_content();builtin->shutdown();SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);elysia::tests::close_test_mixer();TTF_Quit();SDL_Quit();
}
