#define SDL_MAIN_HANDLED
#include "engine/scene/scene_manager.h"
#include "engine/scene/detail/scene_failure_boundary.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/tools/logger.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace elysia;
using tests::require;
using scene::SceneBoundary;
using scene::SceneManagerState;
using scene::SceneRoute;
constexpr unsigned kDetach = 1, kExit = 2, kRemoval = 4;

struct LogCapture
{
    std::ostringstream output;
    std::streambuf* previous = std::clog.rdbuf(output.rdbuf());
    ~LogCapture() { std::clog.rdbuf(previous); }
};

struct State
{
    std::vector<std::string>& events;
    std::string name;
    bool fail_construct = false, fail_enter = false, fail_attach = false;
    bool backend_enter = false;
    bool fail_update = false, queue_request = false, play_motion = false;
    bool removal_changes_camera = false;
    unsigned cleanup_errors = 0, backend_error = 0;
    int constructions = 0, destructions = 0, enters = 0, exits = 0, detaches = 0, removals = 0;
    int object_destructions = 0;
    bool removal_saw_active_runtime = false;
    camera::CameraMotionId motion;
    std::source_location primary_origin = std::source_location::current();
    std::source_location backend_origin = std::source_location::current();
};

class ProbeObject final : public ui::UiElement
{
public:
    explicit ProbeObject(State& state) : _state(state) {}
    ~ProbeObject() override
    {
        _state.events.push_back(_state.name + ".object_destroy");
        ++_state.object_destructions;
    }
private:
    State& _state;
};

class ProbeScene final : public scene::Scene
{
public:
    explicit ProbeScene(State& state)
        : Scene(scene::SceneRuntimeFeatures{.camera = scene::CameraSceneConfig{}}), _state(state)
    {
        record("construct");
        ++_state.constructions;
        if (_state.fail_construct) throw std::runtime_error("target construction");
    }
    ~ProbeScene() override
    {
        record("destroy");
        ++_state.destructions;
    }

private:
    void record(const char* operation) { _state.events.push_back(_state.name + "." + operation); }
    void cleanup(unsigned stage, const char* message)
    {
        if (_state.backend_error == stage)
            throw core::RenderBackendError({message,
                core::make_failure_diagnostic(message, {}, _state.backend_origin)});
        if ((_state.cleanup_errors & stage) != 0) throw std::runtime_error(message);
    }
    void on_enter(const scene::ScenePayload&) override
    {
        record("enter");
        ++_state.enters;
        if (!_objects_created)
        {
            create_and_add_object<ProbeObject>(_state);
            create_and_add_object<ProbeObject>(_state);
            _objects_created = true;
        }
        if (_state.backend_enter)
            throw core::RenderBackendError({"recovery enter backend",
                core::make_failure_diagnostic("recovery enter backend", {}, _state.backend_origin)});
        if (_state.fail_enter) throw std::runtime_error("candidate enter");
        if (_state.play_motion)
        {
            camera_runtime().set_center(camera::CameraSlot::Main, {});
            _state.motion = camera_runtime().move_to(camera::CameraSlot::Main,
                {.center = core::Vector2{100, 0}}, 1, camera::CameraEasing::Linear);
        }
    }
    void on_exit() override
    {
        record("exit");
        ++_state.exits;
        cleanup(kExit, "exit cleanup");
    }
    void on_reset() override {}
    void on_runtime_attach() override
    {
        record("attach");
        if (_state.fail_attach) throw std::runtime_error("candidate attach");
    }
    void on_runtime_detach() override
    {
        record("detach");
        ++_state.detaches;
        cleanup(kDetach, "detach cleanup");
    }
    void on_before_update(double) override
    {
        if (!_state.fail_update) return;
        if (_state.queue_request) request_scene_switch(3);
        throw scene::SceneBoundaryRuntimeError(SceneBoundary::Update, "primary update", _state.primary_origin);
    }
    void on_scene_object_removing(core::SceneObject&) override
    {
        record("remove");
        ++_state.removals;
        _state.removal_saw_active_runtime |= object_query::GameObjectQueryService::instance()->is_available();
        if (_state.removal_changes_camera)
            camera_runtime().set_center(camera::CameraSlot::Main, {700, 0});
        cleanup(kRemoval, "removal cleanup");
    }
    State& _state;
    bool _objects_created = false;
};

