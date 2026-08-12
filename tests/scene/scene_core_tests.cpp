#define SDL_MAIN_HANDLED

#include "engine/io/loaders/asset_config_types.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/camera/camera_manager.h"
#include "engine/scene/scene.h"
#include "engine/scene/routing/scene_key.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/routing/scene_payload.h"
#include "engine/scene/routing/scene_route.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/tools/debug_draw.h"
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

    void reset() override
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

    elysia::physics::PhysicsWorld& exposed_physics_world() noexcept
    {
        return physics_world();
    }

    static inline ProbeState state{};
    static inline ProbeScene* last_instance = nullptr;
};

using FirstProbeScene = ProbeScene<1>;
using SecondProbeScene = ProbeScene<2>;

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
    static_assert(SceneKeys::ElysiaEasterEgg == 1111);
    static_assert(SceneKeys::is_easter_egg(1111));
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
    easter_egg_manager.register_engine_scene<FirstProbeScene>(
        SceneKeys::ElysiaEasterEgg);
    require(throws_logic_error_containing(
        [&easter_egg_manager] {
            easter_egg_manager.register_engine_scene<SecondProbeScene>(
                SceneKeys::ElysiaEasterEgg);
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
    manager.set_runtime_context(full_hd_context);

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
    manager.set_runtime_context(resized_context);
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

void test_scene_maps_debug_draw_categories_to_physics_capture()
{
    using elysia::physics::PhysicsDebugCapture;
    using elysia::tools::DebugDrawCategory;

    FirstProbeScene scene;
    auto* debug_draw = elysia::tools::DebugDraw::instance();
    debug_draw->set_enabled(false);
    debug_draw->set_enabled_categories(DebugDrawCategory::All);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None,
        "disabled DebugDraw must disable physics diagnostic capture");

    debug_draw->set_enabled(true);
    debug_draw->set_enabled_categories(DebugDrawCategory::Gameplay);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None,
        "non-physics categories must not enable physics diagnostic capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsCollider);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Shapes,
        "collider drawing must request shape capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsBroadPhase);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::BroadPhase,
        "broad-phase drawing must request broad-phase capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsContactNormal);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Contacts,
        "contact-normal drawing must request contact capture");

    debug_draw->set_enabled_categories(DebugDrawCategory::PhysicsVelocity);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::Velocities,
        "velocity drawing must request velocity capture");

    debug_draw->set_enabled(false);
    scene.on_update(0.0);
    require(scene.exposed_physics_world().debug_capture()
            == PhysicsDebugCapture::None
            && scene.exposed_physics_world().debug_snapshot().shapes.empty(),
        "turning DebugDraw off must clear the active physics snapshot");
    debug_draw->set_enabled_categories(DebugDrawCategory::All);
}

void test_route_copy_reload_modes_and_runtime_context_binding()
{
    using namespace elysia::scene;

    reset_probe_states();
    elysia::io::ContentRegistry registry;
    SceneRuntimeContext context(nullptr, registry, 1280, 720);

    SceneManager manager;
    manager.set_runtime_context(context);
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
}

int main()
{
    test_scene_key_domains_and_payload_helpers();
    test_registration_and_route_key_errors_are_distinct();
    test_runtime_context_synchronizes_camera_viewports();
    test_scene_maps_debug_draw_categories_to_physics_capture();
    test_route_copy_reload_modes_and_runtime_context_binding();
    test_unbound_scene_runtime_context_is_rejected();
    return EXIT_SUCCESS;
}
