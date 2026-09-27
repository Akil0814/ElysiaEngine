#define SDL_MAIN_HANDLED

#include "engine/io/loaders/asset_config_types.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/camera/camera_manager.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/scene/scene.h"
#include "engine/scene/routing/scene_key.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/routing/scene_payload.h"
#include "engine/scene/routing/scene_route.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>

namespace
{
using elysia::tests::require;

struct RoutePayload
{
    int value = 0;
};

struct ProbeState
{
    int constructions = 0;
    int destructions = 0;
    int enters = 0;
    int exits = 0;
    int resets = 0;
    int payload_value = 0;
    int logical_width = 0;
    int logical_height = 0;
    const elysia::io::ContentRegistry* registry = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool context_cleared_before_destruction = true;
};

template <int Id>
class ProbeScene final : public elysia::scene::Scene
{
public:
    ProbeScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .fixed_step = elysia::scene::FixedStepConfig{},
              .physics = elysia::physics::PhysicsWorldConfig{}})
    {
        last_instance = this;
        ++state.constructions;
    }

    ~ProbeScene() override
    {
        try
        {
            (void)runtime_context();
            state.context_cleared_before_destruction = false;
        }
        catch (const std::logic_error&)
        {
        }

        if (last_instance == this)
            last_instance = nullptr;
        ++state.destructions;
    }

    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const auto* route_payload = elysia::scene::try_scene_payload<RoutePayload>(payload);
        require(route_payload != nullptr, "probe scene must receive the expected route payload");

        const elysia::scene::SceneRuntimeContext& context = runtime_context();
        ++state.enters;
        state.payload_value = route_payload->value;
        state.logical_width = context.logical_width();
        state.logical_height = context.logical_height();
        state.registry = &context.content_registry();
        state.renderer = context.renderer();
    }

    void on_exit() override
    {
        // The runtime context remains usable through on_exit; SceneManager
        // clears it immediately before destroying the cached instance.
        (void)runtime_context();
        ++state.exits;
    }

    void on_reset() override
    {
        ++state.resets;
    }

    void emit_route(const elysia::scene::SceneRoute& route)
    {
        request_scene_switch(route);
    }

    const elysia::scene::SceneRuntimeContext& exposed_runtime_context() const
    {
        return runtime_context();
    }

    elysia::physics::PhysicsWorld& exposed_physics_world()
    {
        return physics_world();
    }

    static inline ProbeState state{};
    static inline ProbeScene* last_instance = nullptr;
};

using FirstProbeScene = ProbeScene<1>;
using SecondProbeScene = ProbeScene<2>;

struct ConstructorDependency
{
    int marker = 0;
};

class ConstructorProbeScene final : public elysia::scene::Scene
{
public:
    ConstructorProbeScene(
        const ConstructorDependency& dependency,
        int registered_value)
        : _dependency(&dependency)
        , _registered_value(registered_value)
    {
        ++constructions;
        received_dependency = _dependency;
        received_value = _registered_value;
    }

    ~ConstructorProbeScene() override
    {
        ++destructions;
    }

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        ++enters;
        received_dependency = _dependency;
        received_value = _registered_value;
    }

    void on_exit() override {}
    void on_reset() override {}

    static inline int constructions = 0;
    static inline int destructions = 0;
    static inline int enters = 0;
    static inline int received_value = 0;
    static inline const ConstructorDependency* received_dependency = nullptr;

private:
    const ConstructorDependency* _dependency = nullptr;
    int _registered_value = 0;
};

class DefaultGameplayProbeScene final : public elysia::gameplay::GameplayScene
{
public:
    DefaultGameplayProbeScene() { instance = this; }
    ~DefaultGameplayProbeScene() override
    {
        if (instance == this)
            instance = nullptr;
    }

    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}

    [[nodiscard]] bool collision_available() noexcept
    {
        return try_collision_runtime() != nullptr;
    }

    static inline DefaultGameplayProbeScene* instance = nullptr;
    static inline int fixed_updates = 0;
    static inline std::uint64_t last_tick = 0;

protected:
    void on_game_fixed_update(std::uint64_t tick, double) override
    {
        ++fixed_updates;
        last_tick = tick;
    }
};

