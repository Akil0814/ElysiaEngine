#pragma once
#include "../demo_scene_payload.h"
#include "../../../demo/physics/physics_scenario.h"

namespace example::scene
{
struct PhysicsDemoPayload
{
    elysia::scene::SceneRoute return_route{};
    std::string scenario_id;
    example::demo::physics::ScenarioMode mode = example::demo::physics::ScenarioMode::Verify;
    int pressure_tier = 0;
};
}
