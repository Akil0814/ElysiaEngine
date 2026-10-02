#pragma once
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/ui/window/ui_window.h"
namespace example::showcase::ui {
class ThemePreview final {
public:
    void attach(elysia::ui::UiWindow& window) { _registration=manager.register_root(window); }
    void clear() noexcept { _registration.reset(); }
    elysia::ui::UiThemeManager manager;
private: elysia::ui::UiThemeRegistration _registration;
};
}
