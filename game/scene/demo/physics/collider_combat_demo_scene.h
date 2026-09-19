#pragma once

#include "physics_combat_demo_scene_base.h"

namespace example::scene
{
class ColliderCombatDemoScene final : public PhysicsCombatDemoSceneBase
{
public:
    ColliderCombatDemoScene();

  protected:
    void on_control_target_removing(elysia::core::SceneObject &) override;

  private:
    elysia::core::GameObject *_query_probe = nullptr;
    void configure_player_controller(example::demo::physics::BlockCombatActor&) override;
    void build_demo() override;
};
}
