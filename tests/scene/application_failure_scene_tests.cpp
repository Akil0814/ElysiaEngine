#define SDL_MAIN_HANDLED

#include "engine/io/loaders/asset_config_types.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/application_failure_scene.h"
#include "engine/scene/routing/scene_key.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/tools/termination_manager.h"
#include "engine/ui/composites/ui_confirmation_dialog.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::builtin
{
class ApplicationFailureSceneTestAccess
{
public:
    static elysia::ui::UiConfirmationDialogConfig dialog_config(
        ApplicationFailurePresentation presentation)
    {
        return ApplicationFailureScene::make_dialog_config(presentation);
    }

    static void apply_payload(
        ApplicationFailureScene& scene,
        const ApplicationFailureScenePayload& payload)
    {
        scene.apply_payload(payload);
    }

    static void confirm_exit(ApplicationFailureScene& scene)
    {
        scene.confirm_exit();
    }

    static std::string_view category(
        const ApplicationFailureScene& scene)
    {
        return scene._category;
    }

    static std::string_view diagnostic_message(
        const ApplicationFailureScene& scene)
    {
        return scene._diagnostic_message;
    }
};
}

namespace
{
using elysia::tests::require;
using elysia::builtin::ApplicationFailurePresentation;
using elysia::builtin::ApplicationFailureScene;
using elysia::builtin::ApplicationFailureScenePayload;
using elysia::builtin::ApplicationFailureSceneTestAccess;

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

void send_control(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control,
    elysia::input::RawInputEventType type,
    elysia::input::InputDevice device =
        elysia::input::InputDevice::Keyboard,
    int mouse_x = 0,
    int mouse_y = 0)
{
    scene_manager.on_input(
        elysia::input::RawInputFrame{},
        { elysia::input::RawInputEvent{
            .control = control,
            .type = type,
            .device = device,
            .mouse_x = mouse_x,
            .mouse_y = mouse_y
        } });
}

void activate_confirm(elysia::scene::SceneManager& scene_manager)
{
    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyRight,
        elysia::input::RawInputEventType::ControlPressed);
    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyEnter,
        elysia::input::RawInputEventType::ControlPressed);
    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyEnter,
        elysia::input::RawInputEventType::ControlReleased);
}

void click(
    elysia::scene::SceneManager& scene_manager,
    int mouse_x,
    int mouse_y)
{
    send_control(
        scene_manager,
        elysia::input::RawInputControl::MouseLeft,
        elysia::input::RawInputEventType::ControlPressed,
        elysia::input::InputDevice::Mouse,
        mouse_x,
        mouse_y);
    send_control(
        scene_manager,
        elysia::input::RawInputControl::MouseLeft,
        elysia::input::RawInputEventType::ControlReleased,
        elysia::input::InputDevice::Mouse,
        mouse_x,
        mouse_y);
}

void configure_scene_manager(
    elysia::scene::SceneManager& scene_manager,
    const elysia::scene::SceneRuntimeContext& context)
{
    scene_manager.set_runtime_context(context);
    scene_manager.register_engine_scene<ApplicationFailureScene>(
        elysia::builtin::SceneKeys::ApplicationFailure);
}

void test_scene_identity_and_route_contract()
{
    require(
        elysia::builtin::SceneKeys::ApplicationFailure == 0xFFFF0005u,
        "application failure must preserve the built-in scene key value");
    require(
        elysia::scene::SceneKeys::is_engine(
            elysia::builtin::SceneKeys::ApplicationFailure),
        "application failure key must belong to the built-in scene range");

    const auto route = elysia::builtin::make_application_failure_route(
        ApplicationFailurePresentation::RuntimeFatal,
        "resource",
        "missing atlas");
    const auto* payload =
        elysia::scene::try_scene_payload<ApplicationFailureScenePayload>(
            route.payload);
    require(
        route.target == elysia::builtin::SceneKeys::ApplicationFailure
            && route.reload_mode == elysia::scene::SceneReloadMode::Reuse
            && payload
            && payload->presentation
                == ApplicationFailurePresentation::RuntimeFatal
            && payload->category == "resource"
            && payload->diagnostic_message == "missing atlas",
        "application failure route helper must preserve the complete diagnostic");

    ApplicationFailureScene scene;
    require(
        throws_logic_error_containing(
            [&scene] { scene.on_enter(elysia::scene::ScenePayload{ 42 }); },
            "ApplicationFailureScene"),
        "wrong application failure payload type must identify the scene");
}

void test_dialog_uses_engine_presentation_contract()
{
    const auto startup = ApplicationFailureSceneTestAccess::dialog_config(
        ApplicationFailurePresentation::StartupLoading);
    require(
        startup.title.value
            == "engine.application_failure.startup.title"
            && startup.message.value
                == "engine.application_failure.startup.message"
            && startup.confirm.value
                == "engine.application_failure.startup.exit",
        "startup failures must retain the startup Engine presentation");

    const auto runtime = ApplicationFailureSceneTestAccess::dialog_config(
        ApplicationFailurePresentation::RuntimeFatal);
    require(
        runtime.title.value
            == "engine.application_failure.runtime.title"
            && runtime.message.value
                == "engine.application_failure.runtime.message"
            && runtime.confirm.value
                == "engine.application_failure.runtime.exit",
        "runtime failures must use the generic Engine presentation");
    require(
        runtime.cancel.value == "engine.common.cancel"
            && runtime.close.value == "engine.common.close_x"
            && runtime.confirm_visual_role
                == elysia::ui::UiButtonVisualRole::Danger,
        "application failure dismissal and exit controls must use Engine keys");
}

