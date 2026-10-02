#pragma once
#include "game/showcase/ui/theme_preview.h"
#include "game/showcase/shared/showcase_frame.h"
#include "game/showcase/ui/ui_gallery_pages.h"
#include "game/showcase/ui/hud_demo_view.h"

#include "engine/scene/scene.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "engine/ui/style/ui_theme_manager.h"

#include <array>
#include <memory>
#include <vector>

struct SDL_Texture;

namespace elysia::ui
{
class UiButton;
class UiActionButton;
class UiLabel;
class UiListContainer;
class UiScrollContainer;
class UiTabContainer;
class UiWindow;
}

namespace example::scene
{
class UiComponentGalleryScene final : public elysia::scene::Scene
{
public:
    UiComponentGalleryScene() = default;
protected:
    void on_shortcuts(const elysia::input::RawInputFrame &input,
                      const std::vector<elysia::input::RawInputEvent> &events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
    void on_before_update(double delta) override;
    void on_routed_input(const elysia::input::InputSnapshot& input) override;

private:
    void rebuild_ui();
    void clear_ui();
    void add_gallery_tab(
        elysia::ui::UiTabContainer& tabs,
        const char* label_key,
        std::unique_ptr<elysia::ui::UiScrollContainer> page);
    void refresh_theme_preview_styles();
    void return_to_caller();
    void set_active_theme(elysia::ui::UiBuiltinTheme theme);
    void sync_theme_switch_button_roles() noexcept;
    void set_status_key(const char* key);
    void add_hud_demo(elysia::ui::UiListContainer& page);
    void sync_hud_demo();
    void trigger_hud_skill();
    void use_hud_item(std::size_t index);
    void toggle_hud_pause();
    void cancel_hud_demo();

private:
    elysia::scene::SceneRoute _return_route;
    elysia::ui::UiWindow* _root_window = nullptr;
    example::showcase::ui::ThemePreview _theme_preview;
    elysia::ui::UiLabel* _status_label = nullptr;
    example::showcase::ui::HudDemoState _hud_state;
    example::showcase::ui::HudDemoView _hud_view;
    example::showcase::ui::UiGalleryPages _pages{_hud_view,
        [this](const char* key){set_status_key(key);},
        [this](elysia::ui::UiBuiltinTheme theme){set_active_theme(theme);},
        [this](elysia::ui::UiListContainer& page){add_hud_demo(page);},[this]{_rebuild_requested=true;}};
    bool _rebuild_requested=false;
};
}
