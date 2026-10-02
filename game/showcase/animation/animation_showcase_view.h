#pragma once
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/widgets/image/ui_animation.h"
#include <array>
#include <functional>
namespace example::showcase::animation {
class AnimationShowcaseView final {
public:
    void build(elysia::ui::UiWindow&,std::array<std::function<void()>,5> actions,std::function<void()> back);
    elysia::ui::UiAnimation* animation=nullptr;
    elysia::ui::UiLabel* segment=nullptr;
private: ShowcaseFrame _frame;
};
}
