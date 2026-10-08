#define SDL_MAIN_HANDLED

#include "engine/scene/scene.h"
#include "engine/physics/contracts/collision_listener.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
using namespace elysia;
using scene::SceneTestAccess;
using tests::require;

class TestScene final : public scene::Scene, private physics::ICollisionListener
{
public:
    TestScene()
        : Scene({.fixed_step = scene::FixedStepConfig{},
                 .physics = physics::PhysicsWorldConfig{},
                 .camera = scene::CameraSceneConfig{}})
    {
        require(physics_world().add_listener(*this), "collision listener registration");
    }
    ~TestScene() override { (void)physics_world().remove_listener(*this); }

    void clear() { clear_scene_objects(); }
    void visit(const object_query::GameObjectVisitor& visitor) const
    {
        static_cast<const object_query::IGameObjectQueryRuntime&>(*this)
            .visit_game_objects(core::DepthLayerMask::all(), visitor);
    }
    bool owns(const core::SceneObject& object) const { return owns_object(object); }
    std::size_t body_count() const { return physics_world().registered_object_count(); }

    std::function<void()> before, after, fixed, collision;
    std::function<void(core::SceneObject&)> registered, removing;

private:
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
    void on_before_update(double) override { if (before) before(); }
    void on_after_update(double) override { if (after) after(); }
    void on_fixed_update(std::uint64_t, double) override { if (fixed) fixed(); }
    void on_collision_event(const physics::CollisionEvent&) override { if (collision) collision(); }
    void on_scene_object_registered(core::SceneObject& object) override
    {
        if (registered) registered(object);
    }
    void on_scene_object_removing(core::SceneObject& object) override
    {
        if (removing) removing(object);
    }
};

class WorldProbe : public core::GameObject, public core::Updatable
{
public:
    WorldProbe() : GameObject(core::DepthLayer::Item) {}
    ~WorldProbe() override { if (destruction) destruction(); }
    void update(double) override { if (updating) updating(); }
    void submit_render_commands(std::vector<core::RenderCommand>&) const override
    {
        if (rendering) rendering();
    }
    std::function<void()> updating, rendering, destruction;
};

class UiProbe final : public ui::UiElement, public ui::UiInputFrameReceiver,
                      public ui::UiInputEventReceiver
{
public:
    void update_presentation_animations(double) override { if (presentation) presentation(); }
    void submit_ui_render_commands(std::vector<core::UiRenderCommand>&) const override
    {
        if (rendering) rendering();
    }
    void on_ui_input_frame(const ui::UiInputFrame&) override { if (frame) frame(); }
    bool on_ui_input_event(const ui::UiInputEvent&) override
    {
        if (event) event();
        return false;
    }
    void cancel_input_interaction() override
    {
        // Cancellation is allowed to delete this root, as real HUD widgets do.
        const auto callback = cancellation;
        if (callback) callback();
    }
    std::function<void()> presentation, rendering, frame, event, cancellation;
};

class BodyProbe final : public core::GameObject, public physics::PhysicsParticipant,
                        public physics::PhysicsStepParticipant
{
public:
    BodyProbe() : GameObject(core::DepthLayer::Item)
    {
        collider.shape = physics::AabbShape{{0, 0, 10, 10}};
        collider.response = physics::CollisionResponse::Overlap;
    }
    physics::BodyDefinition body_definition() const override
    {
        physics::BodyDefinition definition;
        definition.gravity_scale = 0;
        return definition;
    }
    std::span<const physics::Collider> collider_definitions() const override { return {&collider, 1}; }
    void fixed_update(double) override { if (stepping) stepping(); }
    physics::Collider collider;
    std::function<void()> stepping;
};

class CountedProbe final : public core::GameObject
{
public:
    CountedProbe(int& constructions, int& destructions)
        : GameObject(core::DepthLayer::Item), destroyed(destructions)
    {
        ++constructions;
    }
    ~CountedProbe() override { ++destroyed; }
    int& destroyed;
};

struct SoftwareRenderer
{
    SoftwareRenderer()
    {
        require(SDL_Init(0), "SDL initialization");
        surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
        require(surface != nullptr, "software render surface");
        renderer = SDL_CreateSoftwareRenderer(surface);
        require(renderer != nullptr, "software renderer");
    }
    ~SoftwareRenderer()
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        SDL_Quit();
    }
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
};

