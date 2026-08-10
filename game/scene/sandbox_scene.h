#pragma once

#include "../../engine/scene/scene.h"

#include <cstddef>
#include <string_view>

namespace elysia::ui
{
class UiAnimation;
class UiLabel;
class UiWindow;
}

namespace example::scene
{
class SandboxScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:
    void build_ui();
    void play_animation(std::string_view animation_key);
    void play_idle();
    void play_run();
    void play_current_attack();
    void select_previous_attack();
    void select_next_attack();
    void update_attack_segment_label();
    void return_to_main_menu();

    elysia::ui::UiWindow* _window = nullptr;
    elysia::ui::UiAnimation* _animation_preview = nullptr;
    elysia::ui::UiLabel* _attack_segment_label = nullptr;
    std::size_t _attack_segment_index = 0;
};
}
