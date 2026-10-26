#define SDL_MAIN_HANDLED

#include "engine/application/composition/application_scene_composition.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/startup_loading_scene_payload.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>

namespace
{
using elysia::tests::require;

struct InitialPayload
{
    int marker = 0;
};

class InitialScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const InitialPayload* initial_payload =
            elysia::scene::try_scene_payload<InitialPayload>(payload);
        if (!initial_payload)
            throw std::logic_error(
                "InitialScene requires InitialPayload.");

        ++enter_count;
        received_marker = initial_payload->marker;
        received_width = runtime_context().logical_width();
        received_height = runtime_context().logical_height();
    }

    void on_exit() override {}
    void on_reset() override {}

    static inline int enter_count = 0;
    static inline int received_marker = 0;
    static inline int received_width = 0;
    static inline int received_height = 0;
};

class FakeGameModule final : public elysia::application::IGameModule
{
public:
    elysia::application::ApplicationDescriptor descriptor() const override
    {
        ++descriptor_calls;
        return elysia::application::ApplicationDescriptor{
            .logical_width = 960,
            .logical_height = 540,
            .initial_route = elysia::scene::SceneRoute{
                .target = 1,
                .payload = InitialPayload{ .marker = 73 },
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        };
    }

    void register_scenes(
        elysia::scene::SceneManager& scene_manager) const override
    {
        ++registration_calls;
        scene_manager.register_game_scene<InitialScene>(
            1);
    }

    mutable int descriptor_calls = 0;
    mutable int registration_calls = 0;
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

void request_scene(
    elysia::scene::SceneManager& scene_manager,
    elysia::scene::SceneKey target)
{
    scene_manager.on_scene_request(elysia::scene::SceneRequest{
        .type = elysia::scene::SceneRequestType::Switch,
        .route = elysia::scene::SceneRoute{ .target = target }
    });
    scene_manager.on_update(0.0);
}

void test_startup_logo_option_reaches_registered_scene()
{
    FakeGameModule module;
    auto descriptor = module.descriptor();
    // An invalid selection must reach the startup scene constructor instead of
    // silently falling back to its default. Valid selections are checked with
    // real textures in startup_loading_background_tests.
    descriptor.presentation.startup.engine_logo =
        static_cast<elysia::application::ApplicationEngineLogoVariant>(255);
    descriptor.initial_route = {
        .target = elysia::builtin::SceneKeys::StartupLoading,
        .payload = elysia::builtin::StartupLoadingScenePayload{
            .success_route = {.target = 1}
        }
    };
    const elysia::io::ContentRegistry registry{};
    const elysia::scene::SceneRuntimeContext context{nullptr,registry,960,540};
    elysia::scene::SceneManager manager;
    bool rejected_selected_logo = false;
    manager.initialize(context,[&](const elysia::scene::SceneBoundaryFailure& failure)
    {
        rejected_selected_logo =
            failure.scene == elysia::builtin::SceneKeys::StartupLoading
            && failure.diagnostic.message.find("Unsupported startup engine logo variant")
                != std::string::npos;
        return elysia::scene::SceneRoute{
            .target = 1,.payload = InitialPayload{.marker = 73}
        };
    });
    elysia::application::compose_application_scenes(manager,module,descriptor);
    require(rejected_selected_logo && manager.current_scene_key() == 1,
        "application composition must forward the selected engine logo into the startup scene builder");
    manager.shutdown();
}
}

int main()
{
    InitialScene::enter_count = 0;
    InitialScene::received_marker = 0;

    FakeGameModule game_module;
    const elysia::application::ApplicationDescriptor descriptor =
        elysia::application::describe_game_module(game_module);

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        nullptr,
        registry,
        descriptor.logical_width,
        descriptor.logical_height);
    elysia::scene::SceneManager scene_manager;
    scene_manager.initialize(context);

    elysia::application::compose_application_scenes(
        scene_manager,
        game_module,
        descriptor);

    require(game_module.descriptor_calls == 1,
        "Application composition must read the module descriptor exactly once");
    require(game_module.registration_calls == 1,
        "Application composition must ask the game module to register scenes exactly once");
    require(InitialScene::enter_count == 1
        && InitialScene::received_marker == 73,
        "the descriptor initial payload must reach the first project scene unchanged");
    require(InitialScene::received_width == 960
        && InitialScene::received_height == 540,
        "the module logical viewport must be available before the first scene enters");

    require(throws_logic_error_containing(
            [&scene_manager] { request_scene(scene_manager,99); },
            "unregistered game"),
        "the Elysia module must no longer register the former game UiComponentGallery key");
    for (const elysia::scene::SceneKey legacy_demo_key : {
            0xFFFF0003u,0xFFFF0004u,0xFFFF0006u })
    {
        require(throws_logic_error_containing(
                [&scene_manager,legacy_demo_key] {
                    request_scene(scene_manager,legacy_demo_key);
                },
                "unregistered engine-owned"),
            "Application composition must not register legacy development-demo keys");
    }
    request_scene(scene_manager,elysia::scene::SceneKeys::ElysiaRealm);
    require(scene_manager.state() == elysia::scene::SceneManagerState::Faulted,
        "Application composition must register the public Elysia Realm entry and route its invalid-payload enter failure through the SceneManager boundary");

    scene_manager.shutdown();
    test_startup_logo_option_reaches_registered_scene();
    return EXIT_SUCCESS;
}
