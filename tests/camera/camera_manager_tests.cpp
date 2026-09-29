#define SDL_MAIN_HANDLED

#include "engine/camera/camera_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace
{
using elysia::tests::require;
using elysia::camera::CameraManager;
using elysia::camera::CameraPoseTarget;
using elysia::camera::CameraSlot;
using elysia::camera::CameraSlotSet;
using elysia::core::Rect;
using elysia::core::Vector2;

constexpr std::array<CameraSlot, 4> kCameraSlots{
    CameraSlot::Main,
    CameraSlot::Cinematic,
    CameraSlot::Auxiliary1,
    CameraSlot::Auxiliary2
};

using CameraAccess = decltype(
    std::declval<CameraManager&>().camera(CameraSlot::Main)
);
static_assert(std::is_same_v<CameraAccess, const elysia::camera::Camera&>);

void reset_cameras()
{
    auto* manager = CameraManager::instance();
    manager->reset_all();

    for (CameraSlot slot : kCameraSlots)
    {
        manager->set_viewport_size(slot, Vector2::zero());
    }
}

void test_fixed_slots_are_independent()
{
    reset_cameras();
    auto* manager = CameraManager::instance();

    manager->set_center(CameraSlot::Main, Vector2(10.0f, 20.0f));
    manager->set_center(CameraSlot::Cinematic, Vector2(30.0f, 40.0f));
    manager->set_center(CameraSlot::Auxiliary1, Vector2(50.0f, 60.0f));
    manager->set_center(CameraSlot::Auxiliary2, Vector2(70.0f, 80.0f));
    manager->set_zoom(CameraSlot::Main, 2.0f);
    manager->set_zoom(CameraSlot::Cinematic, 3.0f);

    require(manager->camera(CameraSlot::Main).center() == Vector2(10.0f, 20.0f),
        "Main camera must retain its own center");
    require(manager->camera(CameraSlot::Cinematic).center() == Vector2(30.0f, 40.0f),
        "Cinematic camera must retain its own center");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2(50.0f, 60.0f),
        "Auxiliary1 camera must retain its own center");
    require(manager->camera(CameraSlot::Auxiliary2).center() == Vector2(70.0f, 80.0f),
        "Auxiliary2 camera must retain its own center");
    require(manager->camera(CameraSlot::Main).zoom() == 2.0f,
        "Main camera must retain its own zoom");
    require(manager->camera(CameraSlot::Cinematic).zoom() == 3.0f,
        "Cinematic camera must retain its own zoom");
    require(manager->camera(CameraSlot::Auxiliary1).zoom() == 1.0f,
        "zoom changes must not affect other slots");

    const Rect focus_rect(90.0f, 40.0f, 20.0f, 20.0f);
    manager->set_focus(CameraSlot::Cinematic,
        elysia::camera::CameraFocus{focus_rect, focus_rect});
    manager->set_follow_strategy(
        CameraSlot::Cinematic,
        std::make_unique<elysia::camera::HardFollowStrategy>()
    );
    (void)manager->update(CameraSlotSet::all(), 0.0);

    require(manager->camera(CameraSlot::Cinematic).center() == Vector2(100.0f, 50.0f),
        "CameraManager::update must advance configured cameras");
    require(manager->camera(CameraSlot::Main).center() == Vector2(10.0f, 20.0f),
        "updating Cinematic must not change Main");
}

