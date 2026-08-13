#pragma once

#include "physics_combat_demo_scene_base.h"

namespace example::scene
{
class ColliderCombatDemoScene final : public PhysicsCombatDemoSceneBase
{
public:
    ColliderCombatDemoScene();

private:
    void build_demo() override;
};
}
