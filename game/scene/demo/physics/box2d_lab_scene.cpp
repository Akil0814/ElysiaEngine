#include "box2d_lab_scene.h"
#include "../../example_scene_keys.h"
#include "physics_combat_demo_helpers.h"
namespace example::scene
{
Box2DLabScene::Box2DLabScene()
    : PhysicsCombatDemoSceneBase(example::scene_keys::Box2DLab,"Box2DLabScene",
          detail::make_gravity_config(980),"Physics Lab","P Pause | N Step") {}
void Box2DLabScene::build_demo() {}
void Box2DLabScene::on_shortcuts(const elysia::input::RawInputFrame &frame,
                                 const std::vector<elysia::input::RawInputEvent> &events)
{
    PhysicsCombatDemoSceneBase::on_shortcuts(frame, events);
}
}