void test_requests_are_fifo_and_targeted()
{
    reset_cameras();
    auto* manager = CameraManager::instance();
    manager->set_center(CameraSlot::Main, Vector2(5.0f, 5.0f));

    const elysia::camera::CameraShakeParams shake{
        .amplitude = Vector2(0.0f, 10.0f),
        .duration_seconds = 1.0,
        .frequency_hz = 0.0
    };

    manager->request_shake(CameraSlot::Main, shake);
    manager->request_clear_effects(CameraSlot::Main);
    (void)manager->update(CameraSlotSet::all(), 0.1);
    require(manager->camera(CameraSlot::Main).center() == Vector2(5.0f, 5.0f),
        "a later clear request must cancel an earlier shake before camera update");

    manager->request_clear_effects(CameraSlot::Main);
    manager->request_shake(CameraSlot::Main, shake);
    (void)manager->update(CameraSlotSet::all(), 0.1);
    require(manager->camera(CameraSlot::Main).center().nearly_equals(Vector2(5.0f, 14.0f)),
        "a later shake request must remain active after an earlier clear request");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2::zero(),
        "Main requests must not affect Auxiliary1");

    manager->set_zoom(CameraSlot::Main, 1.0f);
    const auto motion = manager->move_camera_to(
        CameraSlot::Main, {.zoom = 3.0f}, 2.0);
    (void)manager->update(CameraSlotSet(CameraSlot::Main), 1.0);
    require(manager->camera(CameraSlot::Main).zoom() == 2.0f,
        "camera motion must animate zoom through the common pose system");
    require(manager->camera_motion_state(motion) == elysia::camera::CameraMotionState::Playing,
        "motion handle must expose active playback state");

    manager->set_zoom(CameraSlot::Main, 1.5f);
    (void)manager->update(CameraSlotSet::all(), 1.0);
    require(manager->camera(CameraSlot::Main).zoom() == 1.5f,
        "immediate zoom must cancel queued transitions for the same slot");
}

void test_reset_preserves_viewport_and_other_slots()
{
    reset_cameras();
    auto* manager = CameraManager::instance();

    manager->set_viewport_size(CameraSlot::Main, Vector2(320.0f, 180.0f));
    manager->set_center(CameraSlot::Main, Vector2(25.0f, 30.0f));
    manager->set_zoom(CameraSlot::Main, 2.0f);
    manager->set_center(CameraSlot::Auxiliary1, Vector2(75.0f, 80.0f));
    manager->set_zoom(CameraSlot::Auxiliary1, 3.0f);
    manager->request_shake(CameraSlot::Main, elysia::camera::CameraShakeParams{});

    manager->reset(CameraSlot::Main);
    (void)manager->update(CameraSlotSet::all(), 0.01);

    require(manager->camera(CameraSlot::Main).center() == Vector2::zero(),
        "reset must clear the camera center and pending requests");
    require(manager->camera(CameraSlot::Main).viewport_size() == Vector2(320.0f, 180.0f),
        "reset must preserve the camera viewport");
    require(manager->camera(CameraSlot::Main).zoom() == 1.0f,
        "reset must restore the default zoom");
    require(manager->camera(CameraSlot::Auxiliary1).center() == Vector2(75.0f, 80.0f),
        "resetting Main must not reset Auxiliary1");
    require(manager->camera(CameraSlot::Auxiliary1).zoom() == 3.0f,
        "resetting Main must not reset Auxiliary1 zoom");
}

