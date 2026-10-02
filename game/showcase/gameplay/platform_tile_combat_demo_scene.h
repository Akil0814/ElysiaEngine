#pragma once

#include "game/showcase/gameplay/gameplay_demo_scene_base.h"

namespace example::scene
{
class PlatformTileCombatDemoScene final : public GameplayDemoSceneBase
{
public:
    PlatformTileCombatDemoScene();

private:
    void build_demo() override;
};
}