void test_payload_normalization_and_termination()
{
    auto* termination = elysia::tools::TerminationManager::instance();
    termination->reset_for_testing();

    ApplicationFailureScene scene;
    ApplicationFailureSceneTestAccess::apply_payload(
        scene,
        ApplicationFailureScenePayload{
            .presentation = ApplicationFailurePresentation::StartupLoading
        });
    require(
        ApplicationFailureSceneTestAccess::category(scene) == "application"
            && ApplicationFailureSceneTestAccess::diagnostic_message(scene)
                == "Startup resource loading failed.",
        "empty startup diagnostics must use the startup fallback");

    ApplicationFailureSceneTestAccess::apply_payload(
        scene,
        ApplicationFailureScenePayload{
            .presentation = ApplicationFailurePresentation::RuntimeFatal,
            .category = "physics",
            .diagnostic_message = "world state is invalid"
        });
    ApplicationFailureSceneTestAccess::confirm_exit(scene);

    const auto info = termination->termination_info();
    require(
        info
            && info->reason
                == elysia::tools::TerminationReason::FatalRuntimeFailure
            && info->category == "physics"
            && info->message == "world state is invalid",
        "confirming an application failure must publish its latest diagnostic");
    termination->reset_for_testing();

    ApplicationFailureSceneTestAccess::apply_payload(
        scene,
        ApplicationFailureScenePayload{
            .presentation = ApplicationFailurePresentation::RuntimeFatal
        });
    require(
        ApplicationFailureSceneTestAccess::diagnostic_message(scene)
            == "A fatal application failure occurred.",
        "empty runtime diagnostics must use the runtime fallback");
}

void test_cancel_key_reopens_the_dialog()
{
    auto* termination = elysia::tools::TerminationManager::instance();
    termination->reset_for_testing();

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        nullptr,registry,1280,720);
    elysia::scene::SceneManager scene_manager;
    configure_scene_manager(scene_manager,context);
    scene_manager.start(
        elysia::builtin::make_application_failure_route(
            ApplicationFailurePresentation::RuntimeFatal,
            "runtime",
            "cancel reopen probe"));

    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyEscape,
        elysia::input::RawInputEventType::ControlPressed);
    require(!termination->termination_requested(),
        "closing the failure dialog must not terminate the application");

    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyEscape,
        elysia::input::RawInputEventType::ControlPressed);
    activate_confirm(scene_manager);
    require(termination->termination_requested(),
        "Cancel on the closed failure window must reopen the dialog");

    scene_manager.shutdown();
    termination->reset_for_testing();
}

void test_persistent_button_reopens_the_dialog()
{
    auto* termination = elysia::tools::TerminationManager::instance();
    termination->reset_for_testing();

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        nullptr,registry,1280,720);
    elysia::scene::SceneManager scene_manager;
    configure_scene_manager(scene_manager,context);
    scene_manager.start(
        elysia::builtin::make_application_failure_route(
            ApplicationFailurePresentation::RuntimeFatal,
            "runtime",
            "button reopen probe"));

    send_control(
        scene_manager,
        elysia::input::RawInputControl::KeyEscape,
        elysia::input::RawInputEventType::ControlPressed);
    scene_manager.on_update(0.0);
    click(scene_manager,640,668);
    activate_confirm(scene_manager);
    require(termination->termination_requested(),
        "the persistent background button must reopen the failure dialog");

    scene_manager.shutdown();
    termination->reset_for_testing();
}

void test_legacy_startup_failure_contract_is_removed()
{
    const std::filesystem::path engine_root =
        std::filesystem::path(ELYSIA_SOURCE_DIR) / "engine";
    bool legacy_contract_found = false;
    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(engine_root))
    {
        if (!entry.is_regular_file())
            continue;
        const auto& path = entry.path();
        if (path.extension() != ".cpp" && path.extension() != ".h")
            continue;
        if (path.generic_string().find("/tests/") != std::string::npos)
            continue;

        std::ifstream stream(path,std::ios::binary);
        const std::string source{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        if (source.find("StartupFailureScene") != std::string::npos
            || source.find("builtin::StartupFailure") != std::string::npos
            || source.find("startup_failure_scene") != std::string::npos)
        {
            legacy_contract_found = true;
            break;
        }
    }
    require(!legacy_contract_found,
        "production sources must not retain the legacy StartupFailure contract");
}
}

int main()
{
    test_scene_identity_and_route_contract();
    test_dialog_uses_engine_presentation_contract();
    test_payload_normalization_and_termination();
    test_cancel_key_reopens_the_dialog();
    test_persistent_button_reopens_the_dialog();
    test_legacy_startup_failure_contract_is_removed();
    return EXIT_SUCCESS;
}
