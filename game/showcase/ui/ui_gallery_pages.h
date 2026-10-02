#pragma once
#include "game/showcase/ui/hud_demo_view.h"
#include "engine/ui/style/ui_theme_manager.h"
#include <functional>
namespace example::showcase::ui {
class UiGalleryPages final {
public:
    UiGalleryPages(HudDemoView& hud,std::function<void(const char*)> status,
        std::function<void(elysia::ui::UiBuiltinTheme)> theme,
        std::function<void(elysia::ui::UiListContainer&)> add_hud, std::function<void()> rebuild)
        :_hud_view(hud),_status(std::move(status)),_theme(std::move(theme)),_add_hud(std::move(add_hud)),_rebuild(std::move(rebuild)) {}
    elysia::ui::UiWindow* _root_window=nullptr;
    std::array<elysia::ui::UiButton*,7> _theme_buttons{};
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


private:
    void rebuild_ui() { _rebuild(); }
    std::function<void()> _rebuild;
    void set_status_key(const char* key) { _status(key); }
    void set_active_theme(elysia::ui::UiBuiltinTheme theme) { _theme(theme); }
    void add_hud_demo(elysia::ui::UiListContainer& page) { _add_hud(page); }
    HudDemoView& _hud_view;
    std::function<void(const char*)> _status;
    std::function<void(elysia::ui::UiBuiltinTheme)> _theme;
    std::function<void(elysia::ui::UiListContainer&)> _add_hud;
};
}
