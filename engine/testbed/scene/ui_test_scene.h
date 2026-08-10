#pragma once

#include "../../scene/scene.h"
#include "testbed_scene_payload.h"
#include "../../ui/style/ui_theme_manager.h"

#include <array>
#include <vector>

namespace elysia::ui
{
class UiButton;
class UiLabel;
class UiWindow;
}

namespace elysia::testbed
{
class UiTestScene final : public elysia::scene::Scene
{
public:
    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:
    void rebuild_ui();
    void clear_ui();
    void refresh_theme_preview_styles();
    void return_to_caller();
    void set_active_theme(elysia::ui::UiBuiltinTheme theme);
    void sync_theme_switch_button_roles() noexcept;
    void set_status_key(const char* key);

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    elysia::ui::UiThemeManager _theme_manager;
    std::vector<elysia::ui::UiThemeRegistration> _theme_registrations;
    std::array<elysia::ui::UiButton*,7> _theme_buttons{};
    elysia::ui::UiLabel* _status_label = nullptr;
};
}
