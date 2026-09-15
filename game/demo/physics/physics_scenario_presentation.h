#pragma once
#include "physics_scenario.h"
#include "../../../engine/core/game_object.h"
namespace example::demo::physics
{
class PhysicsScenarioPresentation final : public elysia::core::GameObject
{
public:
    explicit PhysicsScenarioPresentation(const PhysicsScenario& scenario)
        : GameObject(elysia::core::DepthLayer::Item), _scenario(scenario) {}
    const PhysicsScenario& scenario() const { return _scenario; }
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override
    { _scenario.submit_render_commands(out); }
private:
    const PhysicsScenario& _scenario;
};
}