void test_motion_playback_and_slot_isolation()
{
    reset_cameras();
    auto* manager = CameraManager::instance();
    manager->set_viewport_size(CameraSlot::Main, {100.0f, 100.0f});
    manager->set_viewport_size(CameraSlot::Cinematic, {100.0f, 100.0f});

    const auto main_motion = manager->play_camera_path(CameraSlot::Main, {
        .nodes = {
            {{.center = Vector2(10.0f, 0.0f)}, 1.0, elysia::camera::CameraEasing::Linear},
            {{.center = Vector2(20.0f, 0.0f), .zoom = 3.0f}, 1.0,
             elysia::camera::CameraEasing::Linear}
        }
    });
    const auto cinematic_motion = manager->move_camera_to(
        CameraSlot::Cinematic, {.center = Vector2(100.0f, 0.0f)}, 1.0,
        elysia::camera::CameraEasing::Linear);

    (void)manager->update(CameraSlotSet(CameraSlot::Main), 0.5);
    require(manager->camera(CameraSlot::Main).center() == Vector2(5.0f, 0.0f),
        "path playback must interpolate from the captured logical pose");
    require(manager->camera(CameraSlot::Cinematic).center() == Vector2::zero(),
        "slot-set updates must not advance unselected cameras");

    require(manager->pause_camera_motion(main_motion), "active motion must be pausable");
    (void)manager->update(CameraSlotSet(CameraSlot::Main), 0.5);
    require(manager->camera(CameraSlot::Main).center() == Vector2(5.0f, 0.0f),
        "paused motion must preserve its current pose");
    require(manager->resume_camera_motion(main_motion), "paused motion must resume");
    (void)manager->update(CameraSlotSet(CameraSlot::Main), 1.0);
    require(manager->camera(CameraSlot::Main).center() == Vector2(15.0f, 0.0f),
        "one update may cross a path-node boundary without losing remaining delta");

    const auto completions = manager->update(CameraSlotSet(CameraSlot::Main), 0.5);
    require(completions.size() == 1 && completions.front().id == main_motion,
        "natural path completion must return a value event");
    require(!manager->camera_motion_state(main_motion),
        "completed ResumeFollow motion handles must become inactive");
    require(manager->camera(CameraSlot::Main).center() == Vector2(20.0f, 0.0f)
            && manager->camera(CameraSlot::Main).zoom() == 3.0f,
        "completion frame must retain the exact final pose");

    const auto hold = manager->move_camera_to(
        CameraSlot::Main, {.center = Vector2(30.0f, 0.0f)}, 0.5,
        elysia::camera::CameraEasing::Linear,
        elysia::camera::CameraMotionEndBehavior::Hold);
    (void)manager->update(CameraSlotSet(CameraSlot::Main), 0.5);
    require(manager->camera_motion_state(hold) == elysia::camera::CameraMotionState::Holding,
        "Hold motion must retain an addressable terminal state");
    require(manager->cancel_camera_motion(hold), "held motion must be cancellable");
    require(!manager->cancel_camera_motion(hold), "stale motion cancellation must be a normal miss");
    require(manager->camera_motion_state(cinematic_motion)
            == elysia::camera::CameraMotionState::Playing,
        "motion handles must remain isolated across slots");

    bool rejected_empty_path = false;
    try
    {
        (void)manager->play_camera_path(CameraSlot::Main, {});
    }
    catch (const std::invalid_argument&)
    {
        rejected_empty_path = true;
    }
    require(rejected_empty_path, "empty camera paths must be rejected as contract errors");
}

class PresentationScene final : public elysia::scene::Scene
{
public:
    explicit PresentationScene(bool advance_when_paused = false)
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .camera = elysia::scene::CameraSceneConfig{
                  .initial_slot = CameraSlot::Main,
                  .owned_slots = CameraSlot::Main | CameraSlot::Cinematic,
                  .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame,
                  .advance_when_paused = advance_when_paused
              }})
    {}

    std::optional<elysia::camera::CameraBlendId> blend(
        CameraSlot slot, double duration = 1.0)
    {
        return camera_runtime().blend_to(
            slot, {duration, elysia::camera::CameraEasing::Linear});
    }
    void cut(CameraSlot slot) { camera_runtime().cut_to(slot); }
    bool cancel_blend(elysia::camera::CameraBlendId id) { return camera_runtime().cancel_blend(id); }
    bool pause_blend(elysia::camera::CameraBlendId id) { return camera_runtime().pause_blend(id); }
    bool resume_blend(elysia::camera::CameraBlendId id) { return camera_runtime().resume_blend(id); }
    std::optional<elysia::camera::CameraBlendState> blend_state(
        elysia::camera::CameraBlendId id) const { return camera_runtime().blend_state(id); }
    elysia::camera::CameraMotionId move(CameraSlot slot, const CameraPoseTarget& target)
    {
        return camera_runtime().move_to(
            slot, target, 1.0, elysia::camera::CameraEasing::Linear);
    }
    CameraSlot presented_slot() const { return camera_runtime().presented_slot(); }

    Vector2 main_focus{};
    Vector2 cinematic_focus{100.0f, 0.0f};
    int blend_completions = 0;
    int motion_completions = 0;

private:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(CameraSlot slot) const override
    {
        const Vector2 center = slot == CameraSlot::Main ? main_focus : cinematic_focus;
        const Rect rect = Rect::from_center(center, {1.0f, 1.0f});
        return elysia::camera::CameraFocus{rect, rect};
    }
    void on_camera_blend_completed(elysia::camera::CameraBlendId, CameraSlot) override
    {
        ++blend_completions;
    }
    void on_camera_motion_completed(elysia::camera::CameraMotionId, CameraSlot) override
    {
        ++motion_completions;
    }
};

