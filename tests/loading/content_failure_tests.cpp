#include "engine/builtin/scenes/application_failure_presentation.h"
#include "engine/loading/content_load_failure.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>
#include <source_location>
#include <string>
#include <utility>

int main()
{
    using elysia::tests::require;
    using elysia::loading::ContentLoadError;
    constexpr std::array cases{
        std::pair{ ContentLoadError::Config,"CONTENT-CONFIG" },
        std::pair{ ContentLoadError::Manifest,"CONTENT-MANIFEST" },
        std::pair{ ContentLoadError::Plan,"CONTENT-PLAN" },
        std::pair{ ContentLoadError::Texture,"CONTENT-TEXTURE" },
        std::pair{ ContentLoadError::Atlas,"CONTENT-ATLAS" },
        std::pair{ ContentLoadError::Font,"CONTENT-FONT" },
        std::pair{ ContentLoadError::Audio,"CONTENT-AUDIO" },
        std::pair{ ContentLoadError::Animation,"CONTENT-ANIMATION" },
        std::pair{ ContentLoadError::Effect,"CONTENT-EFFECT" }
    };
    for (const auto& [stage,code] : cases)
    {
        const auto failure = elysia::loading::make_content_load_failure(stage,"failure");
        require(failure.error_code() == code,
            "each content stage must retain a stable external error code");
    }

    const std::source_location origin = std::source_location::current();
    const auto failure = elysia::loading::make_content_load_failure(
        ContentLoadError::Texture,"decoder rejected the image","ui.bad",
        "assets/textures/bad.png",origin);
    const auto route = elysia::builtin::make_application_failure_route(failure,"resource");
    const auto* payload = elysia::scene::try_scene_payload<
        elysia::builtin::ApplicationFailureScenePayload>(route.payload);
    require(payload && payload->diagnostic.origin.line() == origin.line()
            && payload->diagnostic.origin.file_name() == origin.file_name(),
        "failure wrapping must preserve the original source location");

    const auto release = elysia::builtin::build_application_failure_presentation(
        *payload,"Elysia.log",false);
    const auto debug = elysia::builtin::build_application_failure_presentation(
        *payload,"Elysia.log",true);
    require(release.diagnostic_details.empty()
            && release.copy_report.find("decoder rejected") == std::string::npos,
        "Release presentation must omit raw diagnostics");
    require(debug.diagnostic_details.find("ui.bad") != std::string::npos
            && debug.diagnostic_details.find("assets/textures/bad.png") != std::string::npos
            && debug.copy_report.find("decoder rejected") != std::string::npos,
        "Debug presentation must include resource, path and underlying cause");

    const auto unavailable = elysia::builtin::build_application_failure_presentation(
        *payload,{},false);
    require(!unavailable.log_available
            && unavailable.copy_report.find("unavailable") != std::string::npos,
        "missing active logs must use a stable unavailable fallback");
    return EXIT_SUCCESS;
}
