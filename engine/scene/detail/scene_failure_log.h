#pragma once

#include "../scene_boundary_failure.h"
#include "../../tools/logger.h"
#include "../../core/render/render_failure.h"

#include <exception>
#include <string_view>

namespace elysia::scene::detail
{
inline bool is_render_backend_exception(const std::exception_ptr& exception) noexcept
{
    if (!exception) return false;
    try { std::rethrow_exception(exception); }
    catch (const elysia::core::RenderBackendError&) { return true; }
    catch (...) { return false; }
}

inline void log_scene_failure(const SceneBoundaryFailure& failure) noexcept
{
    elysia::tools::Logger::instance()->log_stream(elysia::tools::LogLevel::Error,"scene",
        [&](std::ostream& output) {
            output << elysia::core::format_failure_diagnostic(
                failure.diagnostic,"APPLICATION-FATAL","scene");
        },failure.diagnostic.origin);
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