class ParticipantProbe final : public elysia::core::GameObject,
                               public elysia::physics::PhysicsParticipant
{
public:
    ParticipantProbe() : GameObject(elysia::core::DepthLayer::Item) {}
    ~ParticipantProbe() override
    {
        ++destructions;
        was_unbound_at_destruction = physics_world() == nullptr;
    }

    std::span<const elysia::physics::Collider> collider_definitions() const override
    {
        return {};
    }

    static inline int destructions = 0;
    static inline bool was_unbound_at_destruction = false;
};

class RetirementProbeScene final : public elysia::scene::Scene
{
public:
    RetirementProbeScene()
        : Scene(elysia::scene::SceneRuntimeFeatures{
              .fixed_step = elysia::scene::FixedStepConfig{},
              .physics = elysia::physics::PhysicsWorldConfig{}})
    {
    }

    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}

    ParticipantProbe* add_participant()
    {
        return create_and_add_object<ParticipantProbe>();
    }

    elysia::ui::UiElement* add_ui()
    {
        return create_and_add_object<elysia::ui::UiElement>();
    }

    int removals = 0;
    bool throw_from_first_removal = false;

protected:
    void on_scene_object_removing(elysia::core::SceneObject&) override
    {
        ++removals;
        if (throw_from_first_removal && removals == 1)
            throw std::runtime_error("injected retirement failure");
    }
};

class NoPhysicsProbeScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}

    ParticipantProbe* add_participant()
    {
        return create_and_add_object<ParticipantProbe>();
    }
};

class KeyedProbeScene final : public elysia::scene::Scene
{
public:
    explicit KeyedProbeScene(int identity) : _identity(identity)
    {
        ++constructions[_identity];
    }

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        ++enters[_identity];
    }
    void on_exit() override {}
    void on_reset() override {}

    static inline std::array<int, 2> constructions{};
    static inline std::array<int, 2> enters{};

private:
    int _identity = 0;
};

struct BoundaryProbeState
{
    elysia::scene::SceneBoundary trigger = elysia::scene::SceneBoundary::Update;
    bool removal_started = false;
    int recovery_enters = 0;
};

class ThrowingRenderProbe final : public elysia::core::GameObject
{
public:
    explicit ThrowingRenderProbe(BoundaryProbeState& state)
        : GameObject(elysia::core::DepthLayer::Item), _state(state)
    {
    }

    void submit_render_commands(std::vector<elysia::core::RenderCommand>&) const override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Render)
            throw std::runtime_error("injected render failure");
    }

private:
    BoundaryProbeState& _state;
};

class BoundaryFailureScene final : public elysia::scene::Scene
{
public:
    explicit BoundaryFailureScene(BoundaryProbeState& state) : _state(state) {}

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        using elysia::scene::SceneBoundary;
        if (_state.trigger == SceneBoundary::Enter)
            throw std::runtime_error("injected enter failure");
        if (_state.trigger == SceneBoundary::ObjectRegistration)
            (void)create_and_add_object<ParticipantProbe>();
        if (_state.trigger == SceneBoundary::Render)
            (void)create_and_add_object<ThrowingRenderProbe>(_state);
        if (_state.trigger == SceneBoundary::ObjectRemoval)
            _removal_target = create_and_add_object<elysia::ui::UiElement>();
    }

    void on_exit() override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Exit)
            throw std::runtime_error("injected exit failure");
    }

    void on_reset() override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Reset)
            throw std::runtime_error("injected reset failure");
    }

protected:
    void on_runtime_attach() override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Attach)
            throw std::runtime_error("injected attach failure");
    }

    void on_runtime_detach() override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Detach)
            throw std::runtime_error("injected detach failure");
    }

    void on_routed_input(const elysia::input::InputSnapshot&) override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Input)
            throw std::runtime_error("injected input failure");
    }

    void on_before_update(double) override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::Update)
            throw std::runtime_error("injected update failure");
        if (_state.trigger == elysia::scene::SceneBoundary::ObjectRemoval
            && !_state.removal_started)
        {
            _state.removal_started = true;
            _removal_target->destroy();
        }
    }

    void on_scene_object_removing(elysia::core::SceneObject&) override
    {
        if (_state.trigger == elysia::scene::SceneBoundary::ObjectRemoval)
            throw std::runtime_error("injected object removal failure");
    }

