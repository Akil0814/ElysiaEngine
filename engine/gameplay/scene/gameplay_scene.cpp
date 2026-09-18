#include "gameplay_scene.h"
#include "../control/controller_manager.h"
#include "../collision/gameplay_collision_service.h"
namespace elysia::gameplay
{
GameplayScene::GameplayScene() : GameplayScene(elysia::physics::PhysicsWorldConfig{})
{
}
GameplayScene::GameplayScene(elysia::physics::PhysicsWorldConfig config)
    : Scene(config), _collision_runtime(physics_world())
{
    input_router().set_auto_claim_ui_gamepad(false);
    set_ui_interaction_mode(elysia::input::UiInteractionMode::Pointer);
    input_router().set_cancel_handler([this](auto player, auto reason) {
        ControllerManager::instance()->cancel_local(_control_context, player, reason);
    });
}
GameplayScene::~GameplayScene()
{
    _control_context.reset();
}
bool GameplayScene::activate_collision_runtime() noexcept
{
    if (!collision::GameplayCollisionService::instance()->attach_runtime(_collision_runtime))
        return false;
    _control_context.activate();
    return true;
}
void GameplayScene::deactivate_collision_runtime() noexcept
{
    _control_context.deactivate();
    (void)collision::GameplayCollisionService::instance()->detach_runtime(_collision_runtime);
}
bool GameplayScene::contains_control_target(const elysia::core::GameObject *target) const
{
    return contains_object_address(target);
}
void GameplayScene::on_routed_input(const elysia::input::InputSnapshot &input)
{
    ControllerManager::instance()->input(_control_context, input);
}
void GameplayScene::on_fixed_update(std::uint64_t tick, double delta)
{
    ControllerManager::instance()->advance(_control_context, tick, delta);
    on_game_fixed_update(tick, delta);
}
void GameplayScene::on_pause_changed(bool paused)
{
    if (paused)
        ControllerManager::instance()->pause(_control_context);
}
void GameplayScene::on_scene_object_removing(elysia::core::SceneObject &object)
{
    if (auto *target = dynamic_cast<elysia::core::GameObject *>(&object))
        ControllerManager::instance()->object_removed(_control_context, *target);
    on_control_target_removing(object);
}
} // namespace elysia::gameplay
