#include "tests/support/input_snapshot_builder.h"
#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/elysia/elysia_realm.h"
#include "engine/elysia/detail/elysia_intro_scene.h"
#include "engine/elysia/detail/elysia_realm_scene.h"
#include "engine/elysia/detail/realm_content_payload.h"
#include "engine/elysia/detail/realm_scene_composition.h"
#include "engine/elysia/detail/realm_scene_keys.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/test_assertions.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

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
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
            "Realm scene tests must initialize SDL video and audio");
        require(TTF_Init(),
            "Realm scene tests must initialize SDL_ttf");
        require(elysia::tests::open_test_mixer(),
            "Realm scene tests must open SDL_mixer audio");
        _surface = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "Realm scene tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "Realm scene tests must create a software renderer");
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
    scene_manager.on_input(elysia::tests::events_snapshot({elysia::input::RawInputEvent{.control = elysia::input::RawInputControl::KeyEscape,
                                      .type = elysia::input::RawInputEventType::ControlPressed,
                                      .device = elysia::input::InputDevice::Keyboard}}));
}

void advance_intro_normally(
    elysia::scene::SceneManager& scene_manager,
    SDL_Renderer* renderer)
{
    for (int phase = 0; phase < 3; ++phase)
        scene_manager.on_update(1.5);
    scene_manager.on_render(renderer);
}

void test_payload_contract()
{
    static_assert(elysia::scene::SceneKeys::ElysiaRealm == 1111);
    static_assert(
        elysia::realm::detail::SceneKeys::RealmContent == 0xFFFF0007u);
    static_assert(elysia::scene::SceneKeys::is_engine(
        elysia::realm::detail::SceneKeys::RealmContent));

    elysia::realm::detail::ElysiaIntroScene scene;
    require(throws_logic_error_containing(
            [&scene] { scene.on_enter({}); },
            "ElysiaRealmPayload"),
        "ElysiaIntroScene must require the public Realm payload");

    const elysia::scene::ScenePayload invalid_payload =
        elysia::realm::ElysiaRealmPayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
            [&scene,&invalid_payload] { scene.on_enter(invalid_payload); },
            "ElysiaRealmPayload"),
        "ElysiaIntroScene must reject an invalid return route");

    elysia::realm::detail::ElysiaRealmScene realm_scene;
    require(throws_logic_error_containing(
            [&realm_scene] { realm_scene.on_enter({}); },
            "RealmContentPayload"),
        "ElysiaRealmScene must require its internal payload");
}

void test_sequence_escape_reuse_and_audio_lifecycle()
{
    SdlFixture fixture;
    auto& builtin_resources = *elysia::builtin::BuiltinResources::instance();
    require(builtin_resources.initialize(
                fixture.renderer(),
                elysia::builtin::BuiltinAssetCatalog(
                    std::filesystem::path{ ELYSIA_SOURCE_DIR }),
                std::array{10,20,30,40,50,60,70},
                {})
                .has_value(),
        "Realm scene tests must initialize built-in resources");

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),registry,1280,720);
    elysia::scene::SceneManager scene_manager;
    scene_manager.set_runtime_context(context);
    elysia::realm::detail::register_realm_scenes(
        scene_manager);
    scene_manager.register_game_scene<ReturnScene>(1);

    const auto enter_intro = [&scene_manager](int marker) {
        const elysia::scene::SceneRoute return_route{
            .target = 1,
            .payload = ReturnPayload{.marker = marker},
            .reload_mode = elysia::scene::SceneReloadMode::Reuse};

        scene_manager.on_scene_request(elysia::scene::SceneRequest{
            .type = elysia::scene::SceneRequestType::Switch,
            .route = elysia::scene::SceneRoute{
                .target = elysia::scene::SceneKeys::ElysiaRealm,
                .payload = elysia::realm::ElysiaRealmPayload{
                    .return_route = return_route},
                .reload_mode = elysia::scene::SceneReloadMode::Reuse}
        });
        scene_manager.on_update(0.0);
    };

    scene_manager.start(elysia::scene::SceneRoute{
        .target = 1,
        .payload = ReturnPayload{ .marker = 0 }
    });

    enter_intro(33);
    require(elysia::tests::music_playing() != 0,
        "ElysiaIntroScene must start Realm music on entry");

    send_escape(scene_manager);
    require(scene_manager.current_scene_key()
            == elysia::scene::SceneKeys::ElysiaRealm,
        "ElysiaIntroScene must ignore Escape");
    require(elysia::tests::music_playing() != 0,
        "ignored Intro input must not stop Realm music");

    scene_manager.on_update(4.5);
    require(scene_manager.current_scene_key()
            == elysia::scene::SceneKeys::ElysiaRealm,
        "completed code playback must wait for the logo playback");
    scene_manager.on_update(1.5);
    require(scene_manager.current_scene_key()
            == elysia::scene::SceneKeys::ElysiaRealm,
        "Intro must remain active while the logo hold phase is incomplete");
    scene_manager.on_update(1.5);
    require(scene_manager.current_scene_key()
            == elysia::realm::detail::SceneKeys::RealmContent,
        "Intro must enter Realm exactly after both playbacks complete");
    scene_manager.on_render(fixture.renderer());
    require(elysia::tests::music_playing() != 0,
        "Intro must hand Realm music off without stopping it");

    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && ReturnScene::marker == 33,
        "ElysiaRealmScene Escape must preserve the full caller route");
    require(elysia::tests::music_playing() == 0,
        "ElysiaRealmScene must stop Realm music on exit");

    enter_intro(34);
    advance_intro_normally(scene_manager,fixture.renderer());
    require(scene_manager.current_scene_key()
            == elysia::realm::detail::SceneKeys::RealmContent,
        "the reused Realm entry must replay the complete Intro");
    send_escape(scene_manager);
    require(scene_manager.current_scene_key() == 1 && ReturnScene::marker == 34,
        "Realm Reuse must use the updated caller route");
    require(elysia::tests::music_playing() == 0,
        "Realm Reuse exit must leave Realm music stopped");

    enter_intro(35);
    require(elysia::tests::music_playing() != 0,
        "re-entered Intro must restart Realm music");
    scene_manager.shutdown();
    require(elysia::tests::music_playing() == 0,
        "Intro shutdown before handoff must stop Realm music");
    builtin_resources.shutdown();
}
}

int main()
{
    test_payload_contract();
    test_sequence_escape_reuse_and_audio_lifecycle();
    return EXIT_SUCCESS;
}
