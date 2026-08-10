#pragma once

#include "../builtin_scene_keys.h"
#include "../../scene/routing/scene_route.h"

#include <string>
#include <utility>

namespace elysia::builtin
{
enum class ApplicationFailurePresentation
{
    StartupLoading,
    RuntimeFatal
};

struct ApplicationFailureScenePayload
{
    ApplicationFailurePresentation presentation =
        ApplicationFailurePresentation::RuntimeFatal;
    std::string category;
    std::string diagnostic_message;
};

[[nodiscard]] inline elysia::scene::SceneRoute make_application_failure_route(
    ApplicationFailurePresentation presentation,
    std::string category,
    std::string diagnostic_message)
{
    return elysia::scene::SceneRoute{
        .target = SceneKeys::ApplicationFailure,
        .payload = ApplicationFailureScenePayload{
            .presentation = presentation,
            .category = std::move(category),
            .diagnostic_message = std::move(diagnostic_message)
        },
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    };
}
}
