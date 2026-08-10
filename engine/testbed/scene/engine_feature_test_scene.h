#pragma once

#include "../../scene/scene.h"
#include "../../tools/debug_draw.h"
#include "testbed_scene_payload.h"

#include <cstddef>

namespace elysia::ui
{
class UiAnimation;
class UiWindow;
}

namespace elysia::builtin
{
class BuiltinAudioPlayer;
class EngineCharacter;
}

namespace elysia::testbed
{
// Engine-owned playground for runtime gameplay features such as animation.
class EngineFeatureTestScene final : public elysia::scene::Scene
{
public:
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
    void build_audio_controls();
    void destroy_audio_controls() noexcept;
    void enable_character_debug_draw();
    void restore_character_debug_draw() noexcept;
    void refresh_character_debug_draw();

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiAnimation* _primary_animation = nullptr;
    elysia::ui::UiAnimation* _secondary_animation = nullptr;
    elysia::builtin::EngineCharacter* _character = nullptr;
    elysia::ui::UiWindow* _audio_window = nullptr;
    const elysia::builtin::BuiltinAudioPlayer* _audio_player = nullptr;
    std::size_t _color_overlay_index = 2;
    bool _debug_draw_state_captured = false;
    bool _previous_debug_draw_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_draw_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
