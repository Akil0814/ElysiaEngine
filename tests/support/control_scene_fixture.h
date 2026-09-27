#pragma once
#include "engine/scene/scene_manager.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/gameplay/control/controller_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "game/input/gameplay_input_map.h"
#include "tests/support/test_assertions.h"
namespace elysia::tests
{
template <class T> class ControlSceneFixture
{
  public:
    ControlSceneFixture()
        : context(nullptr, registry, 1280, 720)
    {
        auto *service = gameplay::ControllerService::instance();
        manager.initialize(context);
        if (service->session_active())
            service->end_session();
        require(bool(service->begin_session()), "Fixture explicitly begins its game session");
        manager.template register_game_scene<T>(998, &scene);
        manager.start({.target = 998});
    }
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context;
    scene::SceneManager manager;
    T *scene = nullptr;
};
inline gameplay::ControllerHandle local_controller(gameplay::GameplayScene &scene,
                                                   input::LocalPlayerId player)
{
    auto result = gameplay::ControllerService::instance()->create<gameplay::LocalPlayerController>(
        {gameplay::ControllerScope::Scene, scene.control_context().token()}, player,
        example::input::make_gameplay_input_map());
    require(bool(result), "Explicit local controller creation");
    return *result;
}
} // namespace elysia::tests