private:
    BoundaryProbeState& _state;
    elysia::ui::UiElement* _removal_target = nullptr;
};

class BoundaryRecoveryScene final : public elysia::scene::Scene
{
public:
    explicit BoundaryRecoveryScene(BoundaryProbeState& state) : _state(state) {}
    void on_enter(const elysia::scene::ScenePayload&) override { ++_state.recovery_enters; }
    void on_exit() override {}
    void on_reset() override {}

private:
    BoundaryProbeState& _state;
};

class AlwaysFailingRecoveryScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override
    {
        throw std::runtime_error("injected recovery failure");
    }
    void on_exit() override {}
    void on_reset() override {}
};

class SceneManagerObserverProbe final : public elysia::scene::SceneManagerObserver
{
public:
    void on_scene_manager_quit_requested() override {}
    void on_scene_manager_fault(const elysia::scene::SceneBoundaryFailure& failure) override
    {
        ++faults;
        last_failure = failure;
    }

    int faults = 0;
    elysia::scene::SceneBoundaryFailure last_failure{};
};

void reset_probe_states()
{
    FirstProbeScene::state = {};
    SecondProbeScene::state = {};
}

bool throws_logic_error_containing(
    const std::function<void()>& operation,
    const std::string& expected_text
)
{
    try
    {
        operation();
    }
    catch (const std::logic_error& error)
    {
        return std::string(error.what()).find(expected_text) != std::string::npos;
    }

    return false;
}

void test_scene_key_domains_and_payload_helpers()
{
    using namespace elysia::scene;

    static_assert(!SceneKeys::is_supported(SceneKeys::Invalid));
    static_assert(SceneKeys::is_game(1));
    static_assert(SceneKeys::is_game(999));
    static_assert(!SceneKeys::is_game(1000));
    static_assert(SceneKeys::is_reserved(1000));
    static_assert(SceneKeys::ElysiaRealm == 1111);
    static_assert(SceneKeys::is_engine_owned(1111));
    static_assert(SceneKeys::is_supported(1111));
    static_assert(!SceneKeys::is_reserved(1111));
    static_assert(SceneKeys::is_reserved(1110));
    static_assert(SceneKeys::is_reserved(SceneKeys::EngineMarker));
    static_assert(!SceneKeys::is_engine(SceneKeys::EngineMarker));
    static_assert(SceneKeys::is_engine(SceneKeys::EngineBegin));
    static_assert(elysia::builtin::SceneKeys::StartupLoading == 0xFFFF0001u);
    static_assert(elysia::builtin::SceneKeys::Settings == 0xFFFF0002u);

    const ScenePayload payload = RoutePayload{ 37 };
    const RoutePayload* found = try_scene_payload<RoutePayload>(payload);
    require(found && found->value == 37, "try_scene_payload must return the stored payload");
    require(try_scene_payload<int>(payload) == nullptr,
        "try_scene_payload must return null for a mismatched payload type");
    require(try_scene_payload<RoutePayload>(ScenePayload{}) == nullptr,
        "try_scene_payload must return null for an empty payload");
}