template <typename Callback>
void expect_rejection(Callback&& callback, scene::SceneBoundary boundary, std::string_view phase)
{
    bool rejected = false;
    try { callback(); }
    catch (const std::logic_error& error)
    {
        const auto* tagged = dynamic_cast<const scene::SceneBoundaryTagged*>(&error);
        const std::string_view message(error.what());
        rejected = tagged && tagged->scene_boundary() == boundary
            && message.find(phase) != std::string_view::npos
            && message.find("on_before_update()") != std::string_view::npos
            && message.find("on_after_update()") != std::string_view::npos;
    }
    require(rejected, "unsafe mutation must report its boundary, phase, and safe hooks");
}

enum class Phase { Update, Presentation, WorldRender, UiRender, Query, PhysicsStep, Collision };

const char* phase_name(Phase phase)
{
    switch (phase)
    {
    case Phase::Update: return "object update";
    case Phase::Presentation: return "UI presentation update";
    case Phase::WorldRender: return "world render";
    case Phase::UiRender: return "UI render";
    case Phase::Query: return "object query";
    case Phase::PhysicsStep:
    case Phase::Collision: return "physics step";
    }
    return "unknown";
}

void run_phase(TestScene& scene, WorldProbe& world, UiProbe& ui, BodyProbe& body,
               Phase phase, SDL_Renderer* renderer, const std::function<void()>& callback)
{
    switch (phase)
    {
    case Phase::Update: world.updating = callback; break;
    case Phase::Presentation: ui.presentation = callback; break;
    case Phase::WorldRender: world.rendering = callback; break;
    case Phase::UiRender: ui.rendering = callback; break;
    case Phase::PhysicsStep: body.stepping = callback; break;
    case Phase::Collision: scene.collision = callback; break;
    case Phase::Query:
        scene.visit([&](core::GameObject&) { callback(); return false; });
        return;
    }
    if (phase == Phase::WorldRender || phase == Phase::UiRender)
        SceneTestAccess::render(scene, renderer);
    else
        SceneTestAccess::update(scene, 1.0 / 60.0);
}

void test_dangerous_phases(SDL_Renderer* renderer)
{
    for (const auto phase : {Phase::Update, Phase::Presentation, Phase::WorldRender,
             Phase::UiRender, Phase::Query, Phase::PhysicsStep, Phase::Collision})
    {
        int constructions = 0, destructions = 0;
        TestScene scene;
        SceneTestAccess::enter(scene);
        auto* world = scene.create_and_add_object<WorldProbe>();
        auto* ui = scene.create_and_add_object<UiProbe>();
        auto* body = scene.create_and_add_object<BodyProbe>();
        (void)scene.create_and_add_object<BodyProbe>();
        int callbacks = 0, registrations = 0;
        scene.registered = [&](core::SceneObject&) { ++registrations; };

        run_phase(scene, *world, *ui, *body, phase, renderer, [&] {
            ++callbacks;
            const auto name = phase_name(phase);
            const int previous_constructions = constructions;
            expect_rejection([&] { scene.create_and_add_object<CountedProbe>(constructions, destructions); },
                scene::SceneBoundary::ObjectRegistration, name);
            require(constructions == previous_constructions,
                "create_and_add must reject before running the object constructor");

            auto owned = std::make_unique<CountedProbe>(constructions, destructions);
            expect_rejection([&] { scene.add_object(std::move(owned)); },
                scene::SceneBoundary::ObjectRegistration, name);
            require(constructions == destructions && registrations == 0,
                "add_object rejection must release the incoming object without registering it");

            expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval, name);
            require(!world->is_destroyed() && !ui->is_destroyed() && !body->is_destroyed()
                    && scene.owns(*world) && scene.owns(*ui) && scene.body_count() == 2,
                "rejected clear must preserve flags, ownership, and physics registration");
        });
        require(callbacks > 0, "each guarded phase must actually invoke its callback");
        require(scene.create_and_add_object<WorldProbe>() != nullptr,
            "successful traversal must release its mutation scope");
        scene.clear();
        require(scene.body_count() == 0, "safe clear must release physics objects");
        SceneTestAccess::exit(scene);
    }
}

