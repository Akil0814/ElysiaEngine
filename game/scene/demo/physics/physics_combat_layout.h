#pragma once

#include "../../../../engine/core/geometry/rect.h"
#include "../../../../engine/ui/layout/ui_layout_types.h"

namespace example::scene
{
// Screen-space geometry shared by the physics combat HUD and gallery. Keeping the
// calculation independent from UI objects makes the layout deterministic and
// directly testable at every logical presentation size.
struct PhysicsCombatLayout
{
    elysia::core::Rect viewport;
    elysia::core::Rect hud_panel;
    elysia::core::Rect title;
    elysia::core::Rect controls;
    elysia::core::Rect health;
    elysia::core::Rect stats;
    elysia::core::Rect status;
    elysia::core::Rect menu_list;
};

[[nodiscard]] PhysicsCombatLayout make_physics_combat_layout(
    float logical_width,
    float logical_height) noexcept;

// Converts an absolute screen-space rect into explicit top-left window layout
// metadata. UiWindow intentionally owns child placement and otherwise ignores
// the initial x/y stored on the child.
[[nodiscard]] elysia::ui::UiLayoutChildOptions physics_combat_layout_options(
    const elysia::core::Rect& rect) noexcept;
}
