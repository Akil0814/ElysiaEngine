#define SDL_MAIN_HANDLED

#include "engine/application/composition/application_scene_composition.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/testbed/testbed_scene_keys.h"
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
    void reset() override {}

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
        scene_manager.register_game_scene<InitialScene>(1);
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
    scene_manager.set_runtime_context(context);

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
        "the Elysia module must no longer register the former game UiTest key");
    require(throws_logic_error_containing(
            [&scene_manager] { request_scene(scene_manager,elysia::testbed::SceneKeys::Home); },
            "TestbedHomeScene"),
        "Application composition must register the Testbed home scene");
    require(throws_logic_error_containing(
            [&scene_manager] { request_scene(scene_manager,elysia::testbed::SceneKeys::UiTest); },
            "UiTestScene"),
        "Application composition must register the Testbed UI scene");
    require(throws_logic_error_containing(
            [&scene_manager] { request_scene(scene_manager,elysia::testbed::SceneKeys::EngineFeatureTest); },
            "EngineFeatureTestScene"),
        "Application composition must register the Testbed Engine feature scene");
    require(throws_logic_error_containing(
            [&scene_manager] { request_scene(
                scene_manager,elysia::scene::SceneKeys::ElysiaEasterEgg); },
            "ElysiaScene"),
        "Application composition must register the Elysia Easter egg scene");

    scene_manager.shutdown();
    return EXIT_SUCCESS;
}
