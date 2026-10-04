#include "game/gameplay/control/local_controls.h"
#include "engine/gameplay/control/controller_service.h"
#include "tests/support/input_snapshot_builder.h"
#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/scenes/application_failure_scene.h"
#include "engine/core/render/render_command.h"
#include "engine/effects/number/floating_number_effect.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/core/time.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "game/showcase/shared/showcase_gallery_scene.h"
#include "game/showcase/camera/camera_showcase_scene.h"
#include "game/showcase/input/local_multiplayer_scene.h"
#include "engine/input/input_system.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/effects/effects_showcase_scene.h"
#include "game/showcase/ui/ui_component_gallery_scene.h"
#include "engine/ui/widgets/ui_action_button.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "game/navigation/showcase_scene_keys.h"
#include "engine/tools/debug_draw.h"
#include "engine/typography/font_resolver.h"
#include "engine/localization/localization_manager.h"
#include "engine/io/path/path_manager.h"
#include "tests/support/test_assertions.h"
#include "tests/support/scene_test_access.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <variant>

namespace
{
using elysia::tests::require;

class SdlFixture
{
public:
    SdlFixture()
    {
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
            "Engine test scene tests must initialize SDL video and audio");
        require(TTF_Init(),
            "Engine test scene tests must initialize SDL_ttf");
        require(elysia::tests::open_test_mixer(),
            "Engine test scene tests must open SDL_mixer audio");
        _surface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "Engine test scene tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "Engine test scene tests must create a software renderer");
    }

    ~SdlFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_DestroySurface(_surface);
        elysia::tests::close_test_mixer();
        TTF_Quit();

        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return _renderer; }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};

void test_gallery_hud(const elysia::scene::SceneRuntimeContext& context,SDL_Renderer* renderer)
{
    using namespace elysia;
    example::scene::UiComponentGalleryScene scene;
    scene::SceneTestAccess::bind(scene,context);
    scene::SceneTestAccess::enter(scene,example::scene::ShowcaseEnterPayload{
        .return_route = { .target=1 } });
    auto* root = scene::SceneTestAccess::ui_root(scene,0);
    ui::UiTabContainer* tabs = nullptr;
    std::vector<ui::UiActionButton*> slots;
    const auto visit = [&](auto&& self,ui::UiElement& element) -> void {
        if (auto* tab = dynamic_cast<ui::UiTabContainer*>(&element); tab && !tabs) tabs = tab;
        if (auto* slot = dynamic_cast<ui::UiActionButton*>(&element)) slots.push_back(slot);
        if (auto* host = dynamic_cast<ui::UiChildHost*>(&element))
            for (std::size_t index = 0; index < host->child_count(); ++index)
                self(self,*host->child_at(index));
    };
    visit(visit,*root);
    require(tabs && slots.size() == 4,"controls page must contain the skill, two items and pause HUD slots");
    require(tabs->set_selected_index(2),"HUD example must be accessible on the controls page");
    scene::SceneTestAccess::update(scene,0);
    SDL_SetRenderDrawColor(renderer,20,24,32,255);
    SDL_RenderClear(renderer);
    scene::SceneTestAccess::render(scene,renderer);
    if (const char* path = SDL_getenv("ELYSIA_HUD_QA_PATH"))
    {
        SDL_Surface* capture = SDL_RenderReadPixels(renderer,nullptr);
        require(capture != nullptr && IMG_SavePNG(capture,path),"HUD example capture must save");
        SDL_DestroySurface(capture);
    }
    tests::InputSnapshotBuilder devices;
    const auto route = [&] { scene::SceneTestAccess::route_input(scene,devices.take()); };
    const auto key = [&](input::RawInputControl control) {
        devices.press(control,true); route();
        devices.press(control,false); route();
    };
    route();
    devices.press(input::RawInputControl::KeyE,true); route();
    require(slots[0]->badge_text().value == "1" && slots[0]->overlay_ratio() == 1 && slots[0]->is_external_pressed(),
        "gameplay key must cast once and sync skill count, cooldown and external hold");
    route();
    require(slots[0]->badge_text().value == "1","holding key must not repeat the skill");
    devices.press(input::RawInputControl::KeyE,false); route();
    key(input::RawInputControl::KeyP);
    scene::SceneTestAccess::update(scene,1);
    require(slots[3]->is_selected() && slots[0]->overlay_ratio() == 1,"demo pause must freeze cooldown");
    key(input::RawInputControl::Key1);
    require(slots[1]->badge_text().value == "8","paused demo must not consume an item");
    key(input::RawInputControl::KeyP);
    scene::SceneTestAccess::update(scene,3);
    require(slots[0]->is_enabled() && slots[0]->overlay_ratio() == 0,"skill must become ready after its cooldown");
    const auto click = [&](ui::UiActionButton& slot) {
        const auto center = slot.presentation_screen_rect().center();
        for (auto type : { input::RawInputEventType::ControlPressed,input::RawInputEventType::ControlReleased })
        {
            devices.event({ .control=input::RawInputControl::MouseLeft,.type=type,
                .mouse_x=static_cast<int>(center.x),.mouse_y=static_cast<int>(center.y) });
            route();
        }
    };
    click(*slots[0]);
    require(slots[0]->badge_text().value == "2","mouse and keyboard must share the game skill action");
    click(*slots[2]);
    require(slots[2]->badge_text().value == "2" && slots[2]->is_selected() && !slots[1]->is_selected(),
        "successful item click must consume once and select the used slot");
    key(input::RawInputControl::Key2);
    key(input::RawInputControl::Key2);
    require(slots[2]->badge_text().value == "0" && !slots[2]->is_enabled(),"empty stock must disable item use");
    click(*slots[3]);
    require(slots[3]->is_selected(),"mouse pause must use the same game pause action");
    key(input::RawInputControl::KeyP);
    devices.press(input::RawInputControl::KeyE,true); route();
    require(slots[0]->is_external_pressed(),"external hold must sync during cooldown");
    require(tabs->set_selected_index(0) && !slots[0]->is_pressed(),"leaving HUD page must clear interaction state");
    key(input::RawInputControl::Key1);
    require(slots[1]->badge_text().value == "8","hidden HUD must ignore its gameplay shortcuts");
    scene::SceneTestAccess::exit(scene);
}

