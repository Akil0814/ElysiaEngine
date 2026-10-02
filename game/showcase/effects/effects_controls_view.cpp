#include "game/showcase/effects/effects_controls_view.h"
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/localization/localization_service.h"
namespace example::showcase::effects {
void EffectsControlsView::build(elysia::ui::UiWindow& window,std::array<std::function<void()>,6> actions,
                               std::array<std::function<void()>,9> screen_actions,std::function<void()> back)
{
    clear();
    using namespace elysia::ui;
    const float width=window.screen_rect().width(),height=window.screen_rect().height();
    auto scroll=std::make_unique<UiScrollContainer>(elysia::core::Rect{0,0,width-240,60});
    scroll->set_scroll_axis(UiScrollAxis::Horizontal);scroll->set_scroll_step_x(152);
    auto row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,6*152.0f,52});row->set_direction(UiListDirection::Horizontal);row->set_item_spacing(12);
    const char* keys[]={"showcase.effects.damage","showcase.effects.critical","showcase.effects.heal","showcase.effects.percent","showcase.effects.fraction","showcase.effects.decimal"};
    for(int i=0;i<6;++i){auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,140,52});button->set_text_content(ui_text_key(keys[i]));button->set_on_click(std::move(actions[i]));row->add_back(std::move(button));}
    scroll->set_content(std::move(row));auto* scope=scroll.get();window.add_child(std::move(scroll),ShowcaseFrame::at({24,height-76,width-240,60}));window.register_focus_scope(*scope);
    ShowcaseFrame::build_chrome(window,"showcase.effects.title","showcase.effects.description",std::move(back));
    auto heading=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width-48,24},0,
        ui_text_key("showcase.effects.screen.instructions"));
    heading->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(heading),ShowcaseFrame::at({24,88,width-48,24}));
    const char* screen_keys[]={"showcase.effects.screen.flash","showcase.effects.screen.fade_black",
        "showcase.effects.screen.hold_black","showcase.effects.screen.stop","showcase.effects.screen.cancel",
        "showcase.effects.screen.stretch","showcase.effects.screen.cover","showcase.effects.screen.contain",
        "showcase.effects.screen.after_ui"};
    const float button_width=(width-48-4*8)/5;
    for(std::size_t start:{std::size_t{0},std::size_t{5}}){
        auto row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,width-48,40});
        row->set_direction(UiListDirection::Horizontal);row->set_item_spacing(8);
        const auto end=start==0?5:screen_actions.size();
        for(std::size_t i=start;i<end;++i){
            auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,button_width,40});
            button->set_text_content(ui_text_key(screen_keys[i]));
            button->set_on_click(std::move(screen_actions[i]));
            if(i==8)_screen_layer_button=button.get();
            row->add_back(std::move(button));
        }
        auto* scope=row.get();
        window.add_child(std::move(row),ShowcaseFrame::at({24,start==0?118.0f:166.0f,width-48,40}));
        window.register_focus_scope(*scope);
    }
    auto status=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width-48,24});
    status->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);_screen_status=status.get();
    window.add_child(std::move(status),ShowcaseFrame::at({24,214,width-48,24}));
    window.focus_first_available_scope();
}
void EffectsControlsView::update_screen(const ScreenControlsData& data)
{
    if(_screen_status){
        const auto prefix=data.active?"showcase.effects.screen.playing":data.has_request?"showcase.effects.screen.finished":nullptr;
        std::string text;
        if(prefix)text=std::string(ELYSIA_LOCALIZATION->tr(prefix))+": ";
        text+=ELYSIA_LOCALIZATION->tr(data.action_key);
        _screen_status->set_text_content(elysia::ui::ui_raw_text(std::move(text)));
    }
    if(_screen_layer_button)_screen_layer_button->set_text_content(elysia::ui::ui_text_key(
        data.after_ui?"showcase.effects.screen.after_ui":"showcase.effects.screen.before_ui"));
}
}