class Observer final : public scene::SceneManagerObserver
{
public:
    void on_scene_manager_quit_requested() override { ++quits; }
    void on_scene_manager_fault(const scene::SceneBoundaryFailure& failure) override
    {
        ++faults;
        last = failure;
    }
    int quits = 0, faults = 0;
    scene::SceneBoundaryFailure last;
};

struct Fixture
{
    std::vector<std::string> events;
    State source{events, "source"}, recovery{events, "recovery"}, target{events, "target"};
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context{nullptr, registry, 1280, 720};
    Observer observer;
    scene::SceneManager manager;
    int factory_calls = 0;

    void initialize(scene::SceneFailureRouteFactory factory)
    {
        manager.initialize(context, std::move(factory));
        manager.attach(&observer);
        manager.register_game_scene<ProbeScene>(1, std::ref(source));
        manager.register_game_scene<ProbeScene>(2, std::ref(recovery));
        manager.register_game_scene<ProbeScene>(3, std::ref(target));
    }
    void initialize()
    {
        initialize([this](const scene::SceneBoundaryFailure&) {
            ++factory_calls;
            events.push_back("factory");
            require(manager.current_scene_key() == scene::SceneKeys::Invalid,
                "failure route factory must run without a current scene");
            require(!object_query::GameObjectQueryService::instance()->is_available(),
                "failure route factory must run after query runtime unbinding");
            return SceneRoute{.target = 2};
        });
    }
    void fail_update()
    {
        manager.start({.target = 1});
        source.fail_update = true;
        manager.on_update(0);
    }
};

std::size_t event_index(const Fixture& fixture, const char* event)
{
    const auto found = std::find(fixture.events.begin(), fixture.events.end(), event);
    require(found != fixture.events.end(), "required lifecycle event must have occurred");
    return static_cast<std::size_t>(found - fixture.events.begin());
}

std::string diagnostic(const scene::SceneBoundaryFailure& failure)
{
    return core::format_failure_diagnostic(scene::to_failure_diagnostic(failure), "SCENE", "scene");
}

void require_isolated(Fixture& fixture)
{
    require(fixture.manager.current_scene_key() == scene::SceneKeys::Invalid
        && !object_query::GameObjectQueryService::instance()->is_available(),
        "fault must leave no current scene or active query runtime");
    require(fixture.source.destructions == 1 && !fixture.source.removal_saw_active_runtime,
        "failed scene must be destroyed while global scene services are unbound");
}

void recovery_order_and_camera()
{
    Fixture fixture;
    fixture.source.queue_request = true;
    fixture.source.removal_changes_camera = true;
    fixture.recovery.play_motion = true;
    fixture.initialize();
    LogCapture logs;
    fixture.fail_update();
    require(fixture.manager.state() == SceneManagerState::Running
        && fixture.manager.current_scene_key() == 2 && fixture.factory_calls == 1,
        "ordinary runtime failure must enter the recovery scene once");
    for (const auto& pair : std::array{
        std::pair{"source.detach", "source.exit"}, std::pair{"source.exit", "source.remove"},
        std::pair{"source.remove", "source.object_destroy"}, std::pair{"source.object_destroy", "source.destroy"},
        std::pair{"source.destroy", "factory"},
        std::pair{"factory", "recovery.construct"}, std::pair{"recovery.construct", "recovery.enter"}})
        require(event_index(fixture, pair.first) < event_index(fixture, pair.second),
            "recovery must follow complete failed-scene teardown");
    require(fixture.source.removals == 2 && fixture.source.object_destructions == 2
        && !fixture.source.removal_saw_active_runtime,
        "all failed-scene objects must be removed and destroyed before recovery service binding");
    require(std::count(fixture.events.begin(), fixture.events.begin() + event_index(fixture, "factory"),
        "source.object_destroy") == 2, "both object destructors must finish before the recovery factory runs");
    require(camera::CameraManager::instance()->camera_motion_state(fixture.recovery.motion)
            == camera::CameraMotionState::Playing,
        "old scene cleanup must not cancel recovery motion in the shared Main camera slot");
    fixture.manager.on_update(0.5);
    require(fixture.manager.current_scene_key() == 2 && fixture.target.constructions == 0,
        "request queued by the failed frame must not survive recovery");
    require(std::abs(camera::CameraManager::instance()->camera(camera::CameraSlot::Main).center().x - 50) < 0.001f,
        "recovery camera motion must continue from its own starting pose");
    fixture.manager.on_update(0.5);
    require(std::abs(camera::CameraManager::instance()->camera(camera::CameraSlot::Main).center().x - 100) < 0.001f
        && !camera::CameraManager::instance()->camera_motion_state(fixture.recovery.motion),
        "recovery camera motion must reach its target and complete normally");
    const auto output = logs.output.str();
    const auto first = output.find("primary update");
    require(first != std::string::npos && output.find("primary update", first + 1) == std::string::npos,
        "successful recovery must log its original failure once");
    require(fixture.manager.shutdown(), "recovered scene must shut down cleanly");
}