struct ReturnPayload
{
    int marker = 0;
};

template <int Id>
class ReturnScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const ReturnPayload* route_payload =
            elysia::scene::try_scene_payload<ReturnPayload>(payload);
        if (!route_payload)
            throw std::logic_error("ReturnScene requires ReturnPayload.");
        marker = route_payload->marker;
    }

    void on_exit() override {}
    void on_reset() override {}

    static inline int marker = 0;
};

using FirstReturnScene = ReturnScene<1>;
using SecondReturnScene = ReturnScene<2>;

bool throws_logic_error_containing(
    const std::function<void()>& operation,
    std::string_view expected)
{
    try
    {
        operation();
    }
    catch (const std::logic_error& error)
    {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

void send_escape(elysia::scene::SceneManager& scene_manager)
{
    scene_manager.on_input(elysia::tests::events_snapshot({elysia::input::RawInputEvent{.control = elysia::input::RawInputControl::KeyEscape,
                                      .type = elysia::input::RawInputEventType::ControlPressed,
                                      .device = elysia::input::InputDevice::Keyboard}}));
}

void send_key(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control,
    elysia::input::RawInputEventType type)
{
    scene_manager.on_input(elysia::tests::events_snapshot({elysia::input::RawInputEvent{
            .control = control, .type = type, .device = elysia::input::InputDevice::Keyboard}}));
}

void press_and_release_key(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control)
{
    send_key(scene_manager,control,elysia::input::RawInputEventType::ControlPressed);
    send_key(scene_manager,control,elysia::input::RawInputEventType::ControlReleased);
}

void click_mouse(
    elysia::scene::SceneManager& scene_manager,
    int x,
    int y)
{
    for (const auto type : {
            elysia::input::RawInputEventType::ControlPressed,
            elysia::input::RawInputEventType::ControlReleased})
    {
        scene_manager.on_input(elysia::tests::events_snapshot({elysia::input::RawInputEvent{.control = elysia::input::RawInputControl::MouseLeft,
                                          .type = type,
                                          .device = elysia::input::InputDevice::Mouse,
                                          .mouse_x = x,
                                          .mouse_y = y}}));
    }
}

void test_engine_feature_overlay_cycle()
{
    example::scene::EffectsShowcaseScene scene;
    require(scene.color_overlay_index() == 2,
        "Engine feature test must start with the blue overlay");

    const std::vector events{
        elysia::input::RawInputEvent{
            .control = elysia::input::RawInputControl::KeySpace,
            .type = elysia::input::RawInputEventType::ControlPressed,
            .device = elysia::input::InputDevice::Keyboard
        }
    };
    const std::array<std::size_t,5> expected_indices{ 3,4,0,1,2 };
    for (const std::size_t expected_index : expected_indices)
    {
        elysia::scene::SceneTestAccess::route_input(
            scene, elysia::tests::events_snapshot(events));
        require(scene.color_overlay_index() == expected_index,
            "Space must cycle all Engine feature color overlays and wrap");
    }

    elysia::scene::SceneTestAccess::reset(scene);
    require(scene.color_overlay_index() == 2,
        "reset must restore the Engine feature test default overlay");
}

void test_payload_contract_names_each_scene()
{
    example::scene::ShowcaseGalleryScene home_scene;
    require(throws_logic_error_containing(
            [&home_scene] { elysia::scene::SceneTestAccess::enter(home_scene); },
            "ShowcaseGalleryScene"),
        "ShowcaseGalleryScene must name itself when the demo payload is missing");

    example::scene::UiComponentGalleryScene ui_test_scene;
    require(throws_logic_error_containing(
            [&ui_test_scene] { elysia::scene::SceneTestAccess::enter(ui_test_scene); },
            "UiComponentGalleryScene"),
        "UiComponentGalleryScene must name itself when the demo payload is missing");

    example::scene::EffectsShowcaseScene feature_test_scene;
    const elysia::scene::ScenePayload invalid_payload =
        example::scene::ShowcaseEnterPayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
            [&feature_test_scene,&invalid_payload] {
                elysia::scene::SceneTestAccess::enter(feature_test_scene, invalid_payload);
            },
            "EffectsShowcaseScene"),
        "EffectsShowcaseScene must name itself when the return route is invalid");
}

