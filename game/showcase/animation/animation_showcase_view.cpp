#include "game/showcase/animation/animation_showcase_view.h"
namespace example::showcase::animation {
void AnimationShowcaseView::build(elysia::ui::UiWindow& window,std::array<std::function<void()>,5> actions,std::function<void()> back)
{
    _frame.build(window,"showcase.animation.title","showcase.animation.description",std::move(back));
    auto preview=std::make_unique<elysia::ui::UiAnimation>(elysia::core::Rect{0,0,256,256});animation=preview.get();_frame.content().add_back(std::move(preview));
    auto label=std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{0,0,800,30});segment=label.get();_frame.content().add_back(std::move(label));
    const char* keys[]={"animation_preview.animation_idle","animation_preview.animation_run","animation_preview.attack_previous","animation_preview.attack_replay","animation_preview.attack_next"};
    for(int i=0;i<5;++i)_frame.add_action(keys[i],std::move(actions[i]));
    window.focus_first_available_scope();
}
}
