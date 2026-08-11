#pragma once

#include "physics_demo_scene_base.h"

namespace example::scene
{
class PhysicsCollisionTestScene final : public PhysicsDemoSceneBase
{
public:
    PhysicsCollisionTestScene();
private:
    void build_demo() override;
};

class PlatformTilePhysicsTestScene final : public PhysicsDemoSceneBase
{
public:
    PlatformTilePhysicsTestScene();
private:
    void build_demo() override;
};

class TopDownTilePhysicsTestScene final : public PhysicsDemoSceneBase
{
public:
    TopDownTilePhysicsTestScene();
private:
    void build_demo() override;
};
}