void test_camera_showcase_controls()
{
    using namespace example::showcase::camera;
    using namespace elysia::camera;
    using Access=elysia::scene::SceneTestAccess;
    SdlFixture fixture;
    auto settings=elysia::typography::resolve_font_settings({});
    auto* builtin=elysia::builtin::BuiltinResources::instance();
    require(builtin->initialize(fixture.renderer(),elysia::builtin::BuiltinAssetCatalog(ELYSIA_SOURCE_DIR),settings->engine_point_sizes()).has_value(),"camera assets initialize");
    elysia::typography::FontResolver fonts;
    const std::array<std::string,1> languages{"en"};
    require(fonts.configure(*settings,*ELYSIA_RESOURCES,languages).has_value(),"camera fonts configure");
    require(elysia::io::PathManager::instance()->initialize(ELYSIA_SOURCE_DIR),"camera paths initialize");
    auto* localization=elysia::localization::LocalizationManager::instance();
    require(localization->initialize(fixture.renderer(),std::filesystem::path(ELYSIA_SOURCE_DIR)/"assets/configs/manifests/i18n_manifest.json","en",&fonts).has_value(),"camera text initializes");
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(fixture.renderer(),registry,1280,720,&fonts);
    elysia::scene::SceneManager service_owner;service_owner.initialize(context);
    (void)elysia::gameplay::ControllerService::instance()->begin_session();
    {
        example::scene::CameraShowcaseScene scene;
        Access::bind(scene,context);Access::attach(scene);
        CameraManager::instance()->set_center(CameraSlot::Auxiliary1,{321,456});
        Access::enter(scene,example::scene::ShowcaseEnterPayload{{.target=1}});
        auto& runtime=Access::camera_runtime(scene);
        const auto step=[&](double seconds){Access::update(scene,seconds);};
        const auto run=[&](double seconds){for(int i=0;i<int(seconds*60);++i)step(1.0/60);};
        const auto capture=[&](const std::string& name) {
            if(const char* directory=SDL_getenv("ELYSIA_CAMERA_QA_DIR")) {
                SDL_SetRenderDrawColor(fixture.renderer(),20,24,32,255);SDL_RenderClear(fixture.renderer());Access::render(scene,fixture.renderer());
                std::filesystem::create_directories(directory);auto* pixels=SDL_RenderReadPixels(fixture.renderer(),nullptr);
                require(pixels!=nullptr,"camera phase capture has pixels");require(IMG_SavePNG(pixels,(std::filesystem::path(directory)/(name+".png")).string().c_str()),"camera phase capture saves");SDL_DestroySurface(pixels);
            }
        };

        for(const auto mode:{FollowMode::Hard,FollowMode::Smooth,FollowMode::DeadZone,FollowMode::MultiTarget}) {
            scene.perform(CameraAction::Strategy);require(scene.state().strategy==mode,"all four follow modes cycle in order");
            step(0);require(runtime.slot_camera(CameraSlot::Main).zoom()==1,"strategy changes reset zoom");
        }
        scene.perform(CameraAction::Edge);run(1);
        require(scene.state().bounds,"edge preset enables real world bounds");
        const auto view=runtime.slot_camera(CameraSlot::Main).view_rect();
        require(view.right()<=900.01f && view.bottom()<=700.01f,"camera remains inside the configured world boundary");
        scene.select_page(CameraPage::Motion);
        const auto key=[&](elysia::input::RawInputControl control) {
            elysia::tests::InputSnapshotBuilder input;
            input.press(control,true);Access::input(scene,input.take());
            input.press(control,false);Access::input(scene,input.take());
        };
        key(elysia::input::RawInputControl::KeyDown);key(elysia::input::RawInputControl::KeyEnter);
        require(scene.state().motion.has_value(),"keyboard focus navigation and confirm invoke the same Move action");
        scene.perform(CameraAction::Reset);
        scene.perform(CameraAction::Teleport); // Hidden page action.
        require(Access::game_object(scene,1)->center()==elysia::core::Vector2{120,0},"hidden follow actions cannot mutate targets");
        for(int easing=0;easing<3;++easing) {
            scene.perform(CameraAction::Reset);for(int i=0;i<easing;++i)scene.perform(CameraAction::Easing);
            scene.perform(CameraAction::EndBehavior);scene.perform(CameraAction::Move);const auto start=scene.camera().center();
            step(.25);const auto pose=scene.camera().center();
            const auto fraction=static_cast<float>(apply_camera_easing(scene.state().easing,.25));
            require(std::abs(pose.x-(start.x+400*fraction))<.01f,"move uses selected easing");
            scene.perform(CameraAction::Pause);step(.5);require(scene.camera().center()==pose,"paused move preserves its pose");
            scene.perform(CameraAction::Resume);step(.75);
            require(runtime.motion_state(*scene.state().motion)==CameraMotionState::Holding,"move ends in an owned Hold");
            scene.perform(CameraAction::Cancel);require(!scene.state().motion,"cancel releases held movement");
        }
        scene.perform(CameraAction::Reset);scene.perform(CameraAction::EndBehavior);scene.perform(CameraAction::Path);
        const auto origin=scene.camera().center();step(1);require(scene.camera().center()==origin+elysia::core::Vector2{400,0},"path reaches first waypoint");
        step(1);require(scene.camera().center()==origin+elysia::core::Vector2{400,240},"path reaches second waypoint");
        step(1);require(scene.camera().center()==origin,"path returns to its origin");
        capture("motion_path_hold");
        scene.perform(CameraAction::Follow);require(!scene.state().motion,"restore follow releases held path");
        scene.perform(CameraAction::EndBehavior);scene.perform(CameraAction::Move);step(1);require(!scene.state().motion,"ResumeFollow releases completed motion");
        step(.1);require(scene.camera().center().x<400,"normal following resumes after movement completion");
        scene.select_page(CameraPage::Follow);scene.perform(CameraAction::Automatic);
        scene.select_page(CameraPage::Cinematic);scene.perform(CameraAction::Play);
        require(scene.state().saved_automatic && !scene.state().automatic,"cinematic saves and freezes automatic movement");
        run(6);require(scene.state().automatic && !scene.state().frozen,"return restores the previous automatic movement setting");
        for(const auto phase:{CinematicPhase::Entering,CinematicPhase::Touring,CinematicPhase::Holding,CinematicPhase::Returning}) {
            scene.perform(CameraAction::Reset);scene.perform(CameraAction::Play);
            for(int i=0;i<400 && scene.state().phase!=phase;++i)step(1.0/60);
            require(scene.state().phase==phase,"cinematic reaches each phase via completion events");
            capture("cinematic_phase_"+std::to_string(static_cast<int>(phase)));
            scene.perform(CameraAction::Pause);const auto pose=scene.camera().center();const auto zoom=scene.camera().zoom();
            const auto actor=Access::game_object(scene,0)->center();
            elysia::tests::InputSnapshotBuilder input;input.press(elysia::input::RawInputControl::KeyD,true);input.press(elysia::input::RawInputControl::KeyE,true);
            Access::input(scene,input.take());run(.5);
            require(scene.state().phase==phase && scene.camera().center()==pose && scene.camera().zoom()==zoom,"pause freezes blend, path, hold and return presentation");
            require(Access::game_object(scene,0)->center()==actor,"cinematic gates actor commands while preserving bindings");
            input.press(elysia::input::RawInputControl::KeyD,false);input.press(elysia::input::RawInputControl::KeyE,false);Access::input(scene,input.take());
            scene.perform(CameraAction::Resume);scene.perform(CameraAction::Skip);run(1);
            require(scene.state().phase==CinematicPhase::Idle && !scene.state().frozen && runtime.presented_slot()==CameraSlot::Main,"skip from any phase returns to Main and restores actors");
        }
        scene.perform(CameraAction::Play);run(6);require(scene.state().phase==CinematicPhase::Idle,"full show automatically returns after its hold");
        const auto restored=Access::game_object(scene,0)->center();
        elysia::tests::InputSnapshotBuilder movement;movement.press(elysia::input::RawInputControl::KeyD,true);Access::input(scene,movement.take());run(.5);
        require(Access::game_object(scene,0)->center().x>restored.x+100,"original control binding resumes after cinematic completion");
        movement.press(elysia::input::RawInputControl::KeyD,false);Access::input(scene,movement.take());

        scene.perform(CameraAction::Cut);require(runtime.presented_slot()==CameraSlot::Cinematic && scene.state().frozen,"manual cut freezes actors and presents Cinematic");
        scene.perform(CameraAction::Return);run(1);scene.perform(CameraAction::Blend);run(1);
        require(scene.state().phase==CinematicPhase::Manual,"manual blend stops at the Cinematic view");
        scene.perform(CameraAction::Replay);run(1);require(scene.state().phase==CinematicPhase::Touring,"replay replaces manual view with a fresh show");
        scene.select_page(CameraPage::Follow);require(!scene.state().motion && !scene.state().blend && !scene.state().frozen,"switching pages cancels all cinematic activity");
        scene.select_page(CameraPage::Cinematic);scene.perform(CameraAction::Play);step(.2);
        Access::exit(scene);require(!scene.state().motion && !scene.state().blend && !scene.state().frozen,"exit cleans up running show");
        Access::detach(scene);Access::attach(scene);Access::enter(scene,example::scene::ShowcaseEnterPayload{{.target=1}});
        require(scene.state().page==CameraPage::Follow && scene.camera().zoom()==1,"reentry restores the default page and pose");
        require(CameraManager::instance()->camera(CameraSlot::Auxiliary1).center()==elysia::core::Vector2{321,456},"showcase does not alter unowned auxiliary slots");
        Access::exit(scene);Access::detach(scene);Access::reset(scene);
    }
    require(service_owner.shutdown(),"camera test services shut down");localization->shutdown();fonts.shutdown();builtin->shutdown();
}

