#pragma once
#include "../../scene/scene.h"
#include "../collision/gameplay_collision_runtime.h"
#include "../control/controller_service.h"
namespace elysia::gameplay
{
class GameplayScene : public elysia::scene::Scene
{
  public:
    GameplayScene();
    explicit GameplayScene(elysia::physics::PhysicsWorldConfig config);
    ~GameplayScene() override;
    SceneControlContext &control_context()
    {
        return _control_context;
    }

  protected:
    collision::GameplayCollisionRuntime &collision_runtime() noexcept
    {
        return _collision_runtime;
    }
    virtual void on_game_fixed_update(std::uint64_t, double)
    {
    }
    virtual void on_control_target_removing(elysia::core::SceneObject &)
    {
    }

  private:
    friend class elysia::scene::SceneManager;
    friend class ControllerManager;
    bool contains_control_target(const elysia::core::GameObject *) const;
    bool activate_collision_runtime() noexcept;
    void deactivate_collision_runtime() noexcept;
    void reset_control_context()
    {
        _control_context.reset();
    }
    void on_routed_input(const elysia::input::InputSnapshot &) final;
    void on_fixed_update(std::uint64_t, double) final;
    void on_pause_changed(bool) final;
    void on_scene_object_removing(elysia::core::SceneObject &) final;
    collision::GameplayCollisionRuntime _collision_runtime;
    SceneControlContext _control_context{*this};
};
} // namespace elysia::gameplay
