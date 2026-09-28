#include "engine/scene/runtime/scene_camera_runtime.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace
{
using elysia::camera::CameraBlendState;
using elysia::camera::CameraEasing;
using elysia::camera::CameraManager;
using elysia::camera::CameraMotionState;
using elysia::camera::CameraSlot;
using elysia::core::Vector2;
using elysia::scene::CameraSceneConfig;
using elysia::scene::SceneCameraRuntime;
using elysia::tests::require;

void reset_cameras()
{
    auto* manager = CameraManager::instance();
    manager->reset_all();
    manager->set_viewport_size(CameraSlot::Main, {100.0f, 100.0f});
    manager->set_viewport_size(CameraSlot::Cinematic, {100.0f, 100.0f});
}

void test_configuration_and_slot_boundary()
{
    bool rejected_empty = false;
    try
    {
        SceneCameraRuntime runtime(CameraSceneConfig{.owned_slots = {}});
    }
    catch (const std::invalid_argument&)
    {
        rejected_empty = true;
    }
    require(rejected_empty, "camera runtime must reject an empty owned-slot set");

    SceneCameraRuntime runtime(CameraSceneConfig{});
    bool rejected_foreign = false;
    try
    {
        runtime.set_center(CameraSlot::Cinematic, {});
    }
    catch (const std::logic_error&)
    {
        rejected_foreign = true;
    }
    require(rejected_foreign, "camera runtime must reject a valid slot it does not own");
}

void test_blend_motion_and_handle_ownership()
{
    reset_cameras();
    SceneCameraRuntime runtime(CameraSceneConfig{
        .initial_slot = CameraSlot::Main,
        .owned_slots = CameraSlot::Main | CameraSlot::Cinematic});
    runtime.set_center(CameraSlot::Main, {});
    runtime.set_center(CameraSlot::Cinematic, {100.0f, 0.0f});

    const auto blend = runtime.blend_to(
        CameraSlot::Cinematic, {1.0, CameraEasing::Linear});
    require(blend && runtime.blend_state(*blend) == CameraBlendState::Playing,
        "runtime must expose its active blend handle");
    (void)runtime.advance(0.5);
    require(runtime.presented_camera().center() == Vector2(50.0f, 0.0f),
        "runtime blend must sample the current target slot");
    const auto completed = runtime.advance(0.5);
    require(completed.blend && completed.blend->id == *blend
            && runtime.presented_slot() == CameraSlot::Cinematic,
        "natural blend completion must commit its target and return a value event");

    const auto owned = runtime.move_to(
        CameraSlot::Main, {.center = Vector2(40.0f, 0.0f)}, 1.0, CameraEasing::Linear);
    const auto foreign = CameraManager::instance()->move_camera_to(
        CameraSlot::Auxiliary1, {.center = Vector2(20.0f, 0.0f)}, 1.0);
    require(runtime.motion_state(owned) == CameraMotionState::Playing,
        "runtime must expose motion handles it created");
    require(!runtime.motion_state(foreign) && !runtime.pause_motion(foreign),
        "runtime must reject handles created outside its ownership boundary");
    require(runtime.pause_motion(owned)
            && runtime.motion_state(owned) == CameraMotionState::Paused
            && runtime.resume_motion(owned),
        "owned motion handles must remain controllable");

    runtime.cancel_activity();
    require(!runtime.motion_state(owned) && !runtime.blend_state(*blend),
        "lifecycle cancellation must invalidate runtime-owned playback");
    (void)CameraManager::instance()->cancel_camera_motion(foreign);
}

void test_reset_preserves_viewport_and_restores_initial_slot()
{
    reset_cameras();
    SceneCameraRuntime runtime(CameraSceneConfig{
        .initial_slot = CameraSlot::Main,
        .owned_slots = CameraSlot::Main | CameraSlot::Cinematic});
    runtime.cut_to(CameraSlot::Cinematic);
    runtime.set_center(CameraSlot::Main, {10.0f, 20.0f});
    runtime.set_zoom(CameraSlot::Cinematic, 2.0f);
    runtime.reset();

    require(runtime.presented_slot() == CameraSlot::Main
            && runtime.slot_camera(CameraSlot::Main).center() == Vector2::zero()
            && runtime.slot_camera(CameraSlot::Cinematic).zoom() == 1.0f,
        "reset must restore the initial slot and clear every owned controller");
    require(runtime.slot_camera(CameraSlot::Main).viewport_size() == Vector2(100.0f, 100.0f),
        "runtime reset must preserve the synchronized viewport");
}
} // namespace

int main()
{
    test_configuration_and_slot_boundary();
    test_blend_motion_and_handle_ownership();
    test_reset_preserves_viewport_and_restores_initial_slot();
    return EXIT_SUCCESS;
}