void test_escape_returns_the_full_caller_route()
{
    SdlFixture fixture;
    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(elysia::typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "Engine test scene tests must resolve default font settings");
    auto& builtin_resources = *elysia::builtin::BuiltinResources::instance();
    require(builtin_resources.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(std::filesystem::path{ ELYSIA_SOURCE_DIR }),
                resolved_font_settings->engine_point_sizes())
                .has_value(),
        "Engine test scene tests must initialize built-in resources");

    elysia::typography::FontResolver font_resolver;
    const std::array<std::string,1> supported_languages{ "en" };
    require(font_resolver.configure(
                *resolved_font_settings,
                *elysia::resources::ResourceService::instance(),
                supported_languages)
                .has_value(),
        "Engine test scene tests must configure floating-number fonts");
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        fixture.renderer(),&font_resolver);
    const auto screen_image_path = std::filesystem::path{ELYSIA_SOURCE_DIR} / "assets/textures/ui/screen_effect_test.svg";
    elysia::resources::TexturePtr screen_image(IMG_LoadTexture(fixture.renderer(), screen_image_path.string().c_str()));
    require(screen_image != nullptr, "Screen effect demo image must decode");
    require(elysia::resources::ResourceManager::instance()->store_texture("demo.screen_effect",std::move(screen_image)).has_value(),
        "Screen effect demo image must register");

    require(elysia::io::PathManager::instance()->initialize(ELYSIA_SOURCE_DIR),
        "camera demo captures must resolve the asset root");
    auto* localization = elysia::localization::LocalizationManager::instance();
    require(localization->initialize(fixture.renderer(),
        std::filesystem::path(ELYSIA_SOURCE_DIR) / "assets/configs/manifests/i18n_manifest.json",
        "en", &font_resolver), "camera demo captures must initialize text rendering");
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),registry,1280,720,&font_resolver);
    elysia::scene::SceneManager scene_manager;
    scene_manager.initialize(context);
    scene_manager.register_game_scene<example::scene::ShowcaseGalleryScene>(
        example::scene_keys::ShowcaseGallery);
    scene_manager.register_game_scene<example::scene::UiComponentGalleryScene>(
        example::scene_keys::UiComponentGallery);
    scene_manager.register_game_scene<example::scene::EffectsShowcaseScene>(
        example::scene_keys::EffectsShowcase);
    scene_manager.register_game_scene<example::scene::CameraShowcaseScene>(
        example::scene_keys::CameraShowcase);
    scene_manager.register_game_scene<example::scene::LocalMultiplayerScene>(
        example::scene_keys::LocalMultiplayer);
    scene_manager.register_game_scene<FirstReturnScene>(1);
    scene_manager.register_game_scene<SecondReturnScene>(2);
    scene_manager.register_engine_scene<
        elysia::builtin::ApplicationFailureScene>(
            elysia::builtin::SceneKeys::ApplicationFailure);

    if (!elysia::gameplay::ControllerService::instance()->session_active())

        (void)elysia::gameplay::ControllerService::instance()->begin_session();

    scene_manager.start(elysia::scene::SceneRoute{
        .target = example::scene_keys::UiComponentGallery,
        .payload = example::scene::ShowcaseEnterPayload{
            .return_route = elysia::scene::SceneRoute{
                .target = 1,
                .payload = ReturnPayload{ .marker = 17 },
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        }
    });

    // The gallery owns eight fixed-width tabs. Visit every page through the same
    // keyboard focus path used at runtime, then update and render its active tree.
    for (std::size_t page_index = 0; page_index < 8; ++page_index)
    {
        scene_manager.on_update(1.0 / 60.0);
        scene_manager.on_render(fixture.renderer());
        if (page_index + 1 < 8)
        {
            press_and_release_key(
                scene_manager,elysia::input::RawInputControl::KeyRight);
            press_and_release_key(
                scene_manager,elysia::input::RawInputControl::KeyEnter);
        }
    }

    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 17,
        "UiComponentGalleryScene Escape must return the caller key and payload");

    auto* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->clear();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(
        elysia::tools::DebugDrawCategory::Gameplay);

    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = example::scene_keys::EffectsShowcase,
            .payload = example::scene::ShowcaseEnterPayload{
                .return_route = elysia::scene::SceneRoute{
                    .target = 2,
                    .payload = ReturnPayload{ .marker = 29 },
                    .reload_mode = elysia::scene::SceneReloadMode::Reset
                }
            }
        }
    });
    scene_manager.on_update(0.0);
    require(debug_draw->is_enabled(
                elysia::tools::DebugDrawCategory::PhysicsCollider)
            && debug_draw->commands().size() == 1,
        "Engine feature test must temporarily enable and submit the character collider");
    const auto* initial_collider = std::get_if<elysia::tools::DebugDrawRect>(
        &debug_draw->commands().front().primitive);
    require(initial_collider != nullptr,
        "Engine feature test character must submit an AABB debug command");
    const float initial_collider_x = initial_collider->rect.x();

    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyDown);
    for (int index = 0; index < 5; ++index)
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyRight);
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
    auto* decimal_effect = ELYSIA_OBJECT_QUERY->find_object<
        elysia::effects::FloatingNumberEffect>();
    require(decimal_effect != nullptr,
        "Engine feature controls must navigate to and spawn the final scrolling number preset");
    std::vector<elysia::core::RenderCommand> decimal_commands;
    decimal_effect->submit_render_commands(decimal_commands);
    require(decimal_commands.size() == 4,
        "The final Engine feature number preset must render the four glyphs in 12.5");

    scene_manager.on_input(elysia::tests::events_snapshot({elysia::input::RawInputEvent{.control = elysia::input::RawInputControl::KeyD,
                                      .type = elysia::input::RawInputEventType::ControlPressed,
                                      .device = elysia::input::InputDevice::Keyboard}}));
    scene_manager.on_update(0.25);
    const auto* moved_collider = std::get_if<elysia::tools::DebugDrawRect>(
        &debug_draw->commands().front().primitive);
    require(debug_draw->commands().size() == 1 && moved_collider
            && moved_collider->rect.x() > initial_collider_x,
        "Engine feature test must refresh one collider snapshot at the moved character position");
    const auto capture_screen = [&](const char* name) {
        SDL_SetRenderDrawColor(fixture.renderer(),20,24,32,255);
        SDL_RenderClear(fixture.renderer());
        scene_manager.on_render(fixture.renderer());
        if (const char* directory = SDL_getenv("ELYSIA_SCREEN_EFFECT_QA_DIR"))
        {
            std::filesystem::create_directories(directory);
            SDL_Surface* capture = SDL_RenderReadPixels(fixture.renderer(),nullptr);
            require(capture != nullptr, "Screen effect demo capture must read pixels");
            const auto path = std::filesystem::path(directory) / (std::string(name) + ".png");
            require(IMG_SavePNG(capture,path.string().c_str()), "Screen effect demo capture must save PNG");
            SDL_DestroySurface(capture);
        }
    };
    const auto screen_commands = [&](elysia::effects::ScreenEffectLayer layer) {
        std::vector<elysia::core::UiRenderCommand> commands;
        elysia::effects::EffectManager::instance()->append_screen_effect_commands(layer,{0,0,1280,720},commands);
        return commands;
    };
    capture_screen("01_controls");
    click_mouse(scene_manager,640,138); // Hold black button.
    elysia::core::Time::instance()->begin_frame(0.5);
    scene_manager.on_update(0.5);
    require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).size() == 1,
        "Visible Hold black button must call EffectService");
    capture_screen("02_black_hold");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == example::scene_keys::EffectsShowcase
        && screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).empty(),
        "Escape during black hold must clear the effect without leaving the lab");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF1);
    require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).front().color == elysia::core::Color(255,255,255),
        "F1 shortcut must trigger a white flash");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF5);
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF9);
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF7);
    auto cover = screen_commands(elysia::effects::ScreenEffectLayer::BeforeUi);
    require(cover.size() == 1 && cover.front().texture != nullptr
        && cover.front().screen_rect.height() > 720, "F7 must request Cover image before UI");
    elysia::core::Time::instance()->begin_frame(0.3);
    scene_manager.on_update(0.3);
    capture_screen("03_cover_before_ui");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF8);
    auto contain = screen_commands(elysia::effects::ScreenEffectLayer::BeforeUi);
    require(contain.size() == 1 && contain.front().screen_rect.width() < 1280, "F8 must request Contain image");
    elysia::core::Time::instance()->begin_frame(0.3);
    scene_manager.on_update(0.3);
    capture_screen("04_contain_before_ui");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF4);
    elysia::core::Time::instance()->begin_frame(0.6);
    scene_manager.on_update(0.6);
    require(screen_commands(elysia::effects::ScreenEffectLayer::BeforeUi).empty(), "F4 must stop the active preview with fade out");
    click_mouse(scene_manager,888,186); // Toggle the layer with the same action as F9.
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF6);
    const auto stretch=screen_commands(elysia::effects::ScreenEffectLayer::AfterUi);
    require(stretch.size()==1 && stretch.front().screen_rect==elysia::core::Rect{0,0,1280,720},
        "mouse layer toggle and F6 must stretch the image over the logical viewport");
    elysia::core::Time::instance()->begin_frame(0.3);
    scene_manager.on_update(0.3);
    capture_screen("05_stretch_after_ui");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF2);
    elysia::core::Time::instance()->begin_frame(1.2);
    scene_manager.on_update(1.2);
    require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).empty(),
        "F2 must complete its timed fade without leaving the showcase");
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF3);
    click_mouse(scene_manager,1172,678);
    require(scene_manager.current_scene_key()==example::scene_keys::EffectsShowcase
        && screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).empty(),
        "Back must cancel an active screen effect using the same action as Escape");
    click_mouse(scene_manager,1172,678);
    require(scene_manager.current_scene_key() == 2 && SecondReturnScene::marker == 29,
        "EffectsShowcaseScene Escape must return the caller key and payload");
    require(!debug_draw->enabled()
            && debug_draw->enabled_categories()
                == elysia::tools::DebugDrawCategory::Gameplay,
        "leaving EffectsShowcaseScene must restore the previous DebugDraw settings");

    for(const auto reload:{elysia::scene::SceneReloadMode::Reuse,elysia::scene::SceneReloadMode::Reset}){
        scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,
            .route={.target=example::scene_keys::EffectsShowcase,
                .payload=example::scene::ShowcaseEnterPayload{{.target=2,.payload=ReturnPayload{29}}},
                .reload_mode=reload}});
        scene_manager.on_update(0);
        require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).empty()
            && screen_commands(elysia::effects::ScreenEffectLayer::BeforeUi).empty(),
            "cached and reset showcases reenter without stale screen effects");
        click_mouse(scene_manager,640,138);
        require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).size()==1,
            "rebuilt screen controls keep valid callbacks and reset their layer");
        scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,
            .route={.target=2,.payload=ReturnPayload{29}}});
        scene_manager.on_update(0);
        require(screen_commands(elysia::effects::ScreenEffectLayer::AfterUi).empty(),
            "leaving a showcase cancels its active manual screen effect");
    }

    const elysia::scene::SceneRoute original_caller{
        .target = 1,
        .payload = ReturnPayload{ .marker = 41 },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = example::scene_keys::ShowcaseGallery,
            .payload = example::scene::ShowcaseEnterPayload{
                .return_route = original_caller
            }
        }
    });
    scene_manager.on_update(0.0);

    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key() == example::scene_keys::UiComponentGallery,
        "Gallery keyboard navigation must open the selected child page");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == example::scene_keys::ShowcaseGallery,
        "Demo child Escape must return to ShowcaseGalleryScene");
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyDown);
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyDown);
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
        require(scene_manager.current_scene_key() == example::scene_keys::CameraShowcase,
            "Down after returning to Gallery must open the next menu entry");
        send_escape(scene_manager);
        require(scene_manager.current_scene_key() == example::scene_keys::ShowcaseGallery,
            "Engine feature Escape must return to the cached Gallery");
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyUp);
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyUp);
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
        require(scene_manager.current_scene_key() == example::scene_keys::UiComponentGallery,
            "Up after returning to Gallery must open the previous menu entry");
        send_escape(scene_manager);
        require(scene_manager.current_scene_key() == example::scene_keys::ShowcaseGallery,
            "repeated child visits must keep returning to Gallery");
    }
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 41,
        "ShowcaseGalleryScene must preserve and return the original caller route");

    const auto open_gallery = [&scene_manager,&original_caller]()
    {
        scene_manager.on_scene_request(elysia::scene::SceneRequest{
            .type = elysia::scene::SceneRequestType::Switch,
            .route = {
                .target = example::scene_keys::ShowcaseGallery,
                .payload = example::scene::ShowcaseEnterPayload{
                    .return_route = original_caller},
                .reload_mode = elysia::scene::SceneReloadMode::Recreate}});
        scene_manager.on_update(0.0);
    };

    open_gallery();
    for(int i=0;i<9;++i)press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyDown);
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key() == example::scene_keys::ShowcaseGallery,
        "Failure Test must open a confirmation without immediately leaving Gallery");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == example::scene_keys::ShowcaseGallery,
        "Canceling the Failure confirmation must remain in Gallery");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == 1,
        "Gallery Escape must resume returning to its caller after closing the modal");

    open_gallery();
    for(int i=0;i<9;++i)press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyDown);
    press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyEnter);
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyRight);
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key()
            == elysia::builtin::SceneKeys::ApplicationFailure,
        "Confirming the guarded Failure Test must enter the engine failure scene");

    test_gallery_hud(context,fixture.renderer());
    // Exercise the actual demo UI and optionally export deterministic render captures.
    auto* cameras = elysia::camera::CameraManager::instance();
    cameras->set_viewport_size(elysia::camera::CameraSlot::Main, {1280, 720});
    auto enter_camera = [&] {
        scene_manager.on_scene_request(elysia::scene::SceneRequest{
            .type = elysia::scene::SceneRequestType::Switch,
            .route = {.target = example::scene_keys::CameraShowcase,
                .payload = example::scene::ShowcaseEnterPayload{.return_route = original_caller},
                .reload_mode = elysia::scene::SceneReloadMode::Reuse}});
        scene_manager.on_update(0);
    };
    auto render_camera = [&](const char* name) {
        SDL_SetRenderDrawColor(fixture.renderer(), 20, 24, 32, 255);
        SDL_RenderClear(fixture.renderer());
        scene_manager.on_render(fixture.renderer());
        if (const char* directory = SDL_getenv("ELYSIA_CAMERA_QA_DIR"))
        {
            std::filesystem::create_directories(directory);
            SDL_Surface* capture = SDL_RenderReadPixels(fixture.renderer(), nullptr);
            require(capture != nullptr, "camera demo capture must read rendered pixels");
            const auto path = std::filesystem::path(directory) / (std::string(name) + ".png");
            require(IMG_SavePNG(capture, path.string().c_str()), "camera demo capture must save PNG");
            SDL_DestroySurface(capture);
        }
    };
    enter_camera();
    scene_manager.on_update(0.1);
    render_camera("01_initial");
    for (int frame = 0; frame < 180; ++frame) scene_manager.on_update(1.0 / 60.0);
    require(cameras->camera(elysia::camera::CameraSlot::Main).zoom() > 1.9f,
        "demo close targets must zoom in after the settle delay");
    render_camera("01b_zoomed_in");
    click_mouse(scene_manager, 995, 678);
    elysia::input::RawInputFrame movement;
    movement.state.set_pressed(elysia::input::RawInputControl::KeyD, true);
    scene_manager.on_input(elysia::tests::events_snapshot({{.control=elysia::input::RawInputControl::KeyD,.type=elysia::input::RawInputEventType::ControlPressed}}));
    for (int frame = 0; frame < 180; ++frame) scene_manager.on_update(1.0 / 60.0);
    require(cameras->camera(elysia::camera::CameraSlot::Main).center().x > 100,
        "WASD movement must move the tracked primary and its camera");
    scene_manager.on_input(elysia::tests::events_snapshot({}));
    click_mouse(scene_manager, 995, 678);
    click_mouse(scene_manager, 1116, 154); // Teleport the secondary target.
    scene_manager.on_update(0.1);
    const float first_zoom = cameras->camera(elysia::camera::CameraSlot::Main).zoom();
    require(first_zoom > 0.5f && first_zoom < 1,
        "demo teleport must trigger smooth outward zoom");
    render_camera("02_teleport");
    for (int frame = 0; frame < 360; ++frame) scene_manager.on_update(1.0 / 60.0);
    require(std::abs(cameras->camera(elysia::camera::CameraSlot::Main).zoom() - 0.5f) < 0.001f,
        "demo must settle at minimum zoom when targets separate too far");
    render_camera("03_primary_only");
    click_mouse(scene_manager, 995, 678); // Reset.
    scene_manager.on_update(0);
    require(cameras->camera(elysia::camera::CameraSlot::Main).zoom() == 1,
        "demo reset must restore initial zoom");
    click_mouse(scene_manager, 145, 202); // Manual zoom.
    scene_manager.on_update(1);
    require(cameras->camera(elysia::camera::CameraSlot::Main).zoom() == 1.5f,
        "demo manual zoom must own its completion frame");
    click_mouse(scene_manager, 390, 154); // DeadZone off.
    click_mouse(scene_manager, 632, 154); // Swap primary.
    click_mouse(scene_manager, 390, 202); // Bounds on.
    scene_manager.on_update(0.1);
    render_camera("04_controls");
    click_mouse(scene_manager, 995, 678);
    click_mouse(scene_manager, 874, 154); // Automatic separation and reunion.
    for (int frame = 0; frame < 900; ++frame) scene_manager.on_update(1.0 / 60.0);
    render_camera("05_reunion");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 41,
        "camera demo must preserve the complete return route");
    enter_camera();
    require(cameras->camera(elysia::camera::CameraSlot::Main).zoom() == 1,
        "camera demo re-entry must reset camera state");
    render_camera("06_reentry");
    for(const auto reload:{elysia::scene::SceneReloadMode::Reset,elysia::scene::SceneReloadMode::Recreate}) {
        press_and_release_key(scene_manager,elysia::input::RawInputControl::KeyF3);
        click_mouse(scene_manager,880,154); // Start cinematic through the view callback.
        scene_manager.on_update(.2);
        send_escape(scene_manager);
        require(scene_manager.current_scene_key()==1 && FirstReturnScene::marker==41,"Escape during a blend preserves return payload");
        scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,
            .route={.target=example::scene_keys::CameraShowcase,.payload=example::scene::ShowcaseEnterPayload{original_caller},.reload_mode=reload}});
        scene_manager.on_update(0);
        require(cameras->camera(elysia::camera::CameraSlot::Main).zoom()==1 && cameras->camera(elysia::camera::CameraSlot::Cinematic).zoom()==1,
            "Reset and Recreate clear both camera slots after interrupted cinematic");
    }

    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = {.target = example::scene_keys::LocalMultiplayer,
                  .payload = example::scene::ShowcaseEnterPayload{.return_route = original_caller}}});
    scene_manager.on_update(0);
    auto blocks = ELYSIA_OBJECT_QUERY->find_objects<>();
    require(blocks.size() == 2, "Multiplayer demo creates exactly two command-controlled actors");
    if (blocks[0]->position().x > blocks[1]->position().x)
        std::swap(blocks[0], blocks[1]);
    elysia::input::InputSystem local_input;
    auto dispatch = [&] {
        scene_manager.on_input(local_input.snapshot());
        scene_manager.on_update(1.0 / 60);
    };
    auto pad_button = [&](Uint32 type, Uint8 button) {
        SDL_Event event{};
        event.type = type;
        event.gbutton.which = 77;
        event.gbutton.button = button;
        local_input.process_event(event);
    };
    auto key_event = [&](Uint32 type, SDL_Keycode code) {
        SDL_Event event{};
        event.type = type;
        event.key.key = code;
        local_input.process_event(event);
    };
    local_input.begin_frame();
    dispatch();
    const auto keyboard_first = blocks[0]->position().x, keyboard_second = blocks[1]->position().x;
    local_input.begin_frame();
    key_event(SDL_EVENT_KEY_DOWN, SDLK_D);
    key_event(SDL_EVENT_KEY_DOWN, SDLK_LEFT);
    dispatch();
    require(blocks[0]->position().x > keyboard_first && blocks[1]->position().x < keyboard_second,
            "Multiplayer defaults to independent WASD and arrow partitions without a gamepad");
    local_input.begin_frame();
    key_event(SDL_EVENT_KEY_UP, SDLK_D);
    key_event(SDL_EVENT_KEY_UP, SDLK_LEFT);
    dispatch();
    local_input.begin_frame();
    pad_button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_START);
    dispatch();
    auto owner = scene_manager.local_players().owner(elysia::input::InputSourceId::gamepad(77));
    require(owner.value && owner != elysia::input::PrimaryLocalPlayer, "Unassigned Start joins player two");
    const auto first_x = blocks[0]->position().x, second_x = blocks[1]->position().x;
    local_input.begin_frame();
    pad_button(SDL_EVENT_GAMEPAD_BUTTON_UP, SDL_GAMEPAD_BUTTON_START);
    pad_button(SDL_EVENT_GAMEPAD_BUTTON_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    key_event(SDL_EVENT_KEY_DOWN, SDLK_D);
    dispatch();
    require(blocks[0]->position().x > first_x && blocks[1]->position().x < second_x,
            "Joining must not open pause; keyboard and pad move different demo actors");
    render_camera("07_local_multiplayer_play");
    local_input.begin_frame();
    key_event(SDL_EVENT_KEY_DOWN, SDLK_ESCAPE);
    dispatch();
    const auto paused_first = blocks[0]->position(), paused_second = blocks[1]->position();
    local_input.begin_frame();
    dispatch();
    require(blocks[0]->position() == paused_first && blocks[1]->position() == paused_second,
            "Demo modal menu blocks both players");
    render_camera("08_local_multiplayer_menu");
    click_mouse(scene_manager, 640, 285); // Swap keyboard partitions while the modal menu is open.
    const auto &swapped_bindings=scene_manager.local_players().configuration();
    require(swapped_bindings.partitions.at(swapped_bindings.bindings.at(elysia::input::PrimaryLocalPlayer).keyboard).name=="Arrows" &&
            swapped_bindings.partitions.at(swapped_bindings.bindings.at(owner).keyboard).name=="WASD",
            "Menu atomically swaps partitions and rebinds controller maps");
    click_mouse(scene_manager, 640, 373); // Give the single mouse to P2 without changing UI access.
    require(scene_manager.local_players().owner(elysia::input::InputSourceId::mouse())==owner,
            "Mouse ownership can be transferred from the shared UI");

    auto* controls=elysia::gameplay::ControllerService::instance();
    auto first_controller=example::gameplay::session_player(elysia::input::PrimaryLocalPlayer);
    auto second_controller=example::gameplay::session_player(owner);
    scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,.route=original_caller});
    scene_manager.on_update(0);
    require(controls->get(first_controller) && !controls->describe(first_controller)->bound &&
            controls->get(second_controller) && !controls->describe(second_controller)->bound,
            "Menu transition preserves session controller instances but clears targets");
    const auto &restored_bindings=scene_manager.local_players().configuration();
    require(restored_bindings.partitions.at(restored_bindings.bindings.at(elysia::input::PrimaryLocalPlayer).keyboard).name=="Full keyboard" &&
            scene_manager.local_players().owner(elysia::input::InputSourceId::mouse())==elysia::input::PrimaryLocalPlayer,
            "Leaving multiplayer restores external keyboard and mouse configuration");

    scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,
        .route={.target=example::scene_keys::LocalMultiplayer,
                .payload=example::scene::ShowcaseEnterPayload{.return_route=original_caller},.reload_mode=elysia::scene::SceneReloadMode::Recreate}});
    scene_manager.on_update(0);
    require(controls->describe(first_controller)->bound && controls->describe(second_controller)->bound,
            "Recreated multiplayer scene explicitly restores both session controllers");
    blocks=ELYSIA_OBJECT_QUERY->find_objects<>();
    if(blocks[0]->position().x>blocks[1]->position().x) std::swap(blocks[0],blocks[1]);
    const auto restored_x=blocks[0]->position().x;
    local_input.begin_frame(); dispatch();
    require(blocks[0]->position().x==restored_x,"Scene transition cannot inherit a held movement");
    local_input.begin_frame(); key_event(SDL_EVENT_KEY_UP,SDLK_D); key_event(SDL_EVENT_KEY_UP,SDLK_ESCAPE);
    pad_button(SDL_EVENT_GAMEPAD_BUTTON_UP,SDL_GAMEPAD_BUTTON_DPAD_LEFT); dispatch();
    local_input.begin_frame(); key_event(SDL_EVENT_KEY_DOWN,SDLK_D);
    pad_button(SDL_EVENT_GAMEPAD_BUTTON_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT); dispatch();
    require(blocks[0]->position().x>restored_x,"Rebound session controller resumes after physical release");
    controls->end_session();
    require(!controls->get(first_controller) && !controls->get(second_controller),"Explicit session end invalidates demo handles");

    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(elysia::tools::DebugDrawCategory::Gameplay);
    scene_manager.on_scene_request({.type=elysia::scene::SceneRequestType::Switch,
        .route={.target=example::scene_keys::EffectsShowcase,
            .payload=example::scene::ShowcaseEnterPayload{.return_route=original_caller},
            .reload_mode=elysia::scene::SceneReloadMode::Recreate}});
    scene_manager.on_update(0);
    require(scene_manager.state()==elysia::scene::SceneManagerState::Faulted
            && !debug_draw->enabled()
            && debug_draw->enabled_categories()==elysia::tools::DebugDrawCategory::Gameplay,
        "Failed feature lab controller setup restores the previous global DebugDraw state");


    require(scene_manager.shutdown(), "Demo shutdown after session end restores devices without errors");
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        nullptr,nullptr);
    elysia::effects::EffectManager::instance()->clear_content();
    elysia::resources::ResourceManager::instance()->clear();
    localization->shutdown();
    font_resolver.shutdown();
    builtin_resources.shutdown();
}