void cleanup_failures_block_recovery()
{
    for (const unsigned failures : {kDetach, kExit, kRemoval, kDetach | kExit | kRemoval})
    {
        Fixture fixture;
        fixture.source.cleanup_errors = failures;
        fixture.initialize();
        fixture.fail_update();
        require_isolated(fixture);
        require(fixture.manager.state() == SceneManagerState::Faulted
            && fixture.factory_calls == 0 && fixture.recovery.constructions == 0
            && fixture.observer.faults == 1 && fixture.observer.last.cleanup_failed,
            "any recovery cleanup failure must fault without invoking a recovery factory");
        require(fixture.source.detaches == 1 && fixture.source.exits == 1 && fixture.source.removals == 2,
            "cleanup must continue through every stage and object after earlier errors");
        const auto& failure = fixture.observer.last;
        require(failure.scene == 1 && failure.boundary == SceneBoundary::Update
            && failure.diagnostic.message == "primary update"
            && failure.diagnostic.origin.line() == fixture.source.primary_origin.line(),
            "cleanup errors must preserve the original failure identity and source");
        const auto report = diagnostic(failure);
        if (failures & kDetach) require(report.find("detach cleanup") != std::string::npos, "detach error retained");
        if (failures & kExit) require(report.find("exit cleanup") != std::string::npos, "exit error retained");
        if (failures & kRemoval) require(report.find("removal cleanup") != std::string::npos, "removal error retained");
        require(fixture.manager.shutdown() && fixture.manager.shutdown(), "completed isolation must leave idempotent shutdown");
        require(fixture.source.destructions == 1 && fixture.source.detaches == 1,
            "shutdown must not repeat failed-scene cleanup");
    }
}

void candidate_cleanup_failures_block_recovery()
{
    for (const bool attach_failure : {false, true})
    {
        Fixture fixture;
        fixture.source.fail_enter = !attach_failure;
        fixture.source.fail_attach = attach_failure;
        fixture.source.cleanup_errors = kDetach;
        fixture.initialize();
        fixture.manager.start({.target = 1});
        require_isolated(fixture);
        require(fixture.manager.state() == SceneManagerState::Faulted && fixture.factory_calls == 0
            && fixture.observer.last.cleanup_failed && fixture.source.detaches == 1,
            "candidate failure followed by detach rollback failure must forbid recovery");
        require(fixture.observer.last.boundary == (attach_failure ? SceneBoundary::Attach : SceneBoundary::Enter),
            "nested attach rollback must preserve the original candidate boundary");
        require(fixture.source.exits == 0 && fixture.source.removals == (attach_failure ? 0 : 2),
            "failed entry must retire owned objects without exiting an inactive candidate");
        require(fixture.manager.shutdown(), "candidate isolation leaves shutdown clean");
    }
}

