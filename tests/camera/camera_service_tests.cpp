#include "engine/camera/camera_service.h"
#include "engine/camera/follow_strategy.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
using namespace elysia;
using namespace camera;
using tests::require;
using Access = scene::SceneTestAccess;
using Status = CameraOperationStatus;
using Error = CameraServiceError;
using core::Vector2;

scene::CameraSceneConfig config()
{
    return {.owned_slots = CameraSlot::Main | CameraSlot::Cinematic};
}

struct State
{
    bool fail_attach = false, fail_detach = false, fail_enter = false, fail_exit = false, fail_update = false;
    bool auto_play = false, saw_attach = false, saw_enter = false, saw_detach = false, saw_exit = false;
    bool callback_saw_completion = false;
    int destroyed = 0;
    std::optional<CameraMotionOperation> motion;
    std::optional<CameraBlendOperation> blend;
};

class TestScene final : public scene::Scene
{
public:
    explicit TestScene(State& state, std::optional<scene::CameraSceneConfig> cfg = config())
        : Scene(scene::SceneRuntimeFeatures{.camera = cfg}), _state(state) {}
    ~TestScene() override { ++_state.destroyed; }
    void route(scene::SceneKey key, scene::SceneReloadMode mode = scene::SceneReloadMode::Reuse)
    {
        request_scene_switch(key, {}, mode);
    }
    static inline TestScene* current = nullptr;

private:
    void on_runtime_attach() override
    {
        _state.saw_attach = ELYSIA_CAMERA->is_available() == has_camera();
        if (_state.fail_attach)
        {
            _state.motion = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, 1).value();
            throw std::runtime_error("attach failure");
        }
    }
    void on_runtime_detach() override
    {
        _state.saw_detach = !ELYSIA_CAMERA->is_available();
        if (_state.fail_detach) throw std::runtime_error("detach failure");
    }
    void on_enter(const scene::ScenePayload&) override
    {
        current = this;
        _state.saw_enter = ELYSIA_CAMERA->is_available() == has_camera();
        if (_state.auto_play || _state.fail_enter)
        {
            ELYSIA_CAMERA->set_center(CameraSlot::Main, {}).value();
            _state.motion = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.center = Vector2{100, 0}}, 1).value();
            _state.blend = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
        }
        if (_state.fail_enter) throw std::runtime_error("enter failure");
    }
    void on_exit() override
    {
        _state.saw_exit = !ELYSIA_CAMERA->is_available();
        if (_state.fail_exit) throw std::runtime_error("exit failure");
    }
    void on_reset() override {}
    void on_before_update(double) override
    {
        if (_state.fail_update) throw std::runtime_error("update failure");
    }
    void on_camera_motion_completed(CameraMotionId, CameraSlot) override
    {
        _state.callback_saw_completion = _state.motion && _state.motion->reached_target();
    }
    State& _state;
};

struct Fixture
{
    State state;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context{nullptr, registry, 100, 100};
    TestScene scene;

    explicit Fixture(std::optional<scene::CameraSceneConfig> cfg = config()) : scene(state, cfg)
    {
        CameraManager::instance()->reset_all();
        Access::bind(scene, context);
        Access::attach(scene);
        Access::enter(scene);
    }
    ~Fixture()
    {
        Access::detach(scene);
        if (scene.lifecycle_state() == scene::SceneLifecycleState::Active) Access::exit(scene);
    }
    void step(double delta) { Access::update(scene, delta); }
};

class ImpactObject final : public core::GameObject, public core::Updatable
{
public:
    ImpactObject() : GameObject(core::DepthLayer::Item) {}
    void update(double) override
    {
        accepted = ELYSIA_CAMERA->request_shake({}).has_value();
        set_active(false);
    }
    bool accepted = false;
};

