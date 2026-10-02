#pragma once
#include "game/showcase/animation/animation_showcase_view.h"

#include "engine/scene/scene.h"

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
class AnimationPreviewScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:
    void build_ui();
    void play_animation(std::string_view animation_key);
    void play_idle();
    void play_run();
    void play_current_attack();
    void select_previous_attack();
    void select_next_attack();
    void update_attack_segment_label();
    void return_to_caller();

    elysia::ui::UiWindow* _window = nullptr;
    example::showcase::animation::AnimationShowcaseView _view;
    std::size_t _attack_segment_index = 0;
    elysia::scene::SceneRoute _return_route;
};
}
