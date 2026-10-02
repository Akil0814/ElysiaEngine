#pragma once
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/containers/ui_list_container.h"
#include <array>
#include <functional>
namespace example::showcase::input {
class LocalControlsView final {
public:
    void build(elysia::ui::UiWindow&,std::array<std::function<void()>,11>,std::function<void()> open,std::function<void()> back);
    elysia::ui::UiListContainer* menu=nullptr;
    elysia::ui::UiLabel* status=nullptr;
};
}
