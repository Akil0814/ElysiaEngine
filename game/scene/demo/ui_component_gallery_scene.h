#pragma once

#include "../../../engine/scene/scene.h"
#include "demo_scene_payload.h"
#include "../../../engine/ui/style/ui_theme_manager.h"

#include <array>
#include <memory>
#include <vector>

struct SDL_Texture;

namespace elysia::ui
{
class UiButton;
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
    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;

private:
    void rebuild_ui();
    void clear_ui();
    void add_gallery_tab(
        elysia::ui::UiTabContainer& tabs,
        const char* label_key,
        std::unique_ptr<elysia::ui::UiScrollContainer> page);
    [[nodiscard]] std::unique_ptr<elysia::ui::UiButton> make_button(
        const char* text_key) const;
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        make_page_scroll(elysia::ui::UiListContainer*& content) const;
    [[nodiscard]] elysia::ui::UiListContainer* add_section(
        elysia::ui::UiListContainer& page,
        const char* title_key,
        const char* description_key) const;

    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_overview_page();
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_states_page(SDL_Texture* image_texture);
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_controls_page();
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_media_page(SDL_Texture* image_texture);
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_containers_page();
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_overlays_page();
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_appearance_page();
    [[nodiscard]] std::unique_ptr<elysia::ui::UiScrollContainer>
        build_typography_page();

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
