#pragma once

#include "../../config/user_config_data.h"
#include "../../core/diagnostics/failure_diagnostic.h"
#include <expected>
#include <functional>

namespace elysia::application::detail
{
using WindowOperationResult = std::expected<void,elysia::core::FailureDiagnostic>;
struct ApplicationWindowOperations
{
    std::function<WindowOperationResult(bool)> set_fullscreen;
    std::function<WindowOperationResult(int,int)> set_size;
    std::function<WindowOperationResult(int,int)> set_position;
};
struct ApplicationWindowSnapshot
{
    elysia::config::WindowSettings settings;
    int x = 0;
    int y = 0;
};
[[nodiscard]] WindowOperationResult apply_window_settings(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowOperations& operations);
[[nodiscard]] WindowOperationResult apply_window_settings_transactional(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowSnapshot& previous,
    const ApplicationWindowOperations& operations);
}
