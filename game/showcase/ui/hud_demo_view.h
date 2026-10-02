#pragma once
#include "game/showcase/ui/hud_demo_state.h"
#include "engine/ui/widgets/ui_action_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/containers/ui_list_container.h"
#include <functional>
namespace example::showcase::ui
{
struct HudDemoActions { std::function<void()> cast,pause; std::function<void(std::size_t)> use; };
class HudDemoView final
{
public:
    void build(elysia::ui::UiListContainer&,HudDemoActions);
    void sync(const HudDemoState&);
    void cancel();
    void clear() { cancel(); page=nullptr; skill=nullptr; items.fill(nullptr); pause=nullptr; status=nullptr; }
    elysia::ui::UiScrollContainer* page=nullptr;
    elysia::ui::UiActionButton* skill=nullptr;
    std::array<elysia::ui::UiActionButton*,2> items{};
    elysia::ui::UiActionButton* pause=nullptr;
    elysia::ui::UiLabel* status=nullptr;
private:
    HudDemoActions _actions;
};
}
