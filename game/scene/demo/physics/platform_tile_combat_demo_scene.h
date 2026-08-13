#pragma once

#include "physics_combat_demo_scene_base.h"

namespace example::scene
{
class PlatformTileCombatDemoScene final : public PhysicsCombatDemoSceneBase
{
public:
    PlatformTileCombatDemoScene();

private:
    void build_demo() override;
};
}