void test_registration_and_route_key_errors_are_distinct()
{
    using namespace elysia::scene;

    SceneManager manager;
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);
    manager.initialize(context);
    require(throws_logic_error_containing(
        [&manager] { manager.register_game_scene<FirstProbeScene>(0); },
        "game range"), "game registration must reject Invalid");
    require(throws_logic_error_containing(
        [&manager] { manager.register_game_scene<FirstProbeScene>(1000); },
        "game range"), "game registration must reject reserved keys");
    require(throws_logic_error_containing(
        [&manager] { manager.register_engine_scene<FirstProbeScene>(SceneKeys::EngineMarker); },
        "engine-owned keys"), "engine-owned registration must reject the engine marker");
    require(throws_logic_error_containing(
        [&manager] { manager.register_engine_scene<FirstProbeScene>(999); },
        "engine-owned keys"), "engine-owned registration must reject game keys");
    SceneManager easter_egg_manager;
    easter_egg_manager.initialize(context);
    easter_egg_manager.register_engine_scene<FirstProbeScene>(
        SceneKeys::ElysiaRealm);
    require(throws_logic_error_containing(
        [&easter_egg_manager] {
            easter_egg_manager.register_engine_scene<SecondProbeScene>(
                SceneKeys::ElysiaRealm);
        },
        "duplicate"), "the Elysia Easter egg key must use engine-owned registration");

    manager.register_game_scene<FirstProbeScene>(1);
    require(throws_logic_error_containing(
        [&manager] { manager.register_game_scene<SecondProbeScene>(1); },
        "duplicate"), "duplicate keys must be reported separately");

    require(throws_logic_error_containing(
        [&manager] { manager.start(SceneRoute{}); },
        "Invalid"), "route key zero must be reported as Invalid");
    require(throws_logic_error_containing(
        [&manager] { manager.start(SceneRoute{ .target = 1000 }); },
        "reserved range"), "reserved route keys must be reported separately");
    require(throws_logic_error_containing(
        [&manager] { manager.start(SceneRoute{ .target = 2 }); },
        "unregistered game"), "unregistered game keys must identify their domain");
    require(throws_logic_error_containing(
        [&manager] { manager.start(SceneRoute{
            .target = elysia::builtin::SceneKeys::Settings }); },
        "unregistered engine-owned"), "unregistered engine-owned keys must identify their domain");

    manager.register_engine_scene<SecondProbeScene>(
        elysia::builtin::SceneKeys::Settings);
    require(throws_logic_error_containing(
        [&manager] { manager.register_engine_scene<FirstProbeScene>(
            elysia::builtin::SceneKeys::Settings); },
        "duplicate"), "engine-owned registration must share duplicate-key protection");
}

void test_runtime_context_synchronizes_camera_viewports()
{
    using elysia::camera::CameraManager;
    using elysia::camera::CameraSlot;

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext full_hd_context(
        nullptr, registry, 1280, 720);
    elysia::scene::SceneManager manager;
    manager.initialize(full_hd_context);

    auto* cameras = CameraManager::instance();
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(CameraSlot::Count);
         ++index)
    {
        const auto slot = static_cast<CameraSlot>(index);
        require(cameras->camera(slot).viewport_size()
                == elysia::core::Vector2(1280.0f, 720.0f),
            "Scene runtime binding must initialize every camera viewport");
    }

    elysia::scene::SceneRuntimeContext resized_context(
        nullptr, registry, 960, 540);
    require(manager.shutdown(), "runtime context viewport test must stop cleanly");
    manager.initialize(resized_context);
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(CameraSlot::Count);
         ++index)
    {
        const auto slot = static_cast<CameraSlot>(index);
        require(cameras->camera(slot).viewport_size()
                == elysia::core::Vector2(960.0f, 540.0f),
            "Rebinding runtime context must refresh every camera viewport");
        cameras->reset(slot);
        require(cameras->camera(slot).viewport_size()
                == elysia::core::Vector2(960.0f, 540.0f),
            "Camera scene reset must preserve the synchronized viewport");
    }
}

void test_registration_arguments_are_reusable_for_recreate()
{
    using namespace elysia::scene;

    ConstructorProbeScene::constructions = 0;
    ConstructorProbeScene::destructions = 0;
    ConstructorProbeScene::enters = 0;
    ConstructorProbeScene::received_value = 0;
    ConstructorProbeScene::received_dependency = nullptr;

    ConstructorDependency dependency{ .marker = 73 };
    int registered_value = 41;

    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);
    SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<ConstructorProbeScene>(
        3,
        std::cref(dependency),
        registered_value);

    registered_value = 99;
    manager.start(SceneRoute{ .target = 3 });
    require(ConstructorProbeScene::constructions == 1
            && ConstructorProbeScene::enters == 1
            && ConstructorProbeScene::received_dependency == &dependency
            && ConstructorProbeScene::received_value == 41,
        "scene registration must copy values and explicitly borrow std::cref dependencies");

    manager.on_scene_request(SceneRequest{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 3,
            .reload_mode = SceneReloadMode::Recreate
        }
    });
    manager.on_update(0.0);
    require(ConstructorProbeScene::constructions == 2
            && ConstructorProbeScene::destructions == 1
            && ConstructorProbeScene::enters == 2
            && ConstructorProbeScene::received_dependency == &dependency
            && ConstructorProbeScene::received_value == 41,
        "Recreate must rebuild a non-default scene from the saved registration arguments");

    manager.shutdown();
}

