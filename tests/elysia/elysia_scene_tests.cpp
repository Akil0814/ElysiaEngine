#define SDL_MAIN_HANDLED

#include "engine/builtin/audio/builtin_audio_player.h"
#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/elysia/elysia_scene.h"
#include "engine/elysia/elysia_scene_payload.h"
#include "engine/elysia/realm_scene_composition.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
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
            "Realm scene tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "Realm scene tests must initialize PNG support");
        require(TTF_Init() == 0,
            "Realm scene tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
            "Realm scene tests must open SDL_mixer audio");
        _surface = SDL_CreateRGBSurfaceWithFormat(
            0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "Realm scene tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "Realm scene tests must create a software renderer");
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

class ReturnScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const auto* return_payload =
            elysia::scene::try_scene_payload<ReturnPayload>(payload);
        if (!return_payload)
            throw std::logic_error("ReturnScene requires ReturnPayload.");
        marker = return_payload->marker;
    }

    void on_exit() override {}
    void reset() override {}

    static inline int marker = 0;
};

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

void advance_full_sequence(
    elysia::scene::SceneManager& scene_manager,
    SDL_Renderer* renderer)
{
    for (int phase = 0; phase < 3; ++phase)
        scene_manager.on_update(1.5);
    scene_manager.on_render(renderer);
    for (int line = 0; line < 9; ++line)
        scene_manager.on_update(1.0);
    scene_manager.on_render(renderer);
    scene_manager.on_update(5.0);
    scene_manager.on_render(renderer);
}

void test_payload_contract()
{
    elysia::realm::ElysiaScene scene;
    require(throws_logic_error_containing(
            [&scene] { scene.on_enter({}); },
            "ElysiaScenePayload"),
        "ElysiaScene must require its Realm payload");

    const elysia::scene::ScenePayload invalid_payload =
        elysia::realm::ElysiaScenePayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
            [&scene,&invalid_payload] { scene.on_enter(invalid_payload); },
            "ElysiaScenePayload"),
        "ElysiaScene must reject an invalid return route");
}

void test_sequence_escape_reuse_and_audio_lifecycle()
{
    SdlFixture fixture;
    elysia::builtin::BuiltinAssetCache cache;
    require(cache.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(
                    std::filesystem::path{ ELYSIA_SOURCE_DIR }),
                std::array{10,20,30,40,50,60,70})
                .has_value(),
        "Realm scene tests must initialize built-in resources");

    elysia::builtin::BuiltinAudioPlayer audio_player;
    audio_player.bind(cache,elysia::audio::AudioSettings{});
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),registry,1280,720,&cache,nullptr,&audio_player);
    elysia::scene::SceneManager scene_manager;
    scene_manager.set_runtime_context(context);
    elysia::realm::register_realm_scene(scene_manager);
    scene_manager.register_game_scene<ReturnScene>(1);

    const auto enter_realm = [&scene_manager](int marker) {
        scene_manager.on_scene_request(elysia::scene::SceneRequest{
            .type = elysia::scene::SceneRequestType::Switch,
            .route = elysia::scene::SceneRoute{
                .target = elysia::scene::SceneKeys::ElysiaEasterEgg,
                .payload = elysia::realm::ElysiaScenePayload{
                    .return_route = elysia::scene::SceneRoute{
                        .target = 1,
                        .payload = ReturnPayload{ .marker = marker },
                        .reload_mode = elysia::scene::SceneReloadMode::Reuse
                    }
                },
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        });
        scene_manager.on_update(0.0);
    };

    scene_manager.start(elysia::scene::SceneRoute{
        .target = 1,
        .payload = ReturnPayload{ .marker = 0 }
    });

    enter_realm(33);
    require(Mix_PlayingMusic() != 0,
        "ElysiaScene must start Realm music on entry");
    advance_full_sequence(scene_manager,fixture.renderer());
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && ReturnScene::marker == 33,
        "ElysiaScene Escape must preserve the full caller route");
    require(Mix_PlayingMusic() == 0,
        "ElysiaScene must stop Realm music on exit");

    enter_realm(34);
    advance_full_sequence(scene_manager,fixture.renderer());
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && ReturnScene::marker == 34,
        "ElysiaScene Reuse must replay and use the updated caller route");
    require(Mix_PlayingMusic() == 0,
        "ElysiaScene Reuse exit must leave Realm music stopped");

    scene_manager.shutdown();
    audio_player.unbind();
    cache.shutdown();
}
}

int main()
{
    test_payload_contract();
    test_sequence_escape_reuse_and_audio_lifecycle();
    return EXIT_SUCCESS;
}
