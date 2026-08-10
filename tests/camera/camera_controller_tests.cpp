#include "engine/camera/camera_controller.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>

namespace
{
using elysia::tests::require;
using elysia::camera::Camera;
using elysia::camera::CameraController;
using elysia::camera::CameraFollowContext;
using elysia::camera::CameraShakeParams;
using elysia::camera::DeadZoneFollowStrategy;
using elysia::camera::HardFollowStrategy;
using elysia::camera::SmoothFollowStrategy;
using elysia::core::Rect;
using elysia::core::Vector2;

void test_focus_loss_and_reacquisition()
{
    Camera camera(Vector2::zero(), Vector2(100.0f, 100.0f));
    CameraController controller(camera);

    controller.set_focus_rect(Rect(5.0f, 5.0f, 10.0f, 10.0f));
    controller.update(0.0);
    require(camera.center() == Vector2(10.0f, 10.0f),
        "first focus must initialize the camera center");

    controller.set_focus_rect(std::nullopt);
    controller.set_center(Vector2(20.0f, 20.0f));
    controller.update(0.0);
    require(camera.center() == Vector2(20.0f, 20.0f),
        "missing focus must preserve an explicitly configured center");

    controller.set_focus_rect(Rect(35.0f, 35.0f, 10.0f, 10.0f));
    controller.update(0.0);
    require(camera.center() == Vector2(40.0f, 40.0f),
        "reacquired focus must initialize again");

    controller.set_center(Vector2(30.0f, 30.0f));
    controller.update(0.0);
    require(camera.center() == Vector2(30.0f, 30.0f),
        "persistent focus without a strategy must not snap every frame");
}

void test_follow_strategies()
{
    const Rect focus(90.0f, 40.0f, 20.0f, 20.0f);
    const CameraFollowContext context{
        Vector2::zero(),
        Vector2(100.0f, 100.0f),
        1.0f
    };

    const HardFollowStrategy hard;
    require(hard.update_center(context, focus, 0.0) == Vector2(100.0f, 50.0f),
        "hard follow must select the focus center");

    const SmoothFollowStrategy smooth(50.0);
    require(
        smooth.update_center(context, focus, 1.0)
            .nearly_equals(Vector2(44.72136f, 22.36068f), 0.0001f),
        "smooth follow must advance by speed times delta"
    );
}

void test_dead_zone_uses_screen_pixels()
{
    const DeadZoneFollowStrategy strategy(Rect(25.0f, 25.0f, 50.0f, 50.0f));
    const Rect focus(15.0f, -5.0f, 10.0f, 10.0f);

    const Vector2 at_one_x = strategy.update_center(
        CameraFollowContext{
            Vector2::zero(),
            Vector2(100.0f, 100.0f),
            1.0f
        },
        focus,
        0.0
    );
    require(at_one_x == Vector2::zero(),
        "focus on the pixel dead-zone edge must not move at 1x");

    const Vector2 at_two_x = strategy.update_center(
        CameraFollowContext{
            Vector2::zero(),
            Vector2(100.0f, 100.0f),
            2.0f
        },
        focus,
        0.0
    );
    require(at_two_x.nearly_equals(Vector2(12.5f, 0.0f)),
        "dead-zone screen correction must convert back to world units");
}

void test_zoom_aware_world_bounds()
{
    Camera camera(Vector2::zero(), Vector2(100.0f, 100.0f));
    CameraController controller(camera);
    controller.set_world_bounds(Rect(0.0f, 0.0f, 200.0f, 200.0f));

    controller.set_center(Vector2(10.0f, 10.0f));
    require(camera.center() == Vector2(50.0f, 50.0f),
        "1x bounds must use half of the screen viewport");

    controller.set_zoom(2.0f);
    controller.set_center(Vector2(10.0f, 10.0f));
    require(camera.center() == Vector2(25.0f, 25.0f),
        "2x bounds must use half of the world viewport");

    controller.set_zoom(0.5f);
    require(camera.center() == Vector2(100.0f, 100.0f),
        "world smaller than the visible area must center the camera");
}

void test_zoom_transition_and_effect_composition()
{
    Camera camera(Vector2::zero(), Vector2(100.0f, 100.0f));
    CameraController controller(camera);

    controller.start_zoom_transition(3.0f, 2.0);
    controller.update(1.0);
    require(camera.zoom() == 2.0f,
        "Smoothstep transition must reach its midpoint at half duration");

    controller.start_zoom_transition(4.0f, 2.0);
    controller.update(1.0);
    require(camera.zoom() == 3.0f,
        "replacement transition must continue from the current zoom");

    controller.clear_effects();
    controller.update(1.0);
    require(camera.zoom() == 3.0f,
        "clearing effects must preserve the current zoom");

    controller.start_zoom_transition(5.0f, 0.0);
    require(camera.zoom() == 5.0f,
        "zero-duration transition must apply immediately");

    controller.set_zoom(1.0f);
    controller.start_zoom_transition(3.0f, 1.0);
    controller.start_shake(CameraShakeParams{
        .amplitude = Vector2(0.0f, 10.0f),
        .duration_seconds = 1.0,
        .frequency_hz = 0.0
    });
    controller.update(0.5);
    require(camera.zoom() == 2.0f,
        "zoom transition must progress while shake is active");
    require(camera.center().nearly_equals(Vector2(0.0f, 5.0f)),
        "shake offset must compose with zoom transition");

    controller.reset_scene_state();
    require(camera.zoom() == Camera::k_default_zoom,
        "reset must restore default zoom");
}
}

int main()
{
    test_focus_loss_and_reacquisition();
    test_follow_strategies();
    test_dead_zone_uses_screen_pixels();
    test_zoom_aware_world_bounds();
    test_zoom_transition_and_effect_composition();
    std::cout << "camera controller tests passed\n";
    return EXIT_SUCCESS;
}
