#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/audio/builtin_audio_player.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/testbed/scene/engine_feature_test_scene.h"
#include "engine/testbed/scene/testbed_home_scene.h"
#include "engine/testbed/scene/testbed_scene_payload.h"
#include "engine/testbed/scene/ui_test_scene.h"
#include "engine/testbed/testbed_scene_keys.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
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

void test_engine_feature_overlay_cycle()
{
    elysia::testbed::EngineFeatureTestScene scene;
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
    elysia::testbed::TestbedHomeScene home_scene;
    require(throws_logic_error_containing(
            [&home_scene] { home_scene.on_enter({}); },
            "TestbedHomeScene"),
        "TestbedHomeScene must name itself when the Testbed payload is missing");

    elysia::testbed::UiTestScene ui_test_scene;
    require(throws_logic_error_containing(
            [&ui_test_scene] { ui_test_scene.on_enter({}); },
            "UiTestScene"),
        "UiTestScene must name itself when the Testbed payload is missing");

    elysia::testbed::EngineFeatureTestScene feature_test_scene;
    const elysia::scene::ScenePayload invalid_payload =
        elysia::testbed::TestbedScenePayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
            [&feature_test_scene,&invalid_payload] { feature_test_scene.on_enter(invalid_payload); },
            "EngineFeatureTestScene"),
        "EngineFeatureTestScene must name itself when the return route is invalid");
}

void test_escape_returns_the_full_caller_route()
{
    SdlFixture fixture;
    elysia::builtin::BuiltinAssetCache cache;
    require(cache.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(std::filesystem::path{ ELYSIA_SOURCE_DIR }),
                std::array{10,20,30,40,50,60,70})
                .has_value(),
        "Engine test scene tests must initialize built-in resources");

    elysia::io::ContentRegistry registry;
    elysia::builtin::BuiltinAudioPlayer audio_player;
    audio_player.bind(cache,elysia::audio::AudioSettings{});
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),registry,1280,720,&cache,nullptr,&audio_player);
    require(context.builtin_audio_player() == &audio_player,
        "Testbed runtime context must expose its built-in audio player");
    elysia::scene::SceneManager scene_manager;
    scene_manager.set_runtime_context(context);
    scene_manager.register_engine_scene<elysia::testbed::TestbedHomeScene>(
        elysia::testbed::SceneKeys::Home);
    scene_manager.register_engine_scene<elysia::testbed::UiTestScene>(
        elysia::testbed::SceneKeys::UiTest);
    scene_manager.register_engine_scene<elysia::testbed::EngineFeatureTestScene>(
        elysia::testbed::SceneKeys::EngineFeatureTest);
    scene_manager.register_game_scene<FirstReturnScene>(1);
    scene_manager.register_game_scene<SecondReturnScene>(2);

    scene_manager.start(elysia::scene::SceneRoute{
        .target = elysia::testbed::SceneKeys::UiTest,
        .payload = elysia::testbed::TestbedScenePayload{
            .return_route = elysia::scene::SceneRoute{
                .target = 1,
                .payload = ReturnPayload{ .marker = 17 },
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        }
    });
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 17,
        "UiTestScene Escape must return the caller key and payload");

    auto* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->clear();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(
        elysia::tools::DebugDrawCategory::Gameplay);

    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = elysia::testbed::SceneKeys::EngineFeatureTest,
            .payload = elysia::testbed::TestbedScenePayload{
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
        "EngineFeatureTestScene Escape must return the caller key and payload");
    require(!debug_draw->enabled()
            && debug_draw->enabled_categories()
                == elysia::tools::DebugDrawCategory::Gameplay,
        "leaving EngineFeatureTestScene must restore the previous DebugDraw settings");

    const elysia::scene::SceneRoute original_caller{
        .target = 1,
        .payload = ReturnPayload{ .marker = 41 },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = elysia::testbed::SceneKeys::Home,
            .payload = elysia::testbed::TestbedScenePayload{
                .return_route = original_caller
            }
        }
    });
    scene_manager.on_update(0.0);

    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{
            .target = elysia::testbed::SceneKeys::UiTest,
            .payload = elysia::testbed::TestbedScenePayload{
                .return_route = elysia::scene::SceneRoute{
                    .target = elysia::testbed::SceneKeys::Home,
                    .payload = elysia::testbed::TestbedScenePayload{
                        .return_route = original_caller
                    },
                    .reload_mode = elysia::scene::SceneReloadMode::Reuse
                }
            }
        }
    });
    scene_manager.on_update(0.0);
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == elysia::testbed::SceneKeys::Home,
        "Testbed child Escape must return to TestbedHomeScene");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && FirstReturnScene::marker == 41,
        "TestbedHomeScene must preserve and return the original caller route");

    scene_manager.shutdown();
    audio_player.unbind();
    cache.shutdown();
}
}

int main()
{
    test_engine_feature_overlay_cycle();
    test_payload_contract_names_each_scene();
    test_escape_returns_the_full_caller_route();
    return EXIT_SUCCESS;
}
