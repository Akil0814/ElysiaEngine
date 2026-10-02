#pragma once

#include "engine/core/geometry/rect.h"
#include "engine/ui/layout/ui_layout_types.h"
#include <array>

namespace example::scene
{
// Screen-space geometry for the Gameplay HUD and scenario presentation. Keeping the
// calculation independent from UI objects makes the layout deterministic and
// directly testable at every logical presentation size.
struct GameplayLayout
{
    elysia::core::Rect viewport;
    elysia::core::Rect hud_panel;
    elysia::core::Rect title;
    elysia::core::Rect controls;
    elysia::core::Rect health;
    elysia::core::Rect stats;
    elysia::core::Rect status;
};
struct ScenarioLayout
{
    elysia::core::Rect viewport, title, purpose, expected, details, arena;
    std::array<elysia::core::Rect,5> actions;
};
[[nodiscard]] ScenarioLayout make_scenario_layout(float width, float height) noexcept;

[[nodiscard]] GameplayLayout make_gameplay_layout(
    float logical_width,
    float logical_height) noexcept;

// Converts an absolute screen-space rect into explicit top-left window layout
// metadata. UiWindow intentionally owns child placement and otherwise ignores
// the initial x/y stored on the child.
[[nodiscard]] elysia::ui::UiLayoutChildOptions showcase_layout_options(
    const elysia::core::Rect& rect) noexcept;
}
