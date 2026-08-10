#pragma once

#include "../scene/scene.h"
#include "../tools/timer.h"
#include "../ui/style/ui_theme_manager.h"

#include "elysia_scene_payload.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace elysia::ui
{
class UiWindow;
class UiListContainer;
}

namespace elysia::realm
{
class ElysiaScene final : public elysia::scene::Scene
{

public:
    void on_input(const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_update(double delta)override;
    void on_exit() override;
    void reset() override;

private:
    enum class PlaybackPhase
    {
        Loading,
        Complete
    };

    void reveal_next_code_line();
    void add_label(std::string_view code_line);
    void stop_playback() noexcept;
    void build_ui();
    void destroy_ui() noexcept;
    void return_to_caller();

private:
    PlaybackPhase _playback_phase = PlaybackPhase::Loading;
    std::size_t _current_line = 0;

    elysia::tools::Timer _code_timer;

    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    elysia::ui::UiListContainer* _code_list = nullptr;
    elysia::ui::UiThemeManager _elysia_theme;
    elysia::ui::UiThemeRegistration _theme_registration;
};
}
