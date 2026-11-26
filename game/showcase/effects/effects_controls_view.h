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
struct AnimationControlsData {
    bool loop;
    const char* end_key;
    const char* fit_key;
    const char* anchor_key;
    const char* offset_key;
    const char* scale_key;
};
class EffectsControlsView final {
public:
    void build(elysia::ui::UiWindow&,std::array<std::function<void()>,6> number_actions,
               std::array<std::function<void()>,9> screen_actions,std::function<void()> back,
               std::array<std::function<void()>,8> animation_actions,
               std::array<std::function<void()>,5> particle_actions);
    void update_screen(const ScreenControlsData&);
    void update_particles(const char* status_key);
    void update_animation(const AnimationControlsData&);
    void clear() noexcept { _particle_status=nullptr; _screen_status=nullptr; _screen_layer_button=nullptr; _animation_buttons.fill(nullptr); }
private:
    elysia::ui::UiLabel* _screen_status=nullptr;
    elysia::ui::UiLabel* _particle_status=nullptr;
    elysia::ui::UiButton* _screen_layer_button=nullptr;
    std::array<elysia::ui::UiButton*,8> _animation_buttons{};
};
}
