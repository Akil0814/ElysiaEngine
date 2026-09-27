#pragma once

#include "../scene_boundary_failure.h"
#include "../../tools/logger.h"

#include <exception>
#include <string_view>

namespace elysia::scene::detail
{
inline std::string_view boundary_name(SceneBoundary boundary) noexcept
{
    switch (boundary)
    {
    case SceneBoundary::Enter: return "Enter";
    case SceneBoundary::Exit: return "Exit";
    case SceneBoundary::Reset: return "Reset";
    case SceneBoundary::Attach: return "Attach";
    case SceneBoundary::Detach: return "Detach";
    case SceneBoundary::Input: return "Input";
    case SceneBoundary::Update: return "Update";
    case SceneBoundary::Render: return "Render";
    case SceneBoundary::ObjectRegistration: return "ObjectRegistration";
    case SceneBoundary::ObjectRemoval: return "ObjectRemoval";
    }
    return "Unknown";
}

inline void log_scene_failure(const SceneBoundaryFailure& failure) noexcept
{
    ELYSIA_LOG_ERROR("scene", "SceneKey " << failure.scene << " boundary "
        << boundary_name(failure.boundary) << ": " << failure.message);
}

// Called from a catch handler after the primary failure has already been saved.
inline void log_cleanup_exception(std::string_view stage) noexcept
{
    try
    {
        if (const auto error = std::current_exception())
            std::rethrow_exception(error);
    }
    catch (const std::exception& error)
    {
        ELYSIA_LOG_ERROR("scene_cleanup", stage << ": " << error.what());
    }
    catch (...)
    {
        ELYSIA_LOG_ERROR("scene_cleanup", stage << ": Unknown cleanup exception.");
    }
}
} // namespace elysia::scene::detail
