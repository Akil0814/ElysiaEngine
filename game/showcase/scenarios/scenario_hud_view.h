#pragma once
#include "game/showcase/scenarios/showcase_scenario.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/containers/ui_list_container.h"
#include <functional>
#include <array>
namespace example::showcase::scenarios
{
struct ScenarioActions { std::function<void()> run, pause, step, reset, back; };
class ScenarioHudView final
{
public:
    void build(elysia::ui::UiWindow&,const ShowcaseScenario&,ScenarioActions);
    void update(const ShowcaseScenario&);
    void clear() noexcept { _labels.fill(nullptr); _checks=nullptr; _displayed=0; }
private:
    std::array<elysia::ui::UiLabel*,9> _labels{};
    elysia::ui::UiListContainer* _checks=nullptr;
    std::size_t _displayed=0;
};
}
