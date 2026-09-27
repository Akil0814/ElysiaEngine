#define SDL_MAIN_HANDLED
#include "engine/scene/scene_manager.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/gameplay/collision/gameplay_collision_service.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "engine/object_query/game_object_query_service.h"
#include "engine/effects/effect_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/tools/logger.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <functional>
#include <sstream>

namespace
{
using namespace elysia;
using tests::require;
using scene::SceneTestAccess;

struct LogCapture
{
    std::ostringstream output;
    std::streambuf* previous = std::clog.rdbuf(output.rdbuf());
    ~LogCapture() { std::clog.rdbuf(previous); }
};

class Body final : public core::GameObject, public physics::PhysicsParticipant,
                   public physics::PhysicsStepParticipant
{
public:
    Body() : GameObject(core::DepthLayer::Item)
    {
        collider.shape = physics::AabbShape{{0, 0, 10, 10}};
        collider.response = physics::CollisionResponse::Overlap;
    }
    ~Body() override
    {
        require(physics_world() == nullptr, "body must be unbound before destruction");
        if (on_destroy) on_destroy();
    }
    physics::BodyDefinition body_definition() const override
    {
        physics::BodyDefinition definition;
        definition.gravity_scale = 0;
        return definition;
    }
    std::span<const physics::Collider> collider_definitions() const override
    {
        require(!definitions_consumed, "removal must not read collider definitions again");
        definitions_consumed = true;
        return {&collider, 1};
    }
    void fixed_update(double) override { if (die_in_fixed) destroy(); }
    bool die_in_fixed = false;
    std::function<void()> on_destroy;
    physics::Collider collider;
    mutable bool definitions_consumed = false;
};

class CascadeScene final : public scene::Scene
{
public:
    CascadeScene() : Scene({.fixed_step = scene::FixedStepConfig{},
                           .physics = physics::PhysicsWorldConfig{}}) {}
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    auto& world() { return physics_world(); }
    core::SceneObject *first = nullptr, *second = nullptr, *ui_first = nullptr, *ui_second = nullptr;
    std::array<int, 4> removals{};
    void on_scene_object_removing(core::SceneObject& object) override
    {
        if (&object == ui_first)
        {
            ++removals[0]; first->destroy();
            throw std::runtime_error("cascade primary");
        }
        if (&object == first) { ++removals[1]; object.reset(); }
        if (&object == ui_second) { ++removals[2]; second->destroy(); }
        if (&object == second)
        {
            ++removals[3];
            object.reset();
            throw std::runtime_error("cascade secondary");
        }
    }
};

void cascade_retirement()
{
    CascadeScene scene;
    SceneTestAccess::enter(scene);
    int destructions = 0;
    auto* first = scene.create_and_add_object<Body>();
    auto* second = scene.create_and_add_object<Body>();
    scene.first = first; scene.second = second;
    scene.ui_first = scene.create_and_add_object<ui::UiElement>();
    scene.ui_second = scene.create_and_add_object<ui::UiElement>();
    first->on_destroy = [&] {
        ++destructions;
        require(first->is_destroyed(), "reset must not revive an object in the retirement batch");
        scene.ui_second->destroy();
    };
    second->on_destroy = [&] { ++destructions; };
    scene.ui_first->destroy();
    LogCapture logs;
    bool caught = false;
    try { SceneTestAccess::update(scene, 0); }
    catch (const scene::SceneBoundaryRuntimeError& error)
    {
        caught = error.scene_boundary() == scene::SceneBoundary::ObjectRemoval
            && std::string(error.what()) == "cascade primary";
    }
    require(caught && destructions == 2 && scene.removals == std::array<int, 4>{1, 1, 1, 1},
        "all callback and destructor cascades must retire exactly once despite reset and exceptions");
    require(scene.world().registered_object_count() == 0, "retirement must leave no stale physics owners");
    require(logs.output.str().find("ObjectRemoval callback: cascade secondary") != std::string::npos,
        "later batch failures must be diagnosed without replacing the primary failure");
    SceneTestAccess::update(scene, 1.0 / 60);
    SceneTestAccess::exit(scene);
}

class CollisionScene final : public gameplay::GameplayScene, private physics::ICollisionListener
{
public:
    CollisionScene() : GameplayScene({.physics = physics::PhysicsWorldConfig{}, .gameplay_collision = true})
    { require(physics_world().add_listener(*this), "collision listener attaches"); }
    ~CollisionScene() override { (void)physics_world().remove_listener(*this); }
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    auto& collisions() { return collision_runtime(); }
    auto& world() { return physics_world(); }
    Body* victim = nullptr;
    bool die_in_update = false, die_in_collision = false;
    int collision_calls = 0;
    void on_before_update(double) override { if (die_in_update) victim->destroy(); }
    void on_collision_event(const physics::CollisionEvent&) override
    {
        ++collision_calls;
        if (die_in_collision) victim->destroy();
    }
};

void collision_retirement()
{
    for (int phase = 0; phase != 3; ++phase)
    {
        CollisionScene scene;
        SceneTestAccess::enter(scene);
        auto* body = scene.create_and_add_object<Body>();
        scene.victim = body;
        gameplay::collision::ActorCollisionRig rig{.owner = 1, .team = 1, .body = body->physics_collider(0)};
        require(scene.collisions().bind_actor(rig), "initial actor binds");
        scene.die_in_update = phase == 0;
        body->die_in_fixed = phase == 1;
        scene.die_in_collision = phase == 2;
        if (phase == 2) (void)scene.create_and_add_object<Body>();
        // Two steps also exercise physical removal before scene retirement for collision deaths.
        SceneTestAccess::update(scene, 2.0 / 60);
        require(phase != 2 || scene.collision_calls > 0, "collision death must execute a real callback");
        auto* replacement = scene.create_and_add_object<Body>();
        rig.body = replacement->physics_collider(0);
        require(scene.collisions().bind_actor(rig), "destroyed actor ID must be reusable in every update phase");
        scene.die_in_update = scene.die_in_collision = false;
        SceneTestAccess::exit(scene);
    }
}

class EmptyScene : public scene::Scene
{
public:
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};

class FailingScene final : public gameplay::GameplayScene
{
public:
    explicit FailingScene(bool& fail) : GameplayScene({.physics = physics::PhysicsWorldConfig{},
        .gameplay_collision = true}), fail(fail) {}
    void on_enter(const scene::ScenePayload&) override
    { if (fail) throw std::runtime_error("enter primary"); }
    void on_exit() override {}
    void on_reset() override {}
    bool& fail;
};

void require_services_unbound()
{
    require(!object_query::GameObjectQueryService::instance()->is_available(), "query runtime must be unbound");
    require(!gameplay::collision::GameplayCollisionService::instance()->has_active_runtime(),
        "gameplay collision runtime must be unbound");
    LogCapture logs;
    require(!effects::EffectService::instance()->request_animation_effect({.effect_key = "missing"}),
        "effect request must fail without an active scene");
    require(logs.output.str().find("there is no active scene") != std::string::npos,
        "effect manager must have no active scene, not merely an unknown effect definition");
}

std::size_t occurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    for (std::size_t pos = 0; (pos = text.find(needle, pos)) != std::string::npos; pos += needle.size()) ++count;
    return count;
}