void unavailable_recovery_routes()
{
    for (int scenario = 0; scenario < 4; ++scenario)
    {
        Fixture fixture;
        scene::SceneFailureRouteFactory factory;
        if (scenario != 0)
            factory = [&](const scene::SceneBoundaryFailure&) -> SceneRoute {
                ++fixture.factory_calls;
                require_isolated(fixture);
                if (scenario == 1) throw std::runtime_error("route factory failure");
                return {.target = scenario == 2 ? 1u : 99u};
            };
        fixture.initialize(std::move(factory));
        fixture.fail_update();
        require_isolated(fixture);
        require(fixture.manager.state() == SceneManagerState::Faulted && fixture.observer.faults == 1
            && fixture.factory_calls == (scenario == 0 ? 0 : 1) && fixture.recovery.constructions == 0,
            "missing, throwing, recursive or unregistered recovery routes must fault after isolation");
        if (scenario == 1 || scenario == 3)
            require(diagnostic(fixture.observer.last).find("Recovery trigger") != std::string::npos,
                "recovery route errors must include the original failure context");
        if (scenario == 3)
            require(fixture.observer.last.scene == 99, "invalid recovery destination must retain its attempted key");
        require(fixture.manager.shutdown(), "unavailable recovery route leaves shutdown clean");
    }
}

void target_construction_failure_preserves_healthy_source()
{
    for (const bool exit_failure : {false, true})
    {
        Fixture fixture;
        fixture.source.cleanup_errors = exit_failure ? kExit : 0;
        fixture.target.fail_construct = true;
        fixture.initialize();
        fixture.manager.start({.target = 1});
        fixture.manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 3}});
        fixture.manager.on_update(0);
        require(fixture.source.exits == 1 && fixture.source.detaches == 1,
            "target construction failure must still detach and exit the current source");
        if (exit_failure)
        {
            require_isolated(fixture);
            require(fixture.factory_calls == 0 && fixture.observer.last.scene == 3
                && fixture.observer.last.diagnostic.message == "target construction",
                "source cleanup failure must preserve the target construction error and block recovery");
        }
        else
        {
            require(fixture.manager.current_scene_key() == 2 && fixture.source.destructions == 0,
                "healthy source must remain cached after a different target fails construction");
            fixture.manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 1}});
            fixture.manager.on_update(0);
            require(fixture.source.constructions == 1 && fixture.source.enters == 2,
                "healthy cached source must remain reusable after recovery");
        }
        require(fixture.manager.shutdown(), "target construction recovery must shut down cleanly");
    }
}

void recovery_candidate_failures_do_not_recurse()
{
    for (const bool construction : {false, true})
    {
        Fixture fixture;
        fixture.recovery.fail_construct = construction;
        fixture.recovery.fail_enter = !construction;
        fixture.initialize();
        fixture.fail_update();
        require_isolated(fixture);
        require(fixture.factory_calls == 1 && fixture.observer.faults == 1
            && fixture.observer.last.scene == 2 && fixture.observer.last.boundary == SceneBoundary::Enter,
            "recovery candidate failure must fault once using the recovery scene identity");
        require(diagnostic(fixture.observer.last).find("primary update") != std::string::npos,
            "recovery candidate failure must retain its original trigger");
        require(fixture.recovery.destructions == (construction ? 0 : 1),
            "failed recovery candidate must release all completed scene instances");
        require(fixture.manager.shutdown(), "failed recovery candidate leaves shutdown clean");
    }
}

void cleanup_backend_failures_escape_after_isolation()
{
    for (const unsigned stage : {kDetach, kExit, kRemoval})
    {
        Fixture fixture;
        fixture.source.backend_error = stage;
        fixture.source.cleanup_errors = kDetach | kExit | kRemoval;
        fixture.initialize();
        bool caught = false;
        try { fixture.fail_update(); }
        catch (const core::RenderBackendError& error)
        {
            caught = true;
            const auto report = core::format_failure_diagnostic(error.failure().diagnostic, "RENDER", "render");
            require(report.find("primary update") != std::string::npos
                && report.find("detach cleanup") != std::string::npos
                && report.find("exit cleanup") != std::string::npos
                && report.find("removal cleanup") != std::string::npos,
                "backend cleanup failure must retain its trigger and other cleanup errors");
            require(error.failure().diagnostic.origin.line() == fixture.source.backend_origin.line(),
                "backend cleanup failure must preserve its source");
        }
        require_isolated(fixture);
        require(caught && fixture.factory_calls == 0 && fixture.observer.faults == 0
            && fixture.source.detaches == 1 && fixture.source.exits == 1 && fixture.source.removals == 2,
            "backend cleanup errors must finish all isolation work before escaping without scene recovery");
        require(fixture.manager.shutdown(), "backend isolation leaves shutdown clean");
    }
}

