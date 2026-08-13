#pragma once

#include "physics_combat_demo_scene_base.h"

namespace example::scene
{
class TopDownTileCombatDemoScene final : public PhysicsCombatDemoSceneBase
{
public:
    TopDownTileCombatDemoScene();

private:
    void build_demo() override;
};
}
