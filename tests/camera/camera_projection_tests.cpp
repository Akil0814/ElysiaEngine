#include "engine/camera/camera.h"
#include "tests/support/test_assertions.h"

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
}

int main()
{
    test_zoomed_projection_and_inverse();
    test_zoom_levels_and_zero_viewport();
    test_zoom_validation();
    std::cout << "camera projection tests passed\n";
    return EXIT_SUCCESS;
}
