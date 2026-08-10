#include "engine/tools/debug_draw.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <limits>
#include <variant>

namespace
{
using elysia::tests::require;
using namespace elysia;

void reset_debug_draw()
{
    tools::DebugDraw* debug_draw = tools::DebugDraw::instance();
    debug_draw->clear();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(tools::DebugDrawCategory::All);
}

void test_global_and_category_visibility()
{
    using tools::DebugDrawCategory;

    tools::DebugDraw* debug_draw = tools::DebugDraw::instance();
    reset_debug_draw();

    require(!debug_draw->enabled(), "DebugDraw must default to disabled");
    require(debug_draw->enabled_categories() == DebugDrawCategory::All,
        "DebugDraw must default to every category selected");
    require(!debug_draw->is_enabled(DebugDrawCategory::PhysicsCollider),
        "the global switch must gate category visibility");

    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(
        DebugDrawCategory::PhysicsCollider
        | DebugDrawCategory::PhysicsContact
    );

    require(debug_draw->is_enabled(DebugDrawCategory::PhysicsCollider),
        "selected categories must be visible while globally enabled");
    require(debug_draw->is_enabled(
            DebugDrawCategory::PhysicsCollider
            | DebugDrawCategory::PhysicsContact),
        "combined category queries must require and accept every selected bit");
    require(!debug_draw->is_enabled(DebugDrawCategory::Gameplay),
        "unselected categories must remain hidden");
    require(!debug_draw->is_enabled(DebugDrawCategory::None),
        "the empty category mask must never be visible");

    debug_draw->set_enabled_categories(static_cast<DebugDrawCategory>(0xffffffffu));
    require(debug_draw->enabled_categories() == DebugDrawCategory::All,
        "unknown category bits must be removed from the enabled mask");
}

void test_primitive_submission_and_snapshot_retention()
{
    using tools::DebugDrawCategory;

    tools::DebugDraw* debug_draw = tools::DebugDraw::instance();
    reset_debug_draw();

    const core::Color red{255, 0, 0};
    const core::Color green{0, 255, 0};
    const core::Color blue{0, 0, 255};
    const core::Color white{};

    debug_draw->draw_rect(
        DebugDrawCategory::PhysicsCollider,
        core::Rect{1.0f, 2.0f, 3.0f, 4.0f},
        red,
        2.0f
    );
    debug_draw->draw_circle(
        DebugDrawCategory::PhysicsContact,
        core::Vector2{5.0f, 6.0f},
        7.0f,
        green,
        3.0f
    );
    debug_draw->draw_line(
        DebugDrawCategory::Camera,
        core::Vector2{8.0f, 9.0f},
        core::Vector2{10.0f, 11.0f},
        blue,
        4.0f
    );
    debug_draw->draw_point(
        DebugDrawCategory::Gameplay,
        core::Vector2{12.0f, 13.0f},
        6.0f,
        white
    );

    const std::span<const tools::DebugDrawCommand> commands = debug_draw->commands();
    require(commands.size() == 4,
        "all valid primitives must be retained even while DebugDraw is disabled");
    require(std::holds_alternative<tools::DebugDrawRect>(commands[0].primitive)
            && commands[0].category == DebugDrawCategory::PhysicsCollider
            && commands[0].color == red
            && commands[0].thickness == 2.0f,
        "rect commands must preserve category, shape, color and thickness");
    require(std::holds_alternative<tools::DebugDrawCircle>(commands[1].primitive)
            && commands[1].thickness == 3.0f,
        "circle commands must preserve their shape and thickness");
    require(std::holds_alternative<tools::DebugDrawLine>(commands[2].primitive)
            && commands[2].thickness == 4.0f,
        "line commands must preserve their shape and thickness");

    const auto* point = std::get_if<tools::DebugDrawPoint>(&commands[3].primitive);
    require(point && point->size == 6.0f && commands[3].thickness == 1.0f,
        "point commands must preserve their screen-space diameter");

    debug_draw->set_enabled(true);
    require(debug_draw->commands().size() == 4,
        "toggling visibility must not discard retained category snapshots");
}

void test_invalid_input_and_category_clearing()
{
    using tools::DebugDrawCategory;

    tools::DebugDraw* debug_draw = tools::DebugDraw::instance();
    reset_debug_draw();

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const core::Color color{};
    const DebugDrawCategory combined =
        DebugDrawCategory::PhysicsCollider | DebugDrawCategory::PhysicsContact;

    debug_draw->draw_rect(DebugDrawCategory::None, core::Rect{0, 0, 1, 1}, color);
    debug_draw->draw_rect(combined, core::Rect{0, 0, 1, 1}, color);
    debug_draw->draw_rect(DebugDrawCategory::PhysicsCollider, core::Rect{}, color);
    debug_draw->draw_rect(
        DebugDrawCategory::PhysicsCollider,
        core::Rect{nan, 0.0f, 1.0f, 1.0f},
        color
    );
    debug_draw->draw_circle(
        DebugDrawCategory::PhysicsContact,
        core::Vector2{},
        0.0f,
        color
    );
    debug_draw->draw_line(
        DebugDrawCategory::Camera,
        core::Vector2{1.0f, 1.0f},
        core::Vector2{1.0f, 1.0f},
        color
    );
    debug_draw->draw_point(
        DebugDrawCategory::Gameplay,
        core::Vector2{},
        -1.0f,
        color
    );
    require(debug_draw->commands().empty(),
        "invalid categories and degenerate geometry must be rejected");

    debug_draw->draw_rect(
        DebugDrawCategory::PhysicsCollider,
        core::Rect{0, 0, 10, 10},
        color
    );
    debug_draw->draw_circle(
        DebugDrawCategory::PhysicsContact,
        core::Vector2{},
        2.0f,
        color
    );
    debug_draw->draw_line(
        DebugDrawCategory::Camera,
        core::Vector2{},
        core::Vector2{1.0f, 1.0f},
        color
    );

    debug_draw->clear_categories(
        DebugDrawCategory::PhysicsCollider
        | DebugDrawCategory::PhysicsContact
    );
    require(debug_draw->commands().size() == 1
            && debug_draw->commands().front().category == DebugDrawCategory::Camera,
        "category clearing must remove every command matching any requested bit");

    debug_draw->clear();
    require(debug_draw->commands().empty(),
        "global clearing must remove every retained command");
}
}

int main()
{
    test_global_and_category_visibility();
    test_primitive_submission_and_snapshot_retention();
    test_invalid_input_and_category_clearing();
    reset_debug_draw();
    return EXIT_SUCCESS;
}
