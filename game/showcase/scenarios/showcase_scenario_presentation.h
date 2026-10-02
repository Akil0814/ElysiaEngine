#pragma once
#include "game/showcase/scenarios/showcase_scenario.h"
#include "engine/core/game_object.h"
namespace example::showcase::scenarios
{

class ShowcaseScenarioPresentation final : public elysia::core::GameObject
{
public:
    explicit ShowcaseScenarioPresentation(const ShowcaseScenario& scenario)
        : GameObject(elysia::core::DepthLayer::Item), _scenario(scenario) {}
    const ShowcaseScenario& scenario() const { return _scenario; }
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override
    { _scenario.submit_render_commands(out); }
private:
    const ShowcaseScenario& _scenario;
};
}