void queries_configuration_and_requests()
{
    require(!ELYSIA_CAMERA->is_available(), "service starts unavailable");
    require(ELYSIA_CAMERA->camera().error() == Error::NoActiveRuntime
        && ELYSIA_CAMERA->owned_slots().error() == Error::NoActiveRuntime
        && ELYSIA_CAMERA->request_shake({}).error() == Error::NoActiveRuntime,
        "queries and requests report missing runtime");
    {
        Fixture fixture(std::nullopt);
        require(!ELYSIA_CAMERA->is_available(), "UI-only scene has no camera service");
    }
    {
        Fixture fixture;
        require(fixture.state.saw_attach && fixture.state.saw_enter, "service binds before scene hooks");
        require(ELYSIA_CAMERA->owned_slots()->contains(CameraSlot::Cinematic), "service exposes owned slots");
        require(ELYSIA_CAMERA->camera(CameraSlot::Count).error() == Error::InvalidSlot,
            "invalid slot is rejected before accessing manager");
        require(ELYSIA_CAMERA->set_zoom(CameraSlot::Auxiliary1, 2).error() == Error::SlotNotOwned,
            "service cannot write a foreign slot");
        const auto snapshot = ELYSIA_CAMERA->camera().value();
        ELYSIA_CAMERA->set_center(CameraSlot::Main, {40, 20}).value();
        require(snapshot.center() == Vector2{} && ELYSIA_CAMERA->camera()->center() == Vector2(40, 20),
            "camera query returns an independent value snapshot");
        ELYSIA_CAMERA->set_zoom(CameraSlot::Main, 100).value();
        require(ELYSIA_CAMERA->camera()->zoom() == Camera::kMaxZoom, "zoom retains existing clamping");
        ELYSIA_CAMERA->set_zoom(CameraSlot::Main, 1).value();
        ELYSIA_CAMERA->set_follow_strategy(CameraSlot::Main, std::make_unique<HardFollowStrategy>()).value();
        ELYSIA_CAMERA->set_focus(CameraSlot::Main, CameraFocus{{0, 0, 20, 20}, {0, 0, 20, 20}}).value();
        ELYSIA_CAMERA->snap_to_focus(CameraSlot::Main).value();
        fixture.step(0);
        require(ELYSIA_CAMERA->camera()->center() == Vector2(10, 10), "focus and strategy are configurable externally");
        ELYSIA_CAMERA->set_focus(CameraSlot::Main, std::nullopt).value();
        ELYSIA_CAMERA->set_world_bounds(CameraSlot::Main, core::Rect{-100, -100, 200, 200}).value();
        ELYSIA_CAMERA->set_world_bounds(CameraSlot::Main, std::nullopt).value();
        ELYSIA_CAMERA->set_center(CameraSlot::Main, {}).value();
        ELYSIA_CAMERA->request_shake({}).value();
        ELYSIA_CAMERA->clear_effects(CameraSlot::Main).value();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera()->center() == Vector2{}, "shake followed by clear respects FIFO");
        ELYSIA_CAMERA->clear_effects(CameraSlot::Main).value();
        ELYSIA_CAMERA->request_shake({}).value();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera()->center() != Vector2{}, "default request shakes Main");
    }
    {
        auto cfg = config();
        cfg.default_request_slot = CameraSlot::Cinematic;
        Fixture fixture(cfg);
        ELYSIA_CAMERA->request_shake({}).value();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera(CameraSlot::Main)->center() == Vector2{}
            && ELYSIA_CAMERA->camera(CameraSlot::Cinematic)->center() != Vector2{},
            "configured default request slot can differ from presentation");
        ELYSIA_CAMERA->request_shake(CameraSlot::Main, {}).value();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera(CameraSlot::Main)->center() != Vector2{}, "explicit slot overrides default");
    }
    {
        Fixture fixture(scene::CameraSceneConfig{.initial_slot = CameraSlot::Cinematic, .owned_slots = CameraSlot::Cinematic});
        ELYSIA_CAMERA->request_shake({}).value();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera()->center() != Vector2{}, "unset default uses initial slot");
    }
    auto bad = config();
    bad.default_request_slot = CameraSlot::Auxiliary1;
    bool rejected = false;
    try { scene::SceneCameraRuntime runtime(bad); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "default request slot must be owned");
    {
        Fixture fixture;
        auto* object = fixture.scene.create_and_add_object<ImpactObject>();
        fixture.step(.01);
        require(object->accepted && ELYSIA_CAMERA->camera()->center() != Vector2{},
            "GameObject can submit a shake without a Scene pointer and consume it in the same frame");
    }
}

