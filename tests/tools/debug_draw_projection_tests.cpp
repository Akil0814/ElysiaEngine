#define SDL_MAIN_HANDLED

#include "engine/core/render/debug_draw_projection.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <vector>

namespace
{
using elysia::tests::require;

void test_world_debug_projection()
{
    using namespace elysia;

    tools::DebugDraw* debug_draw = tools::DebugDraw::instance();
    debug_draw->clear();

    const core::Color red{255, 0, 0};
    const core::Color green{0, 255, 0};
    const core::Color blue{0, 0, 255};
    const core::Color white{};

    debug_draw->draw_rect(
        tools::DebugDrawCategory::PhysicsCollider,
        core::Rect{60.0f, 30.0f, 10.0f, 5.0f},
        red,
        2.0f
    );
    debug_draw->draw_circle(
        tools::DebugDrawCategory::PhysicsContact,
        core::Vector2{100.0f, 50.0f},
        4.0f,
        green,
        3.0f
    );
    debug_draw->draw_line(
        tools::DebugDrawCategory::Camera,
        core::Vector2{50.0f, 25.0f},
        core::Vector2{100.0f, 50.0f},
        blue,
        4.0f
    );
    debug_draw->draw_point(
        tools::DebugDrawCategory::Gameplay,
        core::Vector2{75.0f, 37.5f},
        6.0f,
        white
    );
    debug_draw->draw_rect(
        tools::DebugDrawCategory::PhysicsBroadPhase,
        core::Rect{0, 0, 1, 1},
        white
    );

    const camera::Camera camera(
        core::Vector2{100.0f, 50.0f},
        core::Vector2{200.0f, 100.0f},
        2.0f
    );
    std::vector<core::UiRenderCommand> projected;
    core::append_projected_debug_draw_commands(
        debug_draw->commands(),
        tools::DebugDrawCategory::PhysicsCollider
            | tools::DebugDrawCategory::PhysicsContact
            | tools::DebugDrawCategory::Camera
            | tools::DebugDrawCategory::Gameplay,
        camera,
        projected
    );

    require(projected.size() == 4,
        "projection must omit commands from hidden categories");

    require(projected[0].type == core::UiRenderCommandType::DrawRect
            && projected[0].screen_rect == core::Rect{20.0f, 10.0f, 20.0f, 10.0f}
            && projected[0].color == red
            && projected[0].stroke_width.mode == core::UiStrokeWidthMode::Logical
            && projected[0].stroke_width.logical_width == 2.0f,
        "rect projection must transform world bounds while preserving logical stroke width");

    require(projected[1].type == core::UiRenderCommandType::DrawCircle
            && projected[1].circle_center == core::Vector2{100.0f, 50.0f}
            && projected[1].circle_radius == 8.0f
            && projected[1].color == green
            && projected[1].stroke_width.logical_width == 3.0f,
        "circle projection must scale world radius with camera zoom");

    require(projected[2].type == core::UiRenderCommandType::DrawLine
            && projected[2].line_start == core::Vector2::zero()
            && projected[2].line_end == core::Vector2{100.0f, 50.0f}
            && projected[2].color == blue
            && projected[2].stroke_width.logical_width == 4.0f,
        "line projection must transform both endpoints without scaling stroke width");

    require(projected[3].type == core::UiRenderCommandType::FillCircle
            && projected[3].circle_center == core::Vector2{50.0f, 25.0f}
            && projected[3].circle_radius == 3.0f
            && projected[3].color == white,
        "point projection must keep its logical diameter independent of camera zoom");

    debug_draw->clear();
}
}

int main()
{
    test_world_debug_projection();
    return EXIT_SUCCESS;
}
