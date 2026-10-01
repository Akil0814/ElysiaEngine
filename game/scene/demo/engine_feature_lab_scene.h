#pragma once

#include "../../../engine/gameplay/scene/gameplay_scene.h"
#include "../../../engine/tools/debug_draw.h"
#include "demo_scene_payload.h"
#include "../../../engine/effects/screen/screen_effect_types.h"

#include <cstddef>
#include <optional>
#include <string>

namespace elysia::ui
{
class UiAnimation;
class UiWindow;
class UiLabel;
class UiButton;
}

namespace elysia::builtin
{
class EngineCharacter;
}

namespace example::scene
{
// Project-owned playground for runtime engine features such as animation.
class EngineFeatureLabScene final : public elysia::gameplay::GameplayScene
{
    enum class ScreenAction { Flash, FadeBlack, HoldBlack, Stop, Cancel, Stretch, Cover, Contain, ToggleLayer };
    enum class FloatingNumberPreset
    {
        Damage,
        Critical,
        Heal,
        Percent,
        Fraction,
        Decimal
    };

public:
    EngineFeatureLabScene();
    [[nodiscard]] std::size_t color_overlay_index() const noexcept;

protected:
    void on_after_update(double delta) override;
    void on_shortcuts(const elysia::input::RawInputFrame &input,
                      const std::vector<elysia::input::RawInputEvent> &events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;

private:
    elysia::gameplay::ControllerHandle _controller;
    void return_to_caller();
    void apply_secondary_color_overlay();
    void build_feature_controls();
    void build_screen_controls();
    void trigger_screen_action(ScreenAction action);
    void refresh_screen_status();
    void clear_screen_effect_or_return();
    void destroy_feature_controls() noexcept;
    void spawn_floating_number_effect(FloatingNumberPreset preset);
    void enable_character_debug_draw();
    void restore_character_debug_draw() noexcept;
    void refresh_character_debug_draw();

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiAnimation* _primary_animation = nullptr;
    elysia::ui::UiAnimation* _secondary_animation = nullptr;
    elysia::builtin::EngineCharacter* _character = nullptr;
    elysia::ui::UiWindow* _controls_window = nullptr;
    elysia::ui::UiWindow* _screen_controls_window = nullptr;
    elysia::ui::UiLabel* _screen_status = nullptr;
    elysia::ui::UiButton* _screen_layer_button = nullptr;
    std::optional<elysia::effects::ScreenEffectHandle> _screen_effect;
    elysia::effects::ScreenEffectLayer _screen_layer = elysia::effects::ScreenEffectLayer::AfterUi;
    std::string _screen_action = "Ready";
    std::size_t _color_overlay_index = 2;
    bool _debug_draw_state_captured = false;
    bool _previous_debug_draw_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_draw_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