void motion_results_and_legacy_interop()
{
    Fixture fixture;
    auto motion = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.center = Vector2{100, 0}}, 1, CameraEasing::Linear).value();
    const auto copy = motion;
    require(ELYSIA_CAMERA->play_path(CameraSlot::Main, {}).error() == Error::InvalidArgument,
        "empty paths return a structured argument error");
    require(ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, std::numeric_limits<double>::infinity()).error()
        == Error::InvalidArgument && motion.status() == Status::Playing, "invalid replacement preserves current playback");
    ELYSIA_CAMERA->pause_motion(motion).value();
    fixture.step(.5);
    require(copy.status() == Status::Paused && ELYSIA_CAMERA->camera()->center() == Vector2{}, "copies observe paused playback");
    ELYSIA_CAMERA->resume_motion(motion).value();
    fixture.state.motion = motion;
    fixture.step(1);
    require(motion.status() == Status::Completed && motion.reached_target() && fixture.state.callback_saw_completion,
        "status is completed before legacy scene callbacks");
    require(ELYSIA_CAMERA->cancel_motion(motion).error() == Error::OperationFinished, "completed operations cannot be cancelled");

    auto hold = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, .1,
        CameraEasing::Linear, CameraMotionEndBehavior::Hold).value();
    fixture.step(.1);
    require(hold.status() == Status::Holding && hold.reached_target(), "Hold reaches the target but remains controllable");
    ELYSIA_CAMERA->pause_motion(hold).value();
    require(hold.status() == Status::Paused && hold.reached_target(), "paused Hold retains completion information");
    ELYSIA_CAMERA->resume_motion(hold).value();
    require(hold.status() == Status::Holding, "resuming Hold restores Holding status");
    ELYSIA_CAMERA->cancel_motion(hold).value();
    require(hold.status() == Status::Cancelled && hold.reached_target(), "cancellation retains reached-target history");

    auto& runtime = Access::camera_runtime(fixture.scene);
    auto old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    auto replacement = runtime.move_to(CameraSlot::Main, {.zoom = 4}, 1);
    require(old.status() == Status::Superseded, "legacy movement supersedes service playback");
    runtime.cancel_motion(replacement);
    old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    runtime.set_center(CameraSlot::Main, {});
    require(old.status() == Status::Superseded, "legacy center setter updates operation status");
    old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    runtime.set_zoom(CameraSlot::Main, 1);
    require(old.status() == Status::Superseded, "legacy zoom setter updates operation status");
    old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    ELYSIA_CAMERA->snap_to_focus(CameraSlot::Main).value();
    fixture.step(0);
    require(old.status() == Status::Superseded, "queued snap supersedes motion when processed");
    old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    runtime.cancel_activity();
    require(old.status() == Status::Cancelled, "legacy activity cancellation updates operation status");
}

