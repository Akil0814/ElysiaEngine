#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/scenes/application_failure_scene.h"
#include "engine/core/render/render_command.h"
#include "engine/effects/number/floating_number_effect.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/resources/resource_service.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "game/scene/demo/demo_gallery_scene.h"
#include "game/scene/demo/demo_scene_payload.h"
#include "game/scene/demo/engine_feature_lab_scene.h"
#include "game/scene/demo/ui_component_gallery_scene.h"
#include "game/scene/example_scene_keys.h"
#include "engine/tools/debug_draw.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

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
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
            "Engine test scene tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "Engine test scene tests must initialize PNG support");
        require(TTF_Init() == 0,
            "Engine test scene tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
            "Engine test scene tests must open SDL_mixer audio");
        _surface = SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "Engine test scene tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "Engine test scene tests must create a software renderer");
    }

    ~SdlFixture()
    {
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        Mix_CloseAudio();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return _renderer; }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};

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
    void reset() override {}

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
    scene_manager.on_input(
        elysia::input::RawInputFrame{},
        { elysia::input::RawInputEvent{
            .control = elysia::input::RawInputControl::KeyEscape,
            .type = elysia::input::RawInputEventType::ControlPressed,
            .device = elysia::input::InputDevice::Keyboard
        } });
}

void send_key(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control,
    elysia::input::RawInputEventType type)
{
    scene_manager.on_input(
        elysia::input::RawInputFrame{},
        { elysia::input::RawInputEvent{
            .control = control,
            .type = type,
            .device = elysia::input::InputDevice::Keyboard
        } });
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
        scene_manager.on_input(
            elysia::input::RawInputFrame{},
            {elysia::input::RawInputEvent{
                .control = elysia::input::RawInputControl::MouseLeft,
                .type = type,
                .device = elysia::input::InputDevice::Mouse,
                .mouse_x = x,
                .mouse_y = y}});
    }
}

void test_engine_feature_overlay_cycle()
{
    example::scene::EngineFeatureLabScene scene;
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
        scene.on_input(elysia::input::RawInputFrame{},events);
        require(scene.color_overlay_index() == expected_index,
            "Space must cycle all Engine feature color overlays and wrap");
    }

    scene.reset();
    require(scene.color_overlay_index() == 2,
        "reset must restore the Engine feature test default overlay");
}

void test_payload_contract_names_each_scene()
{
    example::scene::DemoGalleryScene home_scene;
    require(throws_logic_error_containing(
            [&home_scene] { home_scene.on_enter({}); },
            "DemoGalleryScene"),
        "DemoGalleryScene must name itself when the demo payload is missing");

    example::scene::UiComponentGalleryScene ui_test_scene;
    require(throws_logic_error_containing(
            [&ui_test_scene] { ui_test_scene.on_enter({}); },
            "UiComponentGalleryScene"),
        "UiComponentGalleryScene must name itself when the demo payload is missing");

    example::scene::EngineFeatureLabScene feature_test_scene;
    const elysia::scene::ScenePayload invalid_payload =
        example::scene::DemoScenePayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
            [&feature_test_scene,&invalid_payload] { feature_test_scene.on_enter(invalid_payload); },
            "EngineFeatureLabScene"),
        "EngineFeatureLabScene must name itself when the return route is invalid");
}

