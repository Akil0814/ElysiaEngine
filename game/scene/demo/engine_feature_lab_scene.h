#pragma once

#include "../../../engine/scene/scene.h"
#include "../../../engine/tools/debug_draw.h"
#include "demo_scene_payload.h"

#include <cstddef>

namespace elysia::ui
{
class UiAnimation;
class UiWindow;
}

namespace elysia::builtin
{
class EngineCharacter;
class BuiltinResources;
}

namespace example::scene
{
// Project-owned playground for runtime engine features such as animation.
class EngineFeatureLabScene final : public elysia::scene::Scene
{
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
    explicit EngineFeatureLabScene(
        const elysia::builtin::BuiltinResources& builtin_resources) noexcept;
    void on_update(double delta) override;
    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    [[nodiscard]] std::size_t color_overlay_index() const noexcept;

private:
    void return_to_caller();
    void apply_secondary_color_overlay();
    void build_feature_controls();
    void destroy_feature_controls() noexcept;
    void spawn_floating_number_effect(FloatingNumberPreset preset);
    void enable_character_debug_draw();
    void restore_character_debug_draw() noexcept;
    void refresh_character_debug_draw();

private:
    const elysia::builtin::BuiltinResources* _builtin_resources = nullptr;
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiAnimation* _primary_animation = nullptr;
    elysia::ui::UiAnimation* _secondary_animation = nullptr;
    elysia::builtin::EngineCharacter* _character = nullptr;
    elysia::ui::UiWindow* _controls_window = nullptr;
    std::size_t _color_overlay_index = 2;
    bool _debug_draw_state_captured = false;
    bool _previous_debug_draw_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_draw_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
