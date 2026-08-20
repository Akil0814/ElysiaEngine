#include "engine/camera/camera.h"
#include "tests/support/test_assertions.h"
#define SDL_MAIN_HANDLED
#include "engine/core/render/render_command_projection.h"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
using elysia::tests::require;
using elysia::camera::Camera;
using elysia::core::Rect;
using elysia::core::Vector2;

void test_zoomed_projection_and_inverse()
{
    const Camera camera(
        Vector2(100.0f, 50.0f),
        Vector2(200.0f, 100.0f),
        2.0f
    );

    require(camera.world_viewport_size() == Vector2(100.0f, 50.0f),
        "2x zoom must halve the visible world size");
    require(camera.view_rect() == Rect(50.0f, 25.0f, 100.0f, 50.0f),
        "view rect must use the zoomed world viewport");
    require(camera.world_to_screen(camera.center()) == Vector2(100.0f, 50.0f),
        "camera center must project to viewport center");

    const Rect world_rect(60.0f, 30.0f, 10.0f, 5.0f);
    const Rect screen_rect = camera.world_to_screen(world_rect);
    require(screen_rect == Rect(20.0f, 10.0f, 20.0f, 10.0f),
        "world rect position and size must both scale with zoom");
    require(camera.screen_to_world(screen_rect).nearly_equals(world_rect),
        "rect projection and inverse projection must round trip");

    const Vector2 world_point(125.0f, 62.5f);
    require(
        camera.screen_to_world(camera.world_to_screen(world_point))
            .nearly_equals(world_point),
        "point projection and inverse projection must round trip"
    );
}

void test_zoom_levels_and_zero_viewport()
{
    Camera camera(Vector2(10.0f, 20.0f), Vector2(200.0f, 100.0f), 0.5f);
    require(camera.world_viewport_size() == Vector2(400.0f, 200.0f),
        "0.5x zoom must double the visible world size");
    require(camera.world_to_screen(camera.center()) == Vector2(100.0f, 50.0f),
        "camera center mapping must be stable at 0.5x");

    camera.set_zoom(1.0f);
    require(camera.view_rect() == Rect(-90.0f, -30.0f, 200.0f, 100.0f),
        "1x zoom must preserve the legacy view rect");

    camera.set_viewport_size(Vector2::zero());
    require(camera.world_viewport_size() == Vector2::zero(),
        "zero viewport must remain zero under zoom");
    require(camera.world_to_screen(camera.center()) == Vector2::zero(),
        "zero viewport must map camera center to the local origin");
}

void test_zoom_validation()
{
    Camera camera;

    camera.set_zoom(0.0f);
    require(camera.zoom() == Camera::k_min_zoom,
        "zero zoom must clamp to the minimum");

    camera.set_zoom(100.0f);
    require(camera.zoom() == Camera::k_max_zoom,
        "large zoom must clamp to the maximum");

    camera.set_zoom(std::numeric_limits<float>::quiet_NaN());
    require(camera.zoom() == Camera::k_default_zoom,
        "NaN zoom must fall back to the default");

    camera.set_zoom(std::numeric_limits<float>::infinity());
    require(camera.zoom() == Camera::k_default_zoom,
        "infinite zoom must fall back to the default");
}

