#define SDL_MAIN_HANDLED

#include "engine/builtin/scenes/startup_loading_scene.h"
#include "engine/builtin/scenes/application_failure_scene_payload.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/scene/routing/scene_request_observer.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/text/ui_text_content.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>

namespace elysia::builtin
{
class StartupLoadingSceneTestAccess
{
public:
    static void prime(
        StartupLoadingScene& scene,
        const StartupLoadingScenePayload& payload)
    {
        scene.clear_state();
        scene._startup_payload = payload;
        scene._completion.reset(payload.wait_for_confirmation);
    }

    static void finish_loading_then_intro(StartupLoadingScene& scene)
    {
        scene.handle_completion_action(
            scene._completion.mark_loading_finished());
        scene.mark_intro_finished();
    }

    static void fail(StartupLoadingScene& scene,std::string_view message)
    {
        scene.handle_failure(message);
    }

    static bool activate_fonts(
        StartupLoadingScene& scene,
        elysia::typography::FontResolver* font_resolver)
    {
        return scene.activate_project_fonts(font_resolver);
    }

    static elysia::ui::UiTextContent start_prompt_content()
    {
        return StartupLoadingScene::make_start_prompt_content();
    }
};
}

namespace
{
using elysia::tests::require;

struct RoutePayload
{
    int marker = 0;
};

class RequestProbe final : public elysia::scene::SceneRequestObserver
{
public:
    void on_scene_request(const elysia::scene::SceneRequest& value) override
    {
        request = value;
        ++request_count;
    }

    elysia::scene::SceneRequest request;
    int request_count = 0;
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

void test_payload_contract_names_startup_scene()
{
    elysia::builtin::BuiltinResources resources;
    elysia::builtin::StartupLoadingScene scene(resources);
    require(throws_logic_error_containing(
        [&scene]
        {
            scene.on_enter(elysia::scene::ScenePayload{ 42 });
        },
        "StartupLoadingScene"),
        "wrong startup payload type must fail with the built-in scene name");

    const elysia::scene::ScenePayload invalid_route =
        elysia::builtin::StartupLoadingScenePayload{
            .success_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
        [&scene,&invalid_route] { scene.on_enter(invalid_route); },
        "StartupLoadingScene"),
        "invalid startup success route must fail with the built-in scene name");
}

void test_success_and_failure_routes_are_forwarded_unchanged()
{
    using namespace elysia::scene;
    using namespace elysia::builtin;

    BuiltinResources resources;
    StartupLoadingScene success_scene(resources);
    RequestProbe success_probe;
    success_scene.attach(&success_probe);
    StartupLoadingSceneTestAccess::prime(
        success_scene,
        StartupLoadingScenePayload{
            .success_route = SceneRoute{
                .target = 7,
                .payload = RoutePayload{ .marker = 71 },
                .reload_mode = SceneReloadMode::Reset
            },
            .wait_for_confirmation = false
        });
    StartupLoadingSceneTestAccess::finish_loading_then_intro(success_scene);
    const RoutePayload* success_payload =
        try_scene_payload<RoutePayload>(success_probe.request.route.payload);
    require(success_probe.request_count == 1
        && success_probe.request.type == SceneRequestType::Switch
        && success_probe.request.route.target == 7
        && success_probe.request.route.reload_mode == SceneReloadMode::Reset
        && success_payload && success_payload->marker == 71,
        "startup success must forward the complete success route");
    success_scene.detach(&success_probe);

    StartupLoadingScene failure_scene(resources);
    RequestProbe failure_probe;
    failure_scene.attach(&failure_probe);
    StartupLoadingSceneTestAccess::prime(
        failure_scene,
        StartupLoadingScenePayload{
            .success_route = SceneRoute{ .target = 1 },
            .failure_route = SceneRoute{
                .target = 8,
                .payload = RoutePayload{ .marker = 82 },
                .reload_mode = SceneReloadMode::Recreate
            }
        });
    StartupLoadingSceneTestAccess::fail(
        failure_scene,
        "injected content failure");
    const RoutePayload* failure_payload =
        try_scene_payload<RoutePayload>(failure_probe.request.route.payload);
    require(failure_probe.request_count == 1
        && failure_probe.request.route.target == 8
        && failure_probe.request.route.reload_mode == SceneReloadMode::Recreate
        && failure_payload && failure_payload->marker == 82,
        "startup failure must forward the complete optional failure route");
    failure_scene.detach(&failure_probe);
}

void test_failure_without_route_uses_builtin_failure_scene()
{
    using namespace elysia::scene;
    using namespace elysia::builtin;

    BuiltinResources resources;
    StartupLoadingScene scene(resources);
    RequestProbe probe;
    scene.attach(&probe);
    StartupLoadingSceneTestAccess::prime(
        scene,
        StartupLoadingScenePayload{
            .success_route = SceneRoute{ .target = 1 }
        });
    StartupLoadingSceneTestAccess::fail(scene,"fatal startup failure");

    const auto* payload =
        try_scene_payload<ApplicationFailureScenePayload>(
            probe.request.route.payload);
    require(
        probe.request_count == 1
            && probe.request.type == SceneRequestType::Switch
            && probe.request.route.target
                == elysia::builtin::SceneKeys::ApplicationFailure
            && probe.request.route.reload_mode == SceneReloadMode::Reuse
            && payload
            && payload->presentation
                == ApplicationFailurePresentation::StartupLoading
            && payload->category == "startup"
            && payload->error_code == "STARTUP-PRESENTATION"
            && payload->diagnostic.message == "fatal startup failure",
        "startup failure without a custom route must use the built-in application failure scene");
    scene.detach(&probe);
}

void test_start_prompt_uses_scene_localization_key()
{
    const auto content =
        elysia::builtin::StartupLoadingSceneTestAccess::start_prompt_content();
    require(
        content.kind == elysia::ui::UiTextContentKind::TextKey
            && content.value
                == "engine.startup_loading.press_any_button",
        "startup prompt must use the StartupLoading scene localization key");
}

void test_font_activation_failure_uses_configured_failure_route()
{
    using namespace elysia::scene;
    using namespace elysia::builtin;

    BuiltinResources resources;
    StartupLoadingScene scene(resources);
    RequestProbe probe;
    scene.attach(&probe);
    StartupLoadingSceneTestAccess::prime(
        scene,
        StartupLoadingScenePayload{
            .success_route = SceneRoute{ .target = 1 },
            .failure_route = SceneRoute{ .target = 9 }
        });

    elysia::typography::FontResolver unconfigured_resolver;
    require(!StartupLoadingSceneTestAccess::activate_fonts(
            scene,
            &unconfigured_resolver),
        "startup loading must reject an unconfigured FontResolver");
    require(probe.request_count == 1
        && probe.request.route.target == 9,
        "font activation failure must use the configured startup failure route");
    scene.detach(&probe);
}
}

int main()
{
    test_payload_contract_names_startup_scene();
    test_start_prompt_uses_scene_localization_key();
    test_success_and_failure_routes_are_forwarded_unchanged();
    test_failure_without_route_uses_builtin_failure_scene();
    test_font_activation_failure_uses_configured_failure_route();
    return EXIT_SUCCESS;
}