void recovery_backend_failure_escapes_with_trigger()
{
    Fixture fixture;
    fixture.recovery.backend_enter = true;
    fixture.initialize();
    bool caught = false;
    try { fixture.fail_update(); }
    catch (const core::RenderBackendError& error)
    {
        caught = true;
        const auto& failure = error.failure();
        const auto report = core::format_failure_diagnostic(failure.diagnostic, "RENDER", "render");
        require(failure.operation == "recovery enter backend"
            && failure.diagnostic.origin.line() == fixture.recovery.backend_origin.line()
            && std::string_view(failure.diagnostic.origin.file_name()) == fixture.recovery.backend_origin.file_name(),
            "recovery backend failure must retain its operation and source");
        require(report.find("Recovery trigger") != std::string::npos
            && report.find("primary update") != std::string::npos
            && report.find("key=1") != std::string::npos && report.find("key=2") != std::string::npos,
            "recovery backend failure must retain both scene identities and its original trigger");
        require(fixture.recovery.destructions == 1 && fixture.recovery.object_destructions == 2,
            "recovery candidate and its objects must be destroyed before backend failure escapes");
    }
    require_isolated(fixture);
    require(caught && fixture.factory_calls == 1 && fixture.observer.faults == 0
        && fixture.recovery.enters == 1 && fixture.recovery.exits == 0
        && fixture.recovery.detaches == 1 && fixture.recovery.removals == 2,
        "recovery backend failure must roll back the candidate without recursive recovery or duplicate exit");
    require(fixture.manager.shutdown(), "recovery backend rollback leaves shutdown clean");
}

void cleanup_classification_survives_transport_and_overflow()
{
    for (const bool overflow : {false, true})
    {
        scene::detail::SceneFailureCollector inner;
        const int count = overflow ? 20 : 1;
        for (int index = 0; index < count; ++index)
            inner.attempt(1, SceneBoundary::Update, "Primary", [] { throw std::runtime_error("primary"); });
        inner.attempt_cleanup(1, SceneBoundary::ObjectRemoval, "Cleanup", [] { throw std::runtime_error("secondary"); });
        scene::detail::SceneFailureCollector outer;
        outer.attempt(1, SceneBoundary::Update, "Nested", [&] { inner.rethrow_if_failed(); });
        const auto result = outer.finish();
        require(!result && result.error().cleanup_failed,
            "secondary cleanup classification must survive exception transport and bounded diagnostic overflow");
    }
    scene::detail::SceneFailureCollector primary_removal;
    primary_removal.attempt_cleanup(1, SceneBoundary::ObjectRemoval, "Removal", [] { throw std::runtime_error("primary removal"); });
    const auto result = primary_removal.finish();
    require(!result && !result.error().cleanup_failed,
        "a primary cleanup boundary failure alone must remain eligible for recovery");
}
}

int main()
{
    tools::LoggerConfig config;
    config.console_color_mode = tools::ConsoleColorMode::Never;
    require(tools::Logger::instance()->configure(config), "logger configuration");
    recovery_order_and_camera();
    cleanup_failures_block_recovery();
    candidate_cleanup_failures_block_recovery();
    unavailable_recovery_routes();
    target_construction_failure_preserves_healthy_source();
    recovery_candidate_failures_do_not_recurse();
    cleanup_backend_failures_escape_after_isolation();
    recovery_backend_failure_escapes_with_trigger();
    cleanup_classification_survives_transport_and_overflow();
}