void test_exception_unwinds_every_phase(SDL_Renderer* renderer)
{
    for (const auto phase : {Phase::Update, Phase::Presentation, Phase::WorldRender,
             Phase::UiRender, Phase::Query, Phase::PhysicsStep, Phase::Collision})
    {
        TestScene scene;
        SceneTestAccess::enter(scene);
        auto* world = scene.create_and_add_object<WorldProbe>();
        auto* ui = scene.create_and_add_object<UiProbe>();
        auto* body = scene.create_and_add_object<BodyProbe>();
        (void)scene.create_and_add_object<BodyProbe>();
        expect_rejection([&] {
            run_phase(scene, *world, *ui, *body, phase, renderer,
                [&] { (void)scene.create_and_add_object<WorldProbe>(); });
        }, scene::SceneBoundary::ObjectRegistration, phase_name(phase));
        require(scene.create_and_add_object<WorldProbe>() != nullptr,
            "an exception must unwind the mutation scope");
        scene.clear();
        SceneTestAccess::exit(scene);
    }
}

void test_nested_queries_and_internal_retirement()
{
    TestScene scene;
    SceneTestAccess::enter(scene);
    auto* world = scene.create_and_add_object<WorldProbe>();
    int destructions = 0;
    world->destruction = [&] { ++destructions; };
    scene.visit([&](core::GameObject&) {
        scene.visit([](core::GameObject&) { return false; });
        expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval, "object query");
        try
        {
            scene.visit([](core::GameObject&) -> bool { throw std::runtime_error("query callback"); });
        }
        catch (const std::runtime_error&) {}
        expect_rejection([&] { (void)scene.create_and_add_object<WorldProbe>(); },
            scene::SceneBoundary::ObjectRegistration, "object query");
        world->destroy();
        expect_rejection([&] { SceneTestAccess::update(scene, 0); },
            scene::SceneBoundary::ObjectRemoval, "object query");
        require(destructions == 0, "nested lifecycle update cannot retire an object being visited");
        return false;
    });
    SceneTestAccess::update(scene, 0);
    require(destructions == 1, "destroy marks remain usable and retire at the next safe point");
    require(scene.create_and_add_object<WorldProbe>() != nullptr, "nested scopes must fully unwind");
    scene.clear();
    SceneTestAccess::exit(scene);
}

void test_registration_and_rollback_guards()
{
    TestScene scene;
    auto* sentinel = scene.create_and_add_object<WorldProbe>();
    int registrations = 0;
    scene.registered = [&](core::SceneObject& object) {
        ++registrations;
        expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval, "object registration");
        require(!object.is_destroyed() && !sentinel->is_destroyed(),
            "registration-time clear rejection must not mark any object");
        if (registrations == 1)
            require(scene.create_and_add_object<WorldProbe>() != nullptr,
                "registration callbacks must still allow nested additions");
    };
    require(scene.create_and_add_object<WorldProbe>() != nullptr && registrations == 2,
        "outer and nested registrations must both finish");

    int rollback_destructions = 0;
    auto failed = std::make_unique<WorldProbe>();
    auto* failed_object = failed.get();
    int rollback_removals = 0, attempted_constructions = 0, attempted_destructions = 0;
    bool removal_guard_checks_finished = false;
    scene.removing = [&](core::SceneObject& object) {
        ++rollback_removals;
        require(&object == failed_object,
            "registration rollback must notify removal only for its failed object");
        expect_rejection([&] {
            scene.create_and_add_object<CountedProbe>(attempted_constructions, attempted_destructions);
        }, scene::SceneBoundary::ObjectRegistration, "object registration rollback");
        require(attempted_constructions == 0,
            "rollback removal callbacks must reject creation before its constructor runs");
        auto incoming = std::make_unique<CountedProbe>(attempted_constructions, attempted_destructions);
        expect_rejection([&] { scene.add_object(std::move(incoming)); },
            scene::SceneBoundary::ObjectRegistration, "object registration rollback");
        require(attempted_constructions == 1 && attempted_destructions == 1,
            "rollback removal callbacks must reject and release incoming ownership without registration");
        expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval,
            "object registration rollback");
        require(!sentinel->is_destroyed(),
            "rollback removal callback rejection must preserve unrelated objects");
        removal_guard_checks_finished = true;
    };
    failed->destruction = [&] {
        ++rollback_destructions;
        expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval,
            "object registration rollback");
        expect_rejection([&] { scene.create_and_add_object<WorldProbe>(); },
            scene::SceneBoundary::ObjectRegistration, "object registration rollback");
    };
    scene.registered = [](core::SceneObject&) { throw std::runtime_error("registration failure"); };
    bool failed_as_expected = false;
    try { scene.add_object(std::move(failed)); }
    catch (const std::runtime_error&) { failed_as_expected = true; }
    require(failed_as_expected && rollback_destructions == 1 && !sentinel->is_destroyed(),
        "failed registration must retire only its own object and preserve its original failure");
    require(rollback_removals == 1 && removal_guard_checks_finished,
        "the rollback removal callback must complete every mutation rejection check");
    scene.registered = {};
    scene.removing = {};
    scene.clear();
    require(scene.create_and_add_object<WorldProbe>() != nullptr,
        "registration rollback must release every mutation scope");
    scene.clear();
}

