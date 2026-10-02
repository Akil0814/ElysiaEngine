#pragma once
#include "engine/ui/window/ui_window.h"
#include <array>
#include <functional>
namespace example::showcase::effects {
class EffectsControlsView final {
public: void build(elysia::ui::UiWindow&,std::array<std::function<void()>,6>,std::function<void()> back);
};
}