void test_runtime_demo_sources_do_not_retain_legacy_names()
{
    const std::filesystem::path game_root =
        std::filesystem::path(ELYSIA_SOURCE_DIR) / "game";
    const std::array forbidden_tokens{
        std::string{"example::"} + "testbed",
        std::string{"example::"} + "physics_demo",
        std::string{"ExampleScene"} + "Keys",
        std::string{"Ui"} + "TestScene",
        std::string{"EngineFeature"} + "TestScene",
        std::string{"PhysicsDemo"} + "MenuScene",
        std::string{"PhysicsDemo"} + "SceneBase",
        std::string{"PhysicsCollision"} + "TestScene",
        std::string{"PlatformTilePhysics"} + "TestScene",
        std::string{"TopDownTilePhysics"} + "TestScene",
        std::string{"physics_"} + "test_scenes"};

    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(game_root))
    {
        if (!entry.is_regular_file())
            continue;
        const std::filesystem::path path = entry.path();
        const std::string extension = path.extension().string();
        if (extension != ".h" && extension != ".cpp")
            continue;

        std::ifstream input(path, std::ios::binary);
        const std::string source{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
        for (const std::string& token : forbidden_tokens)
            require(source.find(token) == std::string::npos,
                "Runtime demo sources must not retain legacy scene names");
    }
}
}

int main()
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    test_engine_feature_overlay_cycle();
    test_payload_contract_names_each_scene();
    test_camera_showcase_controls();
    test_escape_returns_the_full_caller_route();
    test_runtime_demo_sources_do_not_retain_legacy_names();
    return EXIT_SUCCESS;
}