void test_scene_maps_debug_draw_categories_to_physics_capture()
{
    using elysia::physics::PhysicsDebugCapture;
    using elysia::tools::DebugDrawCategory;

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    elysia::scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<FirstProbeScene>(1);
    manager.start({.target = 1, .payload = RoutePayload{0}});
    auto& scene = *FirstProbeScene::last_instance;
    auto* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(DebugDrawCategory::All);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None,
        "disabled DebugDraw must disable physics diagnostic capture");

    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(DebugDrawCategory::Gameplay);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None,
        "non-physics categories must not enable physics diagnostic capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsCollider);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Shapes,
        "collider drawing must request shape capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsBroadPhase);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::BroadPhase,
        "broad-phase drawing must request broad-phase capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsContactNormal);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Contacts,
        "contact-normal drawing must request contact capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsVelocity);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Velocities,
        "velocity drawing must request velocity capture");

    debug_draw->set_enabled(false);
    manager.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None
            && scene.exposed_physics_world().debug_snapshot().shapes.empty(),
        "turning DebugDraw off must clear the active physics snapshot");
    debug_draw->set_enabled_categories(DebugDrawCategory::All);
    manager.shutdown();
}

void test_route_copy_reload_modes_and_runtime_context_binding()
{
    using namespace elysia::scene;

    reset_probe_states();
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);

    SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<FirstProbeScene>(1);
    manager.register_game_scene<SecondProbeScene>(2);

    auto* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->clear();
    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(
        elysia::tools::DebugDrawCategory::Gameplay);
    debug_draw->draw_point(
        elysia::tools::DebugDrawCategory::Gameplay,
        elysia::core::Vector2{},
        4.0f,
        elysia::core::Color{}
    );

    manager.start(SceneRoute{
        .target = 1,
        .payload = RoutePayload{ 11 },
        .reload_mode = SceneReloadMode::Reset
    });

    require(debug_draw->commands().empty(),
        "starting a scene must clear retained debug draw commands");

    require(FirstProbeScene::state.enters == 1 && FirstProbeScene::state.resets == 1,
        "the initial route reload mode must reach SceneManager");
    require(FirstProbeScene::state.payload_value == 11,
        "the initial route payload must reach the first scene");
    require(FirstProbeScene::state.logical_width == 1280
        && FirstProbeScene::state.logical_height == 720
        && FirstProbeScene::state.registry == &registry,
        "SceneManager must bind runtime context before on_enter");

    SceneRoute copied_route{
        .target = 2,
        .payload = RoutePayload{ 22 },
        .reload_mode = SceneReloadMode::Reuse
    };
    FirstProbeScene::last_instance->emit_route(copied_route);
    debug_draw->draw_point(
        elysia::tools::DebugDrawCategory::Gameplay,
        elysia::core::Vector2{},
        4.0f,
        elysia::core::Color{}
    );
    copied_route.target = 999;
    copied_route.reload_mode = SceneReloadMode::Reset;
    std::any_cast<RoutePayload&>(copied_route.payload).value = 99;
    manager.on_update(0.0);

    require(debug_draw->commands().empty(),
        "switching scenes must clear the previous scene debug snapshot");

    require(SecondProbeScene::state.enters == 1 && SecondProbeScene::state.resets == 0,
        "pending requests must preserve the copied target and reload mode");
    require(SecondProbeScene::state.payload_value == 22,
        "pending requests must preserve a value copy of the route payload");

    SceneRequest reuse_first_request{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 1,
            .payload = RoutePayload{ 23 },
            .reload_mode = SceneReloadMode::Reuse
        }
    };
    manager.on_scene_request(reuse_first_request);
    manager.on_update(0.0);
    require(FirstProbeScene::state.constructions == 1
        && FirstProbeScene::state.enters == 2
        && FirstProbeScene::state.payload_value == 23,
        "Reuse must re-enter an existing cached scene without reconstructing it");

    SceneRequest reuse_second_request{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 2,
            .payload = RoutePayload{ 24 },
            .reload_mode = SceneReloadMode::Reuse
        }
    };
    manager.on_scene_request(reuse_second_request);
    manager.on_update(0.0);
    require(SecondProbeScene::state.constructions == 1
        && SecondProbeScene::state.enters == 2
        && SecondProbeScene::state.payload_value == 24,
        "Reuse must preserve the second cached scene and deliver its new payload");

    SceneRequest same_scene_reuse_request{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 2,
            .payload = RoutePayload{ 30 },
            .reload_mode = SceneReloadMode::Reuse
        }
    };
    manager.on_scene_request(same_scene_reuse_request);
    manager.on_update(0.0);
    require(SecondProbeScene::state.constructions == 1
        && SecondProbeScene::state.enters == 3
        && SecondProbeScene::state.resets == 0
        && SecondProbeScene::state.payload_value == 30,
        "Reuse targeting the active scene must re-enter it and deliver the new payload");

    SceneRequest reset_request{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 2,
            .payload = RoutePayload{ 33 },
            .reload_mode = SceneReloadMode::Reset
        }
    };
    manager.on_scene_request(reset_request);
    debug_draw->draw_point(
        elysia::tools::DebugDrawCategory::Gameplay,
        elysia::core::Vector2{},
        4.0f,
        elysia::core::Color{}
    );
    manager.on_update(0.0);
    require(debug_draw->commands().empty(),
        "resetting the active scene must clear its previous debug snapshot");
    require(SecondProbeScene::state.enters == 4
        && SecondProbeScene::state.resets == 1
        && SecondProbeScene::state.payload_value == 33,
        "Reset must reset and re-enter the current scene with the route payload");

    SceneRequest recreate_request{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{
            .target = 2,
            .payload = RoutePayload{ 44 },
            .reload_mode = SceneReloadMode::Recreate
        }
    };
    manager.on_scene_request(recreate_request);
    manager.on_update(0.0);
    require(SecondProbeScene::state.constructions == 2
        && SecondProbeScene::state.destructions == 1
        && SecondProbeScene::state.enters == 5
        && SecondProbeScene::state.payload_value == 44,
        "Recreate must destroy, rebuild, bind, and enter a fresh scene");
    require(SecondProbeScene::state.context_cleared_before_destruction,
        "recreated scenes must be unbound immediately before destruction");

    debug_draw->draw_point(
        elysia::tools::DebugDrawCategory::Gameplay,
        elysia::core::Vector2{},
        4.0f,
        elysia::core::Color{}
    );
    manager.shutdown();
    require(debug_draw->commands().empty(),
        "SceneManager shutdown must clear retained debug commands");
    require(FirstProbeScene::state.context_cleared_before_destruction
        && SecondProbeScene::state.context_cleared_before_destruction,
        "shutdown must clear runtime contexts before cached scenes are destroyed");
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(
        elysia::tools::DebugDrawCategory::All);
}