void blend_results()
{
    Fixture fixture;
    const auto noop = ELYSIA_CAMERA->blend_to(CameraSlot::Main).value();
    require(noop.status() == Status::Completed && noop.reached_target(), "same-slot blend completes immediately");
    ELYSIA_CAMERA->set_center(CameraSlot::Cinematic, {100, 0}).value();
    auto blend = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic, {1, CameraEasing::Linear}).value();
    require(ELYSIA_CAMERA->blend_to(CameraSlot::Main, {0}).error() == Error::InvalidArgument
        && blend.status() == Status::Playing, "invalid blend does not replace valid playback");
    fixture.step(.5);
    require(ELYSIA_CAMERA->camera()->center() == Vector2(50, 0)
        && ELYSIA_CAMERA->camera(CameraSlot::Main)->center() == Vector2{}, "presented query includes blend result");
    ELYSIA_CAMERA->pause_blend(blend).value();
    fixture.step(1);
    require(blend.status() == Status::Paused && ELYSIA_CAMERA->camera()->center() == Vector2(50, 0), "paused blend stays frozen");
    ELYSIA_CAMERA->resume_blend(blend).value();
    fixture.step(.5);
    require(blend.status() == Status::Completed && blend.reached_target()
        && *ELYSIA_CAMERA->presented_slot() == CameraSlot::Cinematic, "blend commits presentation and completion");
    blend = ELYSIA_CAMERA->blend_to(CameraSlot::Main).value();
    ELYSIA_CAMERA->cut_to(CameraSlot::Main).value();
    require(blend.status() == Status::Superseded, "cut supersedes an active blend");
    blend = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
    auto next = ELYSIA_CAMERA->blend_to(CameraSlot::Main).value();
    require(blend.status() == Status::Superseded, "new blend supersedes previous blend");
    ELYSIA_CAMERA->cancel_blend(next).value();
    require(next.status() == Status::Cancelled && !next.reached_target(), "blend cancellation differs from completion");
}

void pause_exit_and_destruction()
{
    std::optional<CameraBlendOperation> old_blend;
    {
        Fixture fixture;
        old_blend = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
    }
    {
        Fixture fixture;
        // Both fresh runtimes issue local blend ID 1. Service generations must
        // distinguish them even though the old result outlives its scene.
        auto current = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
        require(ELYSIA_CAMERA->pause_blend(*old_blend).error() == Error::StaleOperation
            && ELYSIA_CAMERA->resume_blend(*old_blend).error() == Error::StaleOperation
            && ELYSIA_CAMERA->cancel_blend(*old_blend).error() == Error::StaleOperation
            && current.status() == Status::Playing,
            "identical local blend IDs in different scene instances cannot alias");
    }
    std::optional<CameraMotionOperation> retained;
    std::optional<CameraMotionOperation> completed;
    std::optional<CameraMotionOperation> cancelled;
    {
        Fixture fixture;
        completed = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 1}, .01).value();
        fixture.step(.01);
        cancelled = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, 1).value();
        ELYSIA_CAMERA->cancel_motion(*cancelled).value();
        retained = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, 1).value();
        fixture.scene.pause();
        ELYSIA_CAMERA->request_shake({}).value();
        fixture.step(.05);
        require(!retained->reached_target() && ELYSIA_CAMERA->camera()->center() == Vector2{}, "scene pause defers progression and requests");
        Access::detach(fixture.scene);
        require(retained->status() == Status::SceneEnded && !ELYSIA_CAMERA->is_available(), "detach ends operations before hooks");
        require(ELYSIA_CAMERA->resume_motion(*retained).error() == Error::StaleOperation, "detached operations are stale");
        Access::exit(fixture.scene);
        Access::attach(fixture.scene);
        Access::enter(fixture.scene);
        fixture.scene.resume();
        fixture.step(.01);
        require(ELYSIA_CAMERA->camera()->center() == Vector2{}, "unconsumed shake does not survive same-scene reuse");
        const auto current = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
        require(ELYSIA_CAMERA->cancel_motion(*retained).error() == Error::StaleOperation
            && current.status() == Status::Playing, "old generation cannot cancel current playback");
    }
    require(retained->status() == Status::SceneEnded, "result remains readable after scene destruction");
    require(completed->status() == Status::Completed && completed->reached_target()
        && cancelled->status() == Status::Cancelled, "scene teardown preserves terminal result history");
    State state;
    {
        auto scene = std::make_unique<TestScene>(state);
        Access::attach(*scene);
        retained = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, 1).value();
    }
    require(state.destroyed == 1 && !ELYSIA_CAMERA->is_available() && retained->status() == Status::SceneEnded,
        "retaining a result neither retains the scene nor leaves a dangling binding");
}

