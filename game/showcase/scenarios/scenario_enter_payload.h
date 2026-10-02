#pragma once
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/scenarios/showcase_scenario.h"

namespace example::scene
{
struct ScenarioEnterPayload
{
    elysia::scene::SceneRoute return_route{};
    std::string scenario_id;
    int pressure_tier = 0;
};
}