void failed_entry_services()
{
    for (int scenario = 0; scenario != 3; ++scenario)
    {
        bool fail = scenario != 2, recovery_fail = true;
        io::ContentRegistry registry;
        scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
        scene::SceneManager manager;
        scene::SceneFailureRouteFactory recovery;
        if (scenario == 1) recovery = [](const auto&) { return scene::SceneRoute{.target = 2}; };
        manager.initialize(context, recovery);
        manager.register_game_scene<FailingScene>(1, std::ref(fail));
        manager.register_game_scene<FailingScene>(2, std::ref(recovery_fail));
        manager.register_game_scene<EmptyScene>(3);
        LogCapture logs;
        manager.start({.target = 1});
        if (scenario == 2)
        {
            manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 3}});
            manager.on_update(0);
            fail = true;
            manager.on_scene_request({.type = scene::SceneRequestType::Switch, .route = {.target = 1}});
            manager.on_update(0);
        }
        require(manager.state() == scene::SceneManagerState::Faulted, "failed entry without usable recovery faults");
        require_services_unbound();
        require(manager.shutdown(), "failure rollback leaves shutdown clean");
        require_services_unbound();
        const auto output = logs.output.str();
        require(occurrences(output, "SceneKey 1 boundary Enter: enter primary") == 1,
            "primary failure must be logged exactly once with key, boundary and message");
        require(occurrences(output, "SceneKey 2 boundary Enter: enter primary") == (scenario == 1 ? 1 : 0),
            "recovery failure must receive one separate diagnostic");
    }
}

class CleanupFailureScene final : public EmptyScene
{
public:
    void on_enter(const scene::ScenePayload&) override { throw std::runtime_error("original enter"); }
    void on_runtime_detach() override { throw std::runtime_error("secondary detach"); }
};

void secondary_failure_logging()
{
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<CleanupFailureScene>(1);
    LogCapture logs;
    manager.start({.target = 1});
    require_services_unbound();
    const auto output = logs.output.str();
    require(occurrences(output, "SceneKey 1 boundary Enter: original enter") == 1,
        "cleanup errors must not replace the original failure");
    require(occurrences(output, "Candidate detach: secondary detach") == 1,
        "secondary detach failure must be logged once with its stage");
    require(manager.shutdown(), "secondary candidate failure must not leave attached services");
}
} // namespace

int main()
{
    tools::LoggerConfig config;
    config.console_color_mode = tools::ConsoleColorMode::Never;
    require(tools::Logger::instance()->configure(config), "logger configuration");
    cascade_retirement();
    collision_retirement();
    failed_entry_services();
    secondary_failure_logging();
}
