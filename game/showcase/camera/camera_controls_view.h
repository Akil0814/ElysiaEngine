#pragma once
#include "camera_demo_state.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/composites/ui_tab_bar.h"
#include <functional>
namespace example::showcase::camera {
const char* camera_action_key(CameraAction);
bool camera_action_enabled(const CameraDemoState&,CameraAction);
class CameraControlsView final {
public:
    void build(elysia::ui::UiWindow&,std::function<void(CameraPage)>,std::function<void(CameraAction)>,std::function<void()>);
    void update(const CameraDemoState&,elysia::ui::UiTextContent status,elysia::ui::UiTextContent detail);
    void clear() noexcept;
private:
    elysia::ui::UiWindow* _window = nullptr;
    elysia::ui::UiFocusScope* _reset_scope = nullptr;
    elysia::ui::UiFocusScope* _back_scope = nullptr;
    elysia::ui::UiTabBar* _tabs = nullptr;
    std::array<std::array<elysia::ui::UiElement*,2>,3> _rows{};
    elysia::ui::UiLabel* _status = nullptr;
    elysia::ui::UiLabel* _detail = nullptr;
    std::array<std::array<elysia::ui::UiButton*,static_cast<std::size_t>(CameraAction::Count)>,3> _page_buttons{};
};
}