void test_unbound_scene_runtime_context_is_rejected()
{
    FirstProbeScene scene;
    require(throws_logic_error_containing(
        [&scene] { (void)scene.exposed_runtime_context(); },
        "before a runtime context was bound"),
        "a standalone scene must not expose an unbound runtime context");
}

void test_runtime_features_are_opt_in_and_registration_rolls_back()
{
    ParticipantProbe::destructions = 0;
    ParticipantProbe::was_unbound_at_destruction = false;

    NoPhysicsProbeScene scene;
    require(!scene.has_fixed_step() && !scene.has_physics(),
        "a plain Scene must not allocate fixed-step or physics runtimes");
    require(throws_logic_error_containing(
            [&scene] { (void)scene.add_participant(); },
            "without physics"),
        "a Scene without physics must reject PhysicsParticipant immediately");
    require(ParticipantProbe::destructions == 1,
        "failed PhysicsParticipant registration must roll back ownership");
}

void test_default_gameplay_fixed_step_pause_and_reset()
{
    using namespace elysia::scene;

    DefaultGameplayProbeScene::fixed_updates = 0;
    DefaultGameplayProbeScene::last_tick = 0;
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);
    SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<DefaultGameplayProbeScene>(17);
    manager.start({.target = 17});

    auto& scene = *DefaultGameplayProbeScene::instance;
    require(scene.has_fixed_step() && !scene.has_physics()
            && !scene.collision_available(),
        "GameplayScene must default to fixed-step control without physics or collision");

    manager.on_update(1.0 / 30.0);
    require(DefaultGameplayProbeScene::fixed_updates == 2
            && DefaultGameplayProbeScene::last_tick == 2,
        "a physics-free GameplayScene must still execute stable fixed steps");

    scene.pause();
    manager.on_update(1.0);
    require(DefaultGameplayProbeScene::fixed_updates == 2,
        "paused scenes must neither advance nor accumulate fixed steps");
    scene.resume();
    manager.on_update(1.0 / 60.0);
    require(DefaultGameplayProbeScene::fixed_updates == 3
            && DefaultGameplayProbeScene::last_tick == 3,
        "resuming must continue without catching up paused time");

    manager.on_scene_request(SceneRequest{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{.target = 17, .reload_mode = SceneReloadMode::Reset}});
    manager.on_update(0.0);
    manager.on_update(1.0 / 60.0);
    require(DefaultGameplayProbeScene::last_tick == 1,
        "Reset must clear the fixed-step tick and accumulator");
    manager.shutdown();
}

