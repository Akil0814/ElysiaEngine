#pragma once
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include <functional>
#include <array>
namespace example::showcase::camera {
class CameraControlsView final {
public:
    void build(elysia::ui::UiWindow&,std::array<std::function<void()>,7>,std::function<void()> back);
    void status(elysia::ui::UiTextContent value) { if(_status)_status->set_text_content(std::move(value)); }
    void clear() noexcept { _status=nullptr; }
private: elysia::ui::UiLabel* _status=nullptr;
};
}