void managed_transitions_and_failure_recovery()
{
    State source, target, recovery;
    recovery.auto_play = true;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(nullptr, registry, 100, 100);
    scene::SceneManager manager;
    manager.initialize(context, [](const scene::SceneBoundaryFailure&) { return scene::SceneRoute{.target = 3}; });
    manager.register_game_scene<TestScene>(1, std::ref(source));
    manager.register_game_scene<TestScene>(2, std::ref(target));
    manager.register_game_scene<TestScene>(3, std::ref(recovery));
    manager.start({.target = 1});
    for (const auto mode : {scene::SceneReloadMode::Reuse, scene::SceneReloadMode::Reset, scene::SceneReloadMode::Recreate})
    {
        auto old_motion = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 2}, 1).value();
        auto old_blend = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
        TestScene::current->route(1, mode);
        manager.on_update(0);
        auto current = ELYSIA_CAMERA->blend_to(CameraSlot::Cinematic).value();
        require(old_motion.status() == Status::SceneEnded && old_blend.status() == Status::SceneEnded,
            "all reload modes terminate previous generation");
        require(ELYSIA_CAMERA->cancel_blend(old_blend).error() == Error::StaleOperation
            && current.status() == Status::Playing, "colliding local blend IDs cannot alias service operations");
        ELYSIA_CAMERA->cancel_blend(current).value();
    }
    auto old = ELYSIA_CAMERA->move_to(CameraSlot::Main, {.zoom = 3}, 1).value();
    source.fail_exit = true;
    TestScene::current->route(2);
    manager.on_update(0);
    require(manager.current_scene_key() == 3 && recovery.motion->status() == Status::Playing
        && old.status() == Status::SceneEnded && source.saw_detach && source.saw_exit,
        "exit failure recovers with the new runtime bound and old operations ended");
    require(ELYSIA_CAMERA->cancel_motion(old).error() == Error::StaleOperation,
        "old recovery-generation operation cannot affect new scene");
    manager.on_update(1);
    require(recovery.motion->reached_target(), "recovery scene playback continues normally");
    manager.shutdown();

    for (const bool fail_attach : {true, false})
    {
        State failing, recovered;
        failing.fail_attach = fail_attach;
        failing.fail_enter = !fail_attach;
        recovered.auto_play = true;
        scene::SceneManager next;
        next.initialize(context, [](const scene::SceneBoundaryFailure&) { return scene::SceneRoute{.target = 2}; });
        next.register_game_scene<TestScene>(1, std::ref(failing));
        next.register_game_scene<TestScene>(2, std::ref(recovered));
        next.start({.target = 1});
        require(next.current_scene_key() == 2 && recovered.motion->status() == Status::Playing
            && failing.motion->status() == (fail_attach ? Status::SceneEnded : Status::Cancelled),
            "attach and enter failure roll back service state before recovery");
        require(ELYSIA_CAMERA->cancel_motion(*failing.motion).error() == Error::StaleOperation,
            "failed candidate handles never control recovery scene");
    }
    for (const bool fail_update : {true, false})
    {
        State failing, recovered;
        failing.auto_play = true;
        failing.fail_update = fail_update;
        failing.fail_detach = !fail_update;
        recovered.auto_play = true;
        scene::SceneManager next;
        next.initialize(context, [](const scene::SceneBoundaryFailure&) { return scene::SceneRoute{.target = 2}; });
        next.register_game_scene<TestScene>(1, std::ref(failing));
        next.register_game_scene<TestScene>(2, std::ref(recovered));
        next.start({.target = 1});
        ELYSIA_CAMERA->request_shake({}).value();
        const auto failed_operation = *failing.motion;
        if (!fail_update) TestScene::current->route(2);
        next.on_update(0);
        require(next.current_scene_key() == 2 && failed_operation.status() == Status::SceneEnded
            && recovered.motion->status() == Status::Playing, "update and detach failures isolate old service activity");
        next.on_update(.01);
        require(std::abs(ELYSIA_CAMERA->camera(CameraSlot::Main)->center().y) < .001f,
            "failed source's unconsumed shake does not leak into recovery");
    }
}
}

int main()
{
    queries_configuration_and_requests();
    motion_results_and_legacy_interop();
    blend_results();
    pause_exit_and_destruction();
    managed_transitions_and_failure_recovery();
}