void test_retirement_continues_after_callback_failure_and_unbinds_physics()
{
    ParticipantProbe::destructions = 0;
    ParticipantProbe::was_unbound_at_destruction = false;
    RetirementProbeScene scene;
    elysia::scene::SceneTestAccess::enter(scene);
    ParticipantProbe* participant = scene.add_participant();
    elysia::ui::UiElement* ui = scene.add_ui();
    require(participant && participant->physics_world(),
        "a PhysicsParticipant must bind while owned by a physics Scene");

    participant->destroy();
    ui->destroy();
    scene.throw_from_first_removal = true;
    bool removal_failed = false;
    try
    {
        elysia::scene::SceneTestAccess::update(scene, 0.0);
    }
    catch (const std::runtime_error&)
    {
        removal_failed = true;
    }
    require(removal_failed && scene.removals == 2,
        "retirement must notify both GameObject and UI roots exactly once even after a callback fails");
    require(ParticipantProbe::destructions == 1
            && ParticipantProbe::was_unbound_at_destruction,
        "retirement must unregister and unbind physics before destroying the participant");
    elysia::scene::SceneTestAccess::exit(scene);
}

void test_same_scene_type_has_independent_keyed_instances()
{
    using namespace elysia::scene;

    KeyedProbeScene::constructions = {};
    KeyedProbeScene::enters = {};
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);
    SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<KeyedProbeScene>(21, 0);
    manager.register_game_scene<KeyedProbeScene>(22, 1);
    manager.start({.target = 21});

    manager.on_scene_request(SceneRequest{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{.target = 22}});
    manager.on_update(0.0);
    manager.on_scene_request(SceneRequest{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{.target = 21}});
    manager.on_update(0.0);
    require(KeyedProbeScene::constructions == std::array<int, 2>{1, 1}
            && KeyedProbeScene::enters == std::array<int, 2>{2, 1},
        "two SceneKeys registered to one concrete type must retain independent cached instances");

    manager.on_scene_request(SceneRequest{
        .type = SceneRequestType::Switch,
        .route = SceneRoute{.target = 22, .reload_mode = SceneReloadMode::Recreate}});
    manager.on_update(0.0);
    require(KeyedProbeScene::constructions == std::array<int, 2>{1, 2},
        "Recreate must replace only the instance stored under the requested SceneKey");
    manager.shutdown();
}