void test_world_primitive_command_contracts()
{
    using namespace elysia::core;
    require(RenderCommand{}.type == RenderCommandType::Texture,
        "Default world render commands must remain texture commands");

    constexpr Color color{12, 34, 56, 78};
    const RenderCommand fill_rect = make_world_fill_rect_command(
        Rect{1, 2, 3, 4}, color);
    const RenderCommand draw_rect = make_world_draw_rect_command(
        Rect{5, 6, 7, 8}, color, 2.5f);
    const RenderCommand fill_circle = make_world_fill_circle_command(
        Vector2{9, 10}, 11.0f, color);
    const RenderCommand draw_circle = make_world_draw_circle_command(
        Vector2{12, 13}, 14.0f, color,
        std::numeric_limits<float>::quiet_NaN());
    const RenderCommand line = make_world_draw_line_command(
        Vector2{15, 16}, Vector2{17, 18}, color, -2.0f);
    const RenderCommand triangle = make_world_fill_triangle_command(
        Vector2{19, 20}, Vector2{21, 22}, Vector2{23, 24}, color);

    require(fill_rect.type == RenderCommandType::FillRect
            && fill_rect.command_rect == Rect(1, 2, 3, 4)
            && fill_rect.color == color,
        "FillRect factory must preserve world geometry and color");
    require(draw_rect.type == RenderCommandType::DrawRect
            && draw_rect.command_rect == Rect(5, 6, 7, 8)
            && draw_rect.stroke_width == 2.5f,
        "DrawRect factory must preserve its normalized world-space stroke");
    require(fill_circle.type == RenderCommandType::FillCircle
            && fill_circle.circle_center == Vector2(9, 10)
            && fill_circle.circle_radius == 11.0f,
        "FillCircle factory must preserve center and radius");
    require(draw_circle.type == RenderCommandType::DrawCircle
            && draw_circle.stroke_width == 1.0f,
        "DrawCircle must normalize a non-finite stroke width");
    require(line.type == RenderCommandType::DrawLine
            && line.line_start == Vector2(15, 16)
            && line.line_end == Vector2(17, 18)
            && line.stroke_width == 1.0f,
        "DrawLine must preserve endpoints and normalize a non-positive stroke");
    require(triangle.type == RenderCommandType::FillTriangle
            && triangle.triangle_vertices[0] == Vector2(19, 20)
            && triangle.triangle_vertices[1] == Vector2(21, 22)
            && triangle.triangle_vertices[2] == Vector2(23, 24)
            && triangle.color == color,
        "FillTriangle factory must preserve world vertices and color");
}

void test_world_primitive_projection()
{
    using namespace elysia::core;
    constexpr Color color{120, 80, 40, 160};

    for (const float zoom : {0.5f, 1.0f, 2.0f})
    {
        const Camera camera(
            Vector2{100, 50}, Vector2{200, 100}, zoom);

        const RenderCommand rect = make_world_draw_rect_command(
            Rect{90, 45, 20, 10}, color, 3.0f);
        const ScreenRenderCommand projected_rect =
            project_render_command_to_screen(rect, camera);
        require(projected_rect.type == RenderCommandType::DrawRect
                && projected_rect.screen_rect
                    == camera.world_to_screen(rect.command_rect)
                && projected_rect.color == color
                && projected_rect.stroke_width == 3.0f * zoom,
            "World rectangles and strokes must project with camera zoom");

        const RenderCommand circle = make_world_draw_circle_command(
            Vector2{105, 55}, 7.0f, color, 2.0f);
        const ScreenRenderCommand projected_circle =
            project_render_command_to_screen(circle, camera);
        require(projected_circle.type == RenderCommandType::DrawCircle
                && projected_circle.circle_center
                    == camera.world_to_screen(circle.circle_center)
                && projected_circle.circle_radius == 7.0f * zoom
                && projected_circle.stroke_width == 2.0f * zoom,
            "World circle center, radius and stroke must project consistently");

        const RenderCommand line = make_world_draw_line_command(
            Vector2{90, 40}, Vector2{120, 70}, color, 4.0f);
        const ScreenRenderCommand projected_line =
            project_render_command_to_screen(line, camera);
        require(projected_line.type == RenderCommandType::DrawLine
                && projected_line.line_start
                    == camera.world_to_screen(line.line_start)
                && projected_line.line_end
                    == camera.world_to_screen(line.line_end)
                && projected_line.stroke_width == 4.0f * zoom,
            "World line endpoints and width must project with the camera");

        const RenderCommand triangle = make_world_fill_triangle_command(
            Vector2{90, 40}, Vector2{120, 50}, Vector2{100, 70}, color);
        const ScreenRenderCommand projected_triangle =
            project_render_command_to_screen(triangle,camera);
        require(projected_triangle.type == RenderCommandType::FillTriangle
                && projected_triangle.triangle_vertices[0]
                    == camera.world_to_screen(triangle.triangle_vertices[0])
                && projected_triangle.triangle_vertices[1]
                    == camera.world_to_screen(triangle.triangle_vertices[1])
                && projected_triangle.triangle_vertices[2]
                    == camera.world_to_screen(triangle.triangle_vertices[2])
                && projected_triangle.color == color,
            "World triangle vertices must project independently through the camera");
    }
}
}

int main()
{
    test_zoomed_projection_and_inverse();
    test_zoom_levels_and_zero_viewport();
    test_zoom_validation();
    test_world_primitive_command_contracts();
    test_world_primitive_projection();
    std::cout << "camera projection tests passed\n";
    return EXIT_SUCCESS;
}
