#include "game/showcase/effects/effects_controls_view.h"
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/containers/ui_scroll_container.h"
namespace example::showcase::effects {
void EffectsControlsView::build(elysia::ui::UiWindow& window,std::array<std::function<void()>,6> actions,std::function<void()> back)
{
    using namespace elysia::ui;
    const float width=window.screen_rect().width(),height=window.screen_rect().height();
    auto scroll=std::make_unique<UiScrollContainer>(elysia::core::Rect{0,0,width-240,60});
    scroll->set_scroll_axis(UiScrollAxis::Horizontal);scroll->set_scroll_step_x(152);
    auto row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,6*152.0f,52});row->set_direction(UiListDirection::Horizontal);row->set_item_spacing(12);
    const char* keys[]={"showcase.effects.damage","showcase.effects.critical","showcase.effects.heal","showcase.effects.percent","showcase.effects.fraction","showcase.effects.decimal"};
    for(int i=0;i<6;++i){auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,140,52});button->set_text_content(ui_text_key(keys[i]));button->set_on_click(std::move(actions[i]));row->add_back(std::move(button));}
    scroll->set_content(std::move(row));auto* scope=scroll.get();window.add_child(std::move(scroll),ShowcaseFrame::at({24,height-76,width-240,60}));window.register_focus_scope(*scope);
    ShowcaseFrame::build_chrome(window,"showcase.effects.title","showcase.effects.description",std::move(back));
    window.focus_first_available_scope();
}
}
