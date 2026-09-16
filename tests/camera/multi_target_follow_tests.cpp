#include "engine/camera/multi_target_follow_strategy.h"
#include "engine/camera/camera_controller.h"
#include "engine/camera/camera_manager.h"
#include "engine/scene/scene.h"
#include "tests/support/test_assertions.h"
#include <array>
#include <limits>
#include <iostream>

using namespace elysia::camera;
using namespace elysia::core;
using elysia::tests::require;
namespace
{
void near(float actual, float expected, const char* message)
{
    require(std::abs(actual - expected) < 0.001f, message);
}
CameraFocus single(Rect rect) { return {rect, rect}; }
void tick(MultiTargetFollowStrategy& strategy, CameraFollowContext& context,
    const CameraFocus& focus, double dt)
{
    auto result = strategy.update(context, focus, dt);
    context.current_center = result.center;
    if (result.zoom) context.zoom = *result.zoom;
}
void aggregation()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::array rects{Rect{nan, 0, 10, 10}, Rect{-30, -10, 10, 10}, Rect{50, 60, 0, 0}};
    const auto focus = make_camera_focus(rects);
    require(focus && focus->bounds == Rect(-30, -10, 80, 70), "union must include point targets and skip invalid targets");
    require(focus->primary == rects[1], "invalid primary must fall back to first valid rect");
    require(make_camera_focus(rects, 2)->primary == rects[2], "explicit primary must survive aggregation");
    require(!make_camera_focus({}), "empty list must clear focus");
    require(!make_camera_focus(std::span(rects).first(1)), "all invalid targets must clear focus");
}
void framing_and_smoothing()
{
    MultiTargetFollowStrategy strategy;
    CameraFollowContext context{{}, {1000, 600}, 1};
    auto result = strategy.update(context, single(Rect{-50, -20, 100, 40}), 0.2);
    require(result.center == Vector2{} && result.zoom == 1, "inner targets must not move or immediately zoom in");
    strategy.reset();
    result = strategy.update(context, single(Rect{340, -20, 40, 40}), 0.12);
    near(result.center.x, 15, "dead zone must move halfway toward minimum 30-unit correction");

    for (const Rect bounds : {Rect{-500, -20, 1000, 40}, Rect{-20, -300, 40, 600}, Rect{-500, -300, 1000, 600}})
    {
        strategy.reset(); context = {{}, {1000, 600}, 1};
        tick(strategy, context, single(bounds), 0.10);
        near(context.zoom, 0.85f, "outward half-life must halve zoom error in all directions");
        for (int i = 0; i < 240; ++i) tick(strategy, context, single(bounds), 1.0 / 60);
        near(context.zoom, 0.7f, "zoom must converge without overshoot");
    }
    strategy.reset(); context = {{}, {1000, 600}, 1};
    const auto small = single(Rect{-50, -20, 100, 40});
    tick(strategy, context, small, 0.39);
    require(context.zoom == 1, "zoom-in must wait for settle delay");
    tick(strategy, context, small, 0.11);
    require(context.zoom > 1 && context.zoom < 2, "zoom-in must blend after delay");
    for (int i = 0; i < 300; ++i) tick(strategy, context, small, 1.0 / 60);
    near(context.zoom, 2, "zoom-in must respect max zoom");

    strategy.reset(); context = {{}, {1000, 600}, 1};
    // A target between the inner and safe thresholds cannot start zoom-in.
    for (int i = 0; i < 100; ++i) tick(strategy, context, single(Rect{-300, -10, 600, 20}), 0.02);
    require(context.zoom == 1, "hysteresis band must hold zoom");
    tick(strategy, context, single(Rect{0, 0, 0, 0}), 0.8);
    require(std::isfinite(context.zoom), "point target must not divide by zero");

    MultiTargetFollowConfig config;
    config.dead_zone_enabled = false;
    auto run = [&](int fps) {
        MultiTargetFollowStrategy follow(config);
        CameraFollowContext frame{{}, {1000, 600}, 1};
        for (int i = 0; i < fps; ++i) tick(follow, frame, single(Rect{100, -300, 1000, 600}), 1.0 / fps);
        return frame;
    };
    const auto low = run(30), high = run(144);
    near(low.zoom, high.zoom, "zoom response must be frame-rate independent");
    near(low.current_center.x, high.current_center.x, "center response must be frame-rate independent");
    result = strategy.update(context, small, 0);
    require(result.center == context.current_center && !result.zoom, "zero time must not advance");
    context.viewport_size = {};
    require(!strategy.update(context, small, 1).zoom, "invalid viewport must not advance");
}
void overflow_and_recovery()
{
    MultiTargetFollowStrategy strategy;
    CameraFollowContext context{{}, {1000, 600}, 1};
    const CameraFocus far{Rect{-2000, -20, 4000, 40}, Rect{1980, -20, 20, 40}};
    tick(strategy, context, far, 0.1);
    require(strategy.primary_only() && context.current_center.x > 0, "overflow must move toward primary, not group center");
    require(context.zoom > 0.5f && context.zoom < 1, "overflow must remain smooth");
    for (int i = 0; i < 300; ++i) tick(strategy, context, far, 1.0 / 60);
    near(context.zoom, 0.5f, "overflow must stop at min zoom");
    require(context.current_center.x > 1300, "primary must eventually enter safe area");
    tick(strategy, context, single(Rect{0, 0, 1200, 40}), 1);
    require(strategy.primary_only(), "recovery hysteresis must hold between inner and safe thresholds");
    tick(strategy, context, single(Rect{0, 0, 100, 40}), 0.39);
    require(strategy.primary_only(), "recovery must wait for delay");
    tick(strategy, context, single(Rect{0, 0, 100, 40}), 0.02);
    require(!strategy.primary_only(), "recovery must restore group framing");
    strategy.reset();
    require(!strategy.primary_only(), "reset must clear overflow state");
}
void integration()
{
    Camera camera({}, {1000, 600});
    CameraController controller(camera);
    controller.set_follow_strategy(std::make_unique<MultiTargetFollowStrategy>());
    controller.set_focus(single(Rect{1000, -20, 100, 40}));
    controller.update(0);
    require(camera.center() == Vector2{}, "new strategy must not snap on acquisition");
    controller.update(0.1);
    require(camera.center().x > 0 && camera.center().x < 1050, "new strategy must approach first focus smoothly");
    controller.set_focus(std::nullopt);
    const auto held = camera.center();
    const float held_zoom = camera.zoom();
    controller.update(1);
    require(camera.center() == held && camera.zoom() == held_zoom, "missing focus must hold state");
    controller.set_focus(single(Rect{-1500, -20, 3000, 40}));
    controller.update(0);
    require(camera.center() == held, "reacquisition must not snap");
    controller.start_zoom_transition(2, 1);
    controller.update(0.5);
    near(camera.zoom(), (held_zoom + 2) * 0.5f, "manual transition must own zoom");
    controller.update(0.5);
    near(camera.zoom(), 2, "auto zoom must not overwrite final manual frame");
    controller.update(0.1);
    near(camera.zoom(), 1.25f, "auto zoom must resume smoothly next frame");
    controller.set_zoom(0.2f);
    controller.update(0.1);
    require(camera.zoom() > 0.2f && camera.zoom() < 0.5f,
        "manual zoom below automatic range must recover smoothly");
    controller.set_zoom(1.25f);
    controller.set_world_bounds(Rect{-1000, -600, 2000, 1200});
    controller.set_focus(CameraFocus{Rect{-5000, -20, 4000, 40}, Rect{-5000, -20, 20, 40}});
    controller.set_center({-10000, 0});
    controller.start_shake(CameraShakeParams{.amplitude = {0, 10}, .duration_seconds = 1, .frequency_hz = 0});
    controller.update(0.1);
    require(camera.center().y != controller.logical_center().y, "shake must compose after logical framing");
    const auto half_view = camera.world_viewport_size() * 0.5f;
    near(controller.logical_center().x, -1000 + half_view.x,
        "world bounds must clamp with the newly blended zoom, before shake");
    controller.clear_effects();
    controller.set_world_bounds(std::nullopt);
    controller.set_follow_strategy(std::make_unique<HardFollowStrategy>());
    controller.set_focus_rect(Rect{10, 10, 20, 20});
    controller.update(0);
    require(camera.center() == Vector2(20, 20), "legacy acquisition must still snap");
    controller.set_focus_rect(Rect{1000, 10, 20, 20});
    controller.set_follow_strategy(std::make_unique<SmoothFollowStrategy>(10));
    controller.update(0.1);
    near(camera.center().x, 21, "switching legacy strategies must preserve ongoing focus without a new snap");
    controller.reset_scene_state();
    require(camera.zoom() == 1 && !controller.focus_rect(), "reset must clear focus and zoom");

    auto* manager = CameraManager::instance();
    manager->reset_all();
    manager->set_viewport_size(CameraSlot::Main, {1000, 600});
    manager->set_follow_strategy(CameraSlot::Main, std::make_unique<MultiTargetFollowStrategy>());
    manager->set_focus(CameraSlot::Main, single(Rect{-1000, -20, 2000, 40}));
    manager->set_zoom(CameraSlot::Auxiliary1, 3);
    manager->update(0.1);
    require(manager->camera(CameraSlot::Main).zoom() < 1 && manager->camera(CameraSlot::Auxiliary1).zoom() == 3,
        "automatic zoom must remain isolated to its slot");
    manager->reset_all();
}
class LegacyScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
    using Scene::resolve_camera_focus;
    std::optional<Rect> resolve_camera_focus_rect() const override { return Rect{10, 20, 30, 40}; }
};
}
int main()
{
    aggregation(); framing_and_smoothing(); overflow_and_recovery(); integration();
    LegacyScene legacy;
    require(legacy.resolve_camera_focus()->primary == Rect(10, 20, 30, 40), "legacy Scene hook must adapt to new focus");
    std::cout << "multi-target camera tests passed\n";
}
