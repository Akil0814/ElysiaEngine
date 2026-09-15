#pragma once
#include "physics_combat_demo_scene_base.h"
namespace example::scene
{
class Box2DLabScene final : public PhysicsCombatDemoSceneBase
{
  public:
    Box2DLabScene();
    void on_input(const elysia::input::RawInputFrame &,
                  const std::vector<elysia::input::RawInputEvent> &) override;

  protected:
    void build_demo() override;

};
} // namespace example::scene
