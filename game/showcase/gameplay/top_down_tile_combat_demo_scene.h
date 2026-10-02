#pragma once

#include "game/showcase/gameplay/gameplay_demo_scene_base.h"

namespace example::scene
{
class TopDownTileCombatDemoScene final : public GameplayDemoSceneBase
{
public:
    TopDownTileCombatDemoScene();

private:
    void build_demo() override;
};
}
