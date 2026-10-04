#define SDL_MAIN_HANDLED
#include "game/showcase/input/local_multiplayer_scene.h"
#include "game/showcase/camera/camera_showcase_scene.h"
#include "game/showcase/gameplay/gameplay_demo_scene_base.h"
#include "game/navigation/showcase_scene_keys.h"
#include "game/gameplay/control/local_controls.h"
#include "engine/gameplay/control/controller_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/scene_test_access.h"
#include "tests/support/test_assertions.h"
#include <stdexcept>
#include <string_view>
#include <limits>

namespace
{
class ThrowingPhysicsDemo final : public example::scene::GameplayDemoSceneBase
{
public:
    ThrowingPhysicsDemo() : GameplayDemoSceneBase(
        example::scene_keys::ColliderCombatDemo,"exception probe",{}) {}
private:
    void build_demo() override { throw std::runtime_error("injected physics demo build failure"); }
};
}

int main()
{
    using elysia::tests::require;
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    auto* debug = elysia::tools::DebugDraw::instance();
    debug->set_enabled(false);
    debug->set_enabled_categories(elysia::tools::DebugDrawCategory::PhysicsBroadPhase);
    {
        ThrowingPhysicsDemo scene;
        elysia::scene::SceneTestAccess::bind(scene, context);
        bool caught = false;
        try
        {
            elysia::scene::SceneTestAccess::enter(scene,
                example::scene::ShowcaseEnterPayload{.return_route={.target=example::scene_keys::MainMenu}});
        }
        catch (const std::runtime_error& error)
        {
            caught = std::string_view(error.what()) == "injected physics demo build failure";
        }
        require(caught && !debug->enabled()
                && debug->enabled_categories() == elysia::tools::DebugDrawCategory::PhysicsBroadPhase,
            "failed physics demo entry restores global DebugDraw state and preserves first failure");
    }

    elysia::scene::SceneManager manager;
    manager.initialize(context);
    require(elysia::gameplay::ControllerService::instance()->begin_session().has_value(),
        "controller session for multiplayer entry test");
    {
        example::scene::LocalMultiplayerScene scene;
        elysia::scene::SceneTestAccess::bind(scene, context);
        const auto before = scene.local_players().configuration();
        const auto player_count = scene.local_players().players().size();
        const auto revision = scene.local_players().revision();
        const auto first_version = scene.local_players().binding_version(
            elysia::input::PrimaryLocalPlayer);
        bool caught = false;
        try
        {
            elysia::scene::SceneTestAccess::enter(scene,
                example::scene::ShowcaseEnterPayload{.return_route={.target=example::scene_keys::MainMenu}});
        }
        catch (const std::exception& error)
        {
            const std::string_view reason(error.what());
            caught = reason.starts_with("Multiplayer") || reason.starts_with("Invalid multiplayer");
        }
        const auto& after = scene.local_players().configuration();
        require(caught && scene.local_players().players().size() == player_count
                && after.partitions == before.partitions && after.bindings == before.bindings
                && scene.local_players().revision() == revision
                && scene.local_players().binding_version(elysia::input::PrimaryLocalPlayer) == first_version
                && !example::gameplay::existing_session_player(elysia::input::PrimaryLocalPlayer),
            "failed multiplayer entry restores all player and device state");
    }
    elysia::gameplay::ControllerService::instance()->end_session();
    {
        example::scene::CameraShowcaseScene scene;
        elysia::scene::SceneTestAccess::bind(scene,context);
        bool caught=false;
        try {elysia::scene::SceneTestAccess::enter(scene,example::scene::ShowcaseEnterPayload{{.target=example::scene_keys::MainMenu}});}
        catch(const std::logic_error&){caught=true;}
        require(caught && !scene.state().motion && !scene.state().blend && !scene.state().frozen,
            "camera entry failure without controller session cleans playback and actor gates");
        require(elysia::scene::SceneTestAccess::ui_root(scene,0)->is_destroyed(),"failed camera entry retires its controls");
    }
    require(manager.shutdown(),"controller session shuts down after failed entry");

    for (const float invalid : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::max()})
    {
        bool rejected = false;
        try
        {
            example::showcase::gameplay::DemoTileMap tiles({}, {invalid, 1}, 2, 1, {});
        }
        catch (const std::invalid_argument&) { rejected = true; }
        require(rejected,"DemoTileMap rejects nonfinite or overflowing tile dimensions");
    }
}
