#pragma once
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include <array>
#include <functional>
namespace example::showcase::effects {
struct ScreenControlsData {
    const char* action_key;
    bool active, has_request, after_ui;
};
class EffectsControlsView final {
public:
    void build(elysia::ui::UiWindow&,std::array<std::function<void()>,6> number_actions,
               std::array<std::function<void()>,9> screen_actions,std::function<void()> back);
    void update_screen(const ScreenControlsData&);
    void clear() noexcept { _screen_status=nullptr; _screen_layer_button=nullptr; }
private:
    elysia::ui::UiLabel* _screen_status=nullptr;
    elysia::ui::UiButton* _screen_layer_button=nullptr;
};
}