class ManualCameraScene final : public elysia::scene::Scene
{
public:
    ManualCameraScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .camera = elysia::scene::CameraSceneConfig{}})
    {}
    elysia::camera::CameraMotionId move(const CameraPoseTarget& target)
    {
        return camera_runtime().move_to(
            CameraSlot::Main, target, 1.0, elysia::camera::CameraEasing::Linear);
    }
private:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class InvalidOwnedSlotScene final : public elysia::scene::Scene
{
public:
    InvalidOwnedSlotScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .camera = elysia::scene::CameraSceneConfig{
                  .initial_slot = CameraSlot::Main,
                  .owned_slots = CameraSlot::Main | CameraSlot::Count
              }})
    {}
private:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class FailingCameraScene final : public elysia::scene::Scene
{
public:
    FailingCameraScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .camera = elysia::scene::CameraSceneConfig{
                  .initial_slot = CameraSlot::Main,
                  .owned_slots = CameraSlot::Main | CameraSlot::Cinematic
              }})
    {}

    elysia::camera::CameraMotionId started_motion{};

private:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        started_motion = camera_runtime().move_to(
            CameraSlot::Cinematic, {.center = Vector2(50.0f, 0.0f)}, 1.0);
        (void)camera_runtime().blend_to(
            CameraSlot::Cinematic, {1.0, elysia::camera::CameraEasing::Linear});
        throw std::runtime_error("camera scene enter failure");
    }
    void on_exit() override {}
    void on_reset() override {}
};

void configure_presentation_scene_cameras()
{
    auto* manager = CameraManager::instance();
    manager->reset_all();
    manager->set_viewport_size(CameraSlot::Main, {100.0f, 100.0f});
    manager->set_viewport_size(CameraSlot::Cinematic, {100.0f, 100.0f});
    manager->set_follow_strategy(
        CameraSlot::Main, std::make_unique<elysia::camera::HardFollowStrategy>());
    manager->set_follow_strategy(
        CameraSlot::Cinematic, std::make_unique<elysia::camera::HardFollowStrategy>());
}

void test_scene_camera_presentation_and_lifecycle()
{
    configure_presentation_scene_cameras();
    PresentationScene scene;
    elysia::scene::SceneTestAccess::enter(scene);
    elysia::scene::SceneTestAccess::update(scene, 0.0);
    require(scene.camera().center() == Vector2::zero(),
        "Scene must initially present its configured slot");

    const auto first_blend = scene.blend(CameraSlot::Cinematic);
    require(first_blend.has_value(), "blend to another owned slot must start playback");
    require(scene.pause_blend(*first_blend), "active blend handles must be pausable");
    elysia::scene::SceneTestAccess::update(scene, 0.25);
    require(scene.camera().center() == Vector2::zero()
            && scene.blend_state(*first_blend) == elysia::camera::CameraBlendState::Paused,
        "explicitly paused blend must not advance");
    require(scene.resume_blend(*first_blend), "paused blend handles must resume");
    elysia::scene::SceneTestAccess::update(scene, 0.5);
    require(scene.camera().center() == Vector2(50.0f, 0.0f),
        "Scene camera must blend source snapshot toward the live target slot");

    scene.cinematic_focus = {200.0f, 0.0f};
    elysia::scene::SceneTestAccess::update(scene, 0.5);
    require(scene.presented_slot() == CameraSlot::Cinematic
            && scene.camera().center() == Vector2(200.0f, 0.0f),
        "completed blend must commit the target's latest pose without an end jump");
    require(scene.blend_completions == 1,
        "natural blend completion must emit exactly one value hook");

    scene.cut(CameraSlot::Main);
    const auto interrupted = scene.blend(CameraSlot::Cinematic);
    elysia::scene::SceneTestAccess::update(scene, 0.25);
    require(scene.camera().center() == Vector2(50.0f, 0.0f),
        "partial blend must expose its composed presentation pose");
    const auto replacement = scene.blend(CameraSlot::Main);
    require(replacement && replacement != interrupted,
        "replacement blend must receive a new handle");
    elysia::scene::SceneTestAccess::update(scene, 0.5);
    require(scene.camera().center() == Vector2(25.0f, 0.0f),
        "replacement blend must start from the current composed pose");
    require(scene.cancel_blend(*replacement), "active blend must be cancellable");
    require(scene.camera().center() == Vector2::zero(),
        "cancelled blend must return to the last committed slot");

    const auto motion = scene.move(CameraSlot::Cinematic, {.center = Vector2(300.0f, 0.0f)});
    elysia::scene::SceneTestAccess::update(scene, 1.0);
    require(scene.motion_completions == 1,
        "Scene must dispatch natural camera motion completion after camera resolution");
    require(!CameraManager::instance()->camera_motion_state(motion),
        "completed Scene motion must no longer be active");

    const auto exiting_motion = scene.move(
        CameraSlot::Cinematic, {.center = Vector2(400.0f, 0.0f)});
    elysia::scene::SceneTestAccess::exit(scene);
    require(!CameraManager::instance()->camera_motion_state(exiting_motion),
        "Scene exit must cancel owned camera playback without a completion hook");
    require(scene.motion_completions == 1,
        "lifecycle cancellation must not report natural completion");

    CameraManager::instance()->set_center(CameraSlot::Main, {10.0f, 0.0f});
    CameraManager::instance()->set_center(CameraSlot::Cinematic, {20.0f, 0.0f});
    elysia::scene::SceneTestAccess::reset(scene);
    require(CameraManager::instance()->camera(CameraSlot::Main).center() == Vector2::zero()
            && CameraManager::instance()->camera(CameraSlot::Cinematic).center() == Vector2::zero(),
        "Scene reset must clear every owned slot");
}