void test_escape_returns_the_full_caller_route()
{
    SdlFixture fixture;
    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(elysia::typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "Engine test scene tests must resolve default font settings");
    elysia::builtin::BuiltinAssetCache cache;
    require(cache.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(std::filesystem::path{ ELYSIA_SOURCE_DIR }),
                resolved_font_settings->engine_point_sizes())
                .has_value(),
        "Engine test scene tests must initialize built-in resources");

    elysia::typography::FontResolver font_resolver;
    const std::array<std::string,1> supported_languages{ "en" };
    require(font_resolver.configure(
                *resolved_font_settings,
                cache,
                *elysia::resources::ResourceService::instance(),
                supported_languages)
                .has_value(),
        "Engine test scene tests must configure floating-number fonts");
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        fixture.renderer(),&font_resolver);

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),registry,1280,720,&cache,&font_resolver);
    elysia::scene::SceneManager scene_manager;
    scene_manager.set_runtime_context(context);
    scene_manager.register_game_scene<example::scene::DemoGalleryScene>(
        example::scene_keys::DemoGallery);
    scene_manager.register_game_scene<example::scene::UiComponentGalleryScene>(
        example::scene_keys::UiComponentGallery);
    scene_manager.register_game_scene<example::scene::EngineFeatureLabScene>(
        example::scene_keys::EngineFeatureLab);
    scene_manager.register_game_scene<FirstReturnScene>(1);
    scene_manager.register_game_scene<SecondReturnScene>(2);
    scene_manager.register_engine_scene<
        elysia::builtin::ApplicationFailureScene>(
            elysia::builtin::SceneKeys::ApplicationFailure);

    scene_manager.start(elysia::scene::SceneRoute{
        .target = example::scene_keys::UiComponentGallery,
        .payload = example::scene::DemoScenePayload{
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
            .target = example::scene_keys::EngineFeatureLab,
            .payload = example::scene::DemoScenePayload{
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

    scene_manager.on_input(
        elysia::input::RawInputFrame{},
        {elysia::input::RawInputEvent{
            .control = elysia::input::RawInputControl::KeyD,
            .type = elysia::input::RawInputEventType::ControlPressed,
            .device = elysia::input::InputDevice::Keyboard
        }});
    scene_manager.on_update(0.25);
    const auto* moved_collider = std::get_if<elysia::tools::DebugDrawRect>(
        &debug_draw->commands().front().primitive);
    require(debug_draw->commands().size() == 1 && moved_collider
            && moved_collider->rect.x() > initial_collider_x,
        "Engine feature test must refresh one collider snapshot at the moved character position");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 2 && SecondReturnScene::marker == 29,
        "EngineFeatureLabScene Escape must return the caller key and payload");
    require(!debug_draw->enabled()
            && debug_draw->enabled_categories()
                == elysia::tools::DebugDrawCategory::Gameplay,
        "leaving EngineFeatureLabScene must restore the previous DebugDraw settings");

    const elysia::scene::SceneRoute original_caller{
        .target = 1,
        .payload = ReturnPayload{ .marker = 41 },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = example::scene_keys::DemoGallery,
            .payload = example::scene::DemoScenePayload{
                .return_route = original_caller
            }
        }
    });
    scene_manager.on_update(0.0);

    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = example::scene_keys::UiComponentGallery,
            .payload = example::scene::DemoScenePayload{
                .return_route = elysia::scene::SceneRoute{
                    .target = example::scene_keys::DemoGallery,
                    .payload = example::scene::DemoScenePayload{
                        .return_route = original_caller
                    },
                    .reload_mode = elysia::scene::SceneReloadMode::Reuse
                }
            }
        }
    });
    scene_manager.on_update(0.0);
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == example::scene_keys::DemoGallery,
        "Demo child Escape must return to DemoGalleryScene");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 41,
        "DemoGalleryScene must preserve and return the original caller route");

    const auto open_gallery = [&scene_manager,&original_caller]()
    {
        scene_manager.on_scene_request(elysia::scene::SceneRequest{
            .type = elysia::scene::SceneRequestType::Switch,
            .route = {
                .target = example::scene_keys::DemoGallery,
                .payload = example::scene::DemoScenePayload{
                    .return_route = original_caller},
                .reload_mode = elysia::scene::SceneReloadMode::Reuse}});
        scene_manager.on_update(0.0);
    };

    open_gallery();
    click_mouse(scene_manager, 640, 502);
    require(scene_manager.current_scene_key() == example::scene_keys::DemoGallery,
        "Failure Test must open a confirmation without immediately leaving Gallery");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == example::scene_keys::DemoGallery,
        "Canceling the Failure confirmation must remain in Gallery");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == 1,
        "Gallery Escape must resume returning to its caller after closing the modal");

    open_gallery();
    click_mouse(scene_manager, 640, 502);
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyRight);
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key()
            == elysia::builtin::SceneKeys::ApplicationFailure,
        "Confirming the guarded Failure Test must enter the engine failure scene");

    scene_manager.shutdown();
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        nullptr,nullptr);
    font_resolver.shutdown();
    cache.shutdown();
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
    test_engine_feature_overlay_cycle();
    test_payload_contract_names_each_scene();
    test_escape_returns_the_full_caller_route();
    test_runtime_demo_sources_do_not_retain_legacy_names();
    return EXIT_SUCCESS;
}