void test_scene_boundary_failures_recover_transactionally()
{
    using namespace elysia::scene;
    constexpr SceneKey failing_key = 31;
    constexpr SceneKey recovery_key = 32;
    const std::array boundaries{
        SceneBoundary::Enter,
        SceneBoundary::Exit,
        SceneBoundary::Reset,
        SceneBoundary::Attach,
        SceneBoundary::Detach,
        SceneBoundary::Input,
        SceneBoundary::Update,
        SceneBoundary::Render,
        SceneBoundary::ObjectRegistration,
        SceneBoundary::ObjectRemoval};

    for (const SceneBoundary boundary : boundaries)
    {
        BoundaryProbeState probe{.trigger = boundary};
        std::optional<SceneBoundaryFailure> observed_failure;
        elysia::io::ContentRegistry registry;
        SceneRuntimeContext context(nullptr, registry, 1280, 720);
        SceneManager manager;
        manager.initialize(context, [&](const SceneBoundaryFailure& failure) {
            observed_failure = failure;
            return SceneRoute{.target = recovery_key};
        });
        manager.register_game_scene<BoundaryFailureScene>(failing_key, std::ref(probe));
        manager.register_game_scene<BoundaryRecoveryScene>(recovery_key, std::ref(probe));
        manager.start({.target = failing_key});

        if (manager.current_scene_key() == failing_key)
        {
            switch (boundary)
            {
            case SceneBoundary::Exit:
            case SceneBoundary::Detach:
                manager.on_scene_request(SceneRequest{
                    .type = SceneRequestType::Switch,
                    .route = SceneRoute{.target = recovery_key}});
                manager.on_update(0.0);
                break;
            case SceneBoundary::Reset:
                manager.on_scene_request(SceneRequest{
                    .type = SceneRequestType::Switch,
                    .route = SceneRoute{
                        .target = failing_key,
                        .reload_mode = SceneReloadMode::Reset}});
                manager.on_update(0.0);
                break;
            case SceneBoundary::Input:
                manager.on_input({});
                break;
            case SceneBoundary::Update:
            case SceneBoundary::ObjectRemoval:
                manager.on_update(0.0);
                break;
            case SceneBoundary::Render:
                manager.on_render(reinterpret_cast<SDL_Renderer*>(1));
                break;
            default:
                break;
            }
        }

        require(observed_failure.has_value()
                && observed_failure->scene == failing_key
                && observed_failure->boundary == boundary,
            "SceneManager must preserve the exact failing scene boundary");
        require(manager.state() == SceneManagerState::Running
                && manager.current_scene_key() == recovery_key
                && probe.recovery_enters == 1,
            "each scene boundary failure must roll back and enter the injected recovery route once");
        require(manager.shutdown(), "a recovered manager must shut down cleanly");
    }
}

void test_recovery_failure_faults_without_recursing()
{
    using namespace elysia::scene;

    BoundaryProbeState probe{.trigger = SceneBoundary::Enter};
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);
    SceneManager manager;
    manager.initialize(context, [](const SceneBoundaryFailure&) {
        return SceneRoute{.target = 42};
    });
    manager.register_game_scene<BoundaryFailureScene>(41, std::ref(probe));
    manager.register_game_scene<AlwaysFailingRecoveryScene>(42);
    SceneManagerObserverProbe observer;
    manager.attach(&observer);

    manager.start({.target = 41});
    require(manager.state() == SceneManagerState::Faulted
            && observer.faults == 1
            && observer.last_failure.scene == 42,
        "a recovery-scene failure must fault once instead of recursively routing");
    require(manager.shutdown(), "fault notification alone must not make cleanup fail");
    require(manager.state() == SceneManagerState::Faulted,
        "a faulted manager must remain faulted after shutdown");
}
}

int main()
{
    test_scene_key_domains_and_payload_helpers();
    test_registration_and_route_key_errors_are_distinct();
    test_runtime_context_synchronizes_camera_viewports();
    test_registration_arguments_are_reusable_for_recreate();
    test_scene_maps_debug_draw_categories_to_physics_capture();
    test_route_copy_reload_modes_and_runtime_context_binding();
    test_unbound_scene_runtime_context_is_rejected();
    test_runtime_features_are_opt_in_and_registration_rolls_back();
    test_default_gameplay_fixed_step_pause_and_reset();
    test_retirement_continues_after_callback_failure_and_unbinds_physics();
    test_same_scene_type_has_independent_keyed_instances();
    test_scene_boundary_failures_recover_transactionally();
    test_recovery_failure_faults_without_recursing();
    return EXIT_SUCCESS;
}