void test_scene_camera_pause_policy()
{
    configure_presentation_scene_cameras();
    PresentationScene frozen;
    elysia::scene::SceneTestAccess::enter(frozen);
    const auto frozen_blend = frozen.blend(CameraSlot::Cinematic);
    frozen.pause();
    elysia::scene::SceneTestAccess::update(frozen, 0.5);
    require(frozen.camera().center() == Vector2::zero(),
        "camera runtime must freeze with a paused Scene by default");
    frozen.resume();
    elysia::scene::SceneTestAccess::update(frozen, 0.5);
    require(frozen.camera().center() == Vector2(50.0f, 0.0f),
        "resuming the Scene must continue the frozen blend");
    require(frozen.cancel_blend(*frozen_blend), "resumed blend must remain controllable");
    elysia::scene::SceneTestAccess::exit(frozen);

    configure_presentation_scene_cameras();
    PresentationScene advancing(true);
    elysia::scene::SceneTestAccess::enter(advancing);
    (void)advancing.blend(CameraSlot::Cinematic);
    advancing.pause();
    elysia::scene::SceneTestAccess::update(advancing, 0.5);
    require(advancing.camera().center() == Vector2(50.0f, 0.0f),
        "advance_when_paused must keep camera playback running");
    advancing.resume();
    elysia::scene::SceneTestAccess::exit(advancing);
}

void test_manual_scene_still_advances_camera_runtime()
{
    reset_cameras();
    auto* manager = CameraManager::instance();
    manager->set_viewport_size(CameraSlot::Main, {100.0f, 100.0f});
    ManualCameraScene scene;
    elysia::scene::SceneTestAccess::enter(scene);
    const auto motion = scene.move({.center = Vector2(40.0f, 0.0f)});
    elysia::scene::SceneTestAccess::update(scene, 0.5);
    require(manager->camera(CameraSlot::Main).center() == Vector2(20.0f, 0.0f),
        "Manual focus mode must still advance effects and pose motion");
    require(manager->camera_motion_state(motion) == elysia::camera::CameraMotionState::Playing,
        "manual scene motion must remain controllable by handle");
    elysia::scene::SceneTestAccess::exit(scene);
}

void test_scene_camera_configuration_validation()
{
    bool rejected_invalid_ownership = false;
    try
    {
        InvalidOwnedSlotScene scene;
    }
    catch (const std::invalid_argument&)
    {
        rejected_invalid_ownership = true;
    }
    require(rejected_invalid_ownership,
        "Scene camera ownership must reject CameraSlot::Count even when valid slots are present");

    reset_cameras();
    FailingCameraScene failing;
    bool enter_failed = false;
    try
    {
        elysia::scene::SceneTestAccess::enter(failing);
    }
    catch (const std::runtime_error&)
    {
        enter_failed = true;
    }
    require(enter_failed, "failing Scene enter must preserve its original exception");
    require(!CameraManager::instance()->camera_motion_state(failing.started_motion),
        "Scene enter failure must cancel motion started by the candidate");
    require(failing.camera().center() == CameraManager::instance()->camera(CameraSlot::Main).center(),
        "Scene enter failure must discard its local presentation blend");
}