void test_safe_hooks_and_input_mutation()
{
    for (int hook = 0; hook < 3; ++hook)
    {
        TestScene scene;
        SceneTestAccess::enter(scene);
        (void)scene.create_and_add_object<WorldProbe>();
        int calls = 0;
        const auto callback = [&] {
            ++calls;
            scene.clear();
            require(scene.create_and_add_object<BodyProbe>() != nullptr,
                "scene update hooks must support synchronous clear and creation");
        };
        if (hook == 0) scene.before = callback;
        if (hook == 1) scene.after = callback;
        if (hook == 2) scene.fixed = callback;
        SceneTestAccess::update(scene, 1.0 / 60.0);
        require(calls == 1 && scene.body_count() == 1, "each safe update hook must run");
        scene.clear();
        SceneTestAccess::exit(scene);
    }

    TestScene scene;
    SceneTestAccess::enter(scene);
    auto* original = scene.create_and_add_object<UiProbe>();
    int added_frames = 0, added_events = 0;
    bool added = false;
    original->frame = [&] {
        if (std::exchange(added, true)) return;
        scene.create_and_add_object<UiProbe>()->frame = [&] { ++added_frames; };
    };
    SceneTestAccess::dispatch_ui_frame(scene, {});
    require(added_frames == 0, "new frame receivers must wait for the next snapshot");
    SceneTestAccess::dispatch_ui_frame(scene, {});
    require(added_frames == 1, "input callback additions must remain allowed");
    original->event = [&] {
        if (!std::exchange(added, true))
            scene.create_and_add_object<UiProbe>()->event = [&] { ++added_events; };
    };
    added = false;
    (void)SceneTestAccess::dispatch_ui_events(scene, std::vector<ui::UiInputEvent>(2));
    require(added_events == 0, "new event receivers must wait for the next batch");
    (void)SceneTestAccess::dispatch_ui_events(scene, std::vector<ui::UiInputEvent>(2));
    require(added_events == 2, "event callback additions must remain allowed");
    original->cancellation = [&] { scene.clear(); };
    scene.pause();
    require(scene.create_and_add_object<UiProbe>() != nullptr,
        "root cancellation must retain its supported immediate-clear behavior");
    scene.clear();
    SceneTestAccess::exit(scene);
}

void test_reentrant_clear_has_no_marking_side_effect()
{
    TestScene scene;
    SceneTestAccess::enter(scene);
    auto* victim = scene.create_and_add_object<WorldProbe>();
    auto* survivor = scene.create_and_add_object<WorldProbe>();
    int removals = 0;
    scene.removing = [&](core::SceneObject&) {
        ++removals;
        expect_rejection([&] { scene.clear(); }, scene::SceneBoundary::ObjectRemoval, "object retirement");
        require(!survivor->is_destroyed(), "reentrant clear must reject before marking surviving objects");
    };
    victim->destroy();
    SceneTestAccess::update(scene, 0);
    require(removals == 1 && scene.owns(*survivor), "retirement must preserve unmarked survivors");
    scene.removing = {};
    scene.clear();
    SceneTestAccess::exit(scene);
}
} // namespace

int main()
{
    SoftwareRenderer renderer;
    test_dangerous_phases(renderer.renderer);
    test_exception_unwinds_every_phase(renderer.renderer);
    test_nested_queries_and_internal_retirement();
    test_registration_and_rollback_guards();
    test_safe_hooks_and_input_mutation();
    test_reentrant_clear_has_no_marking_side_effect();
    return 0;
}
