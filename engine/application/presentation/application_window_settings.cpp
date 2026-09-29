#include "application_window_settings.h"
#include <SDL3/SDL.h>

namespace elysia::application::detail
{
WindowOperationResult apply_window_settings(
    const elysia::config::WindowSettings& settings,const ApplicationWindowOperations& operations)
{
    if (settings.windowed_size.width <= 0 || settings.windowed_size.height <= 0)
        return std::unexpected(elysia::core::make_failure_diagnostic("Windowed size must be positive."));
    if (!operations.set_fullscreen || !operations.set_size || !operations.set_position)
        return std::unexpected(elysia::core::make_failure_diagnostic("Window operations are unavailable."));
    switch (settings.mode)
    {
    case elysia::config::WindowMode::Windowed:
        if (auto result = operations.set_fullscreen(false); !result) return result;
        if (auto result = operations.set_size(settings.windowed_size.width,settings.windowed_size.height); !result) return result;
        return operations.set_position(SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    case elysia::config::WindowMode::BorderlessFullscreen:
        return operations.set_fullscreen(true);
    }
    return std::unexpected(elysia::core::make_failure_diagnostic("Unknown window mode."));
}

WindowOperationResult apply_window_settings_transactional(
    const elysia::config::WindowSettings& settings,const ApplicationWindowSnapshot& previous,
    const ApplicationWindowOperations& operations)
{
    if (settings.windowed_size.width <= 0 || settings.windowed_size.height <= 0
        || (settings.mode != elysia::config::WindowMode::Windowed
            && settings.mode != elysia::config::WindowMode::BorderlessFullscreen))
        return apply_window_settings(settings,operations);
    auto result = apply_window_settings(settings,operations);
    if (result || !operations.set_fullscreen || !operations.set_size || !operations.set_position)
        return result;
    auto& diagnostic = result.error();
    const auto restore = [&](WindowOperationResult restored)
    {
        if (!restored)
        {
            auto& failure = restored.error();
            diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
                "window-rollback",{},{},{},{},failure.message,failure.origin));
            diagnostic.entries.insert(diagnostic.entries.end(),
                std::make_move_iterator(failure.entries.begin()),std::make_move_iterator(failure.entries.end()));
        }
    };
    // Force the physical rollback even though the committed configuration is unchanged.
    restore(operations.set_fullscreen(false));
    restore(operations.set_size(previous.settings.windowed_size.width,previous.settings.windowed_size.height));
    restore(operations.set_position(previous.x,previous.y));
    if (previous.settings.mode == elysia::config::WindowMode::BorderlessFullscreen)
        restore(operations.set_fullscreen(true));
    return result;
}
}