class CameraScene final : public elysia::scene::Scene
{
public:
    explicit CameraScene(CameraSlot slot = CameraSlot::Main)
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .camera = elysia::scene::CameraSceneConfig{
                  .initial_slot = slot, .owned_slots = slot}})
    {
        last_instance = this;
    }

    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}

    void request_self_reset()
    {
        request_scene_switch(1, {}, elysia::scene::SceneReloadMode::Reset);
    }

    void request_self_reuse()
    {
        request_scene_switch(1, {}, elysia::scene::SceneReloadMode::Reuse);
    }

    CameraSlot presented_slot() const { return camera_runtime().presented_slot(); }

    static inline CameraScene* last_instance = nullptr;
};

void test_scene_defaults_and_main_lifecycle()
{
    reset_cameras();
    auto* cameras = CameraManager::instance();
    cameras->set_viewport_size(CameraSlot::Main, Vector2(640.0f, 360.0f));
    cameras->set_center(CameraSlot::Main, Vector2(10.0f, 20.0f));
    cameras->set_center(CameraSlot::Cinematic, Vector2(70.0f, 80.0f));

    {
        CameraScene scene(CameraSlot::Cinematic);
        require(scene.presented_slot() == CameraSlot::Cinematic,
            "Scene runtime configuration must select its render camera");
        require(scene.camera().center() == Vector2(70.0f, 80.0f),
            "Scene must read an explicitly selected render camera");
    }

    elysia::scene::SceneManager scene_manager;
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 640, 360);
    scene_manager.initialize(context);
    scene_manager.register_game_scene<CameraScene>(1);
    scene_manager.start(elysia::scene::SceneRoute{
        .target = 1,
        .reload_mode = elysia::scene::SceneReloadMode::Reuse
    });

    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "entering the first managed scene must reset Main");
    require(cameras->camera(CameraSlot::Main).viewport_size() == Vector2(640.0f, 360.0f),
        "scene transitions must preserve Main viewport");
    require(cameras->camera(CameraSlot::Cinematic).center() == Vector2(70.0f, 80.0f),
        "scene transitions must preserve Cinematic");

    cameras->set_center(CameraSlot::Main, Vector2(35.0f, 45.0f));
    CameraScene::last_instance->request_self_reset();
    scene_manager.on_update(0.0);

    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "Reset reload must clear Main before re-entering the scene");
    require(cameras->camera(CameraSlot::Cinematic).center() == Vector2(70.0f, 80.0f),
        "Reset reload must not clear Cinematic");

    cameras->set_center(CameraSlot::Main, Vector2(35.0f, 45.0f));
    cameras->set_zoom(CameraSlot::Main, 2.0f);
    CameraScene::last_instance->request_self_reuse();
    scene_manager.on_update(0.0);

    require(cameras->camera(CameraSlot::Main).center() == Vector2(35.0f, 45.0f),
        "Reuse reload must preserve Main center");
    require(cameras->camera(CameraSlot::Main).zoom() == 2.0f,
        "Reuse reload must preserve Main zoom");

    scene_manager.shutdown();
    require(cameras->camera(CameraSlot::Main).center() == Vector2::zero(),
        "shutdown must reset Main center");
    require(cameras->camera(CameraSlot::Main).zoom() == 1.0f,
        "shutdown must reset Main zoom");
}
}

int main()
{
    test_fixed_slots_are_independent();
    test_requests_are_fifo_and_targeted();
    test_reset_preserves_viewport_and_other_slots();
    test_motion_playback_and_slot_isolation();
    test_scene_camera_presentation_and_lifecycle();
    test_scene_camera_pause_policy();
    test_manual_scene_still_advances_camera_runtime();
    test_scene_camera_configuration_validation();
    test_scene_defaults_and_main_lifecycle();
    reset_cameras();
    std::cout << "camera manager tests passed\n";
    return EXIT_SUCCESS;
}
