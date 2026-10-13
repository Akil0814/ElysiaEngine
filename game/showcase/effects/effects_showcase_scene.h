#pragma once
#include "game/showcase/effects/effects_controls_view.h"

#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/tools/debug_draw.h"
#include "game/showcase/shared/showcase_enter_payload.h"

#include "engine/effects/screen/screen_effect_types.h"

#include <cstddef>
#include <optional>

namespace elysia::ui
{
class UiAnimation;
class UiWindow;
}

namespace elysia::builtin
{
class EngineCharacter;
}

namespace example::scene
{
// Project-owned showcase of color overlays, floating numbers and screen effects.
class EffectsShowcaseScene final : public elysia::gameplay::GameplayScene
{
    enum class ScreenAction { Flash, FadeBlack, HoldBlack, Stop, Cancel, Stretch, Cover, Contain, ToggleLayer };
    enum class AnimationAction { Play, Loop, End, Fit, Anchor, Offset, Scale, Image };
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
    EffectsShowcaseScene();
    ~EffectsShowcaseScene() override;
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
    void cancel_screen_effect() noexcept;
    void trigger_screen_action(ScreenAction action);
    void trigger_animation_action(AnimationAction action);
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
    example::showcase::effects::EffectsControlsView _view;
    std::optional<elysia::effects::ScreenEffectHandle> _screen_effect;
    elysia::effects::ScreenEffectLayer _screen_layer = elysia::effects::ScreenEffectLayer::AfterUi;
    elysia::effects::ScreenAnimationEffectRequest _animation_request;
    const char* _screen_action_key = "showcase.effects.screen.ready";
    std::size_t _color_overlay_index = 2;
    bool _debug_draw_state_captured = false;
    bool _previous_debug_draw_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_draw_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
