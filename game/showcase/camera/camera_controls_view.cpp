#include "camera_controls_view.h"
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include <vector>
#include <algorithm>
namespace example::showcase::camera {
namespace {
using A = CameraAction;
const std::array<std::vector<A>,3> actions{{
    {A::Strategy,A::DeadZone,A::Swap,A::Automatic,A::Teleport,A::Zoom,A::Bounds,A::Edge},
    {A::Move,A::Path,A::Easing,A::EndBehavior,A::Pause,A::Resume,A::Cancel,A::Follow,A::Shake,A::ClearShake},
    {A::Cut,A::Blend,A::Return,A::Play,A::Pause,A::Resume,A::Skip,A::Replay,A::Shake,A::ClearShake}}};
}
const char* camera_action_key(CameraAction action) {
    static constexpr const char* keys[]={"strategy","dead_zone","swap","auto_motion","teleport","zoom","bounds","edge",
        "move","path","easing","end","pause","resume","cancel","follow","shake","clear_shake",
        "cut","blend","return_main","play","skip","replay","reset"};
    return keys[static_cast<std::size_t>(action)];
}
bool camera_action_enabled(const CameraDemoState& state,CameraAction action) {
    if(action==A::Reset) return true;
    const auto& list=actions[static_cast<std::size_t>(state.page)];
    if(std::find(list.begin(),list.end(),action)==list.end()) return false;
    const bool busy=state.motion.has_value() || state.blend.has_value() || state.phase==CinematicPhase::Holding;
    switch(action) {
    case A::Pause:return busy && !state.paused;
    case A::Resume:return busy && state.paused;
    case A::Cancel:return state.motion.has_value();
    case A::Return:case A::Skip:return state.frozen;
    case A::DeadZone:return state.strategy==FollowMode::MultiTarget;
    case A::Shake:case A::ClearShake:return !state.paused && !state.blend;
    default:return true;
    }
}
void CameraControlsView::build(elysia::ui::UiWindow& window,std::function<void(CameraPage)> page,
    std::function<void(CameraAction)> action,std::function<void()> back) {
    clear();
    _window=&window;
    using namespace elysia::ui;
    const float width=window.screen_rect().width()-48;
    auto tabs=std::make_unique<UiTabBar>(elysia::core::Rect{0,0,width,44});
    const char* titles[]={"showcase.camera.page_follow","showcase.camera.page_motion","showcase.camera.page_cinematic"};
    for(std::size_t index=0;index<3;++index)(void)tabs->add_tab(ui_text_key(titles[index]));
    _tabs=tabs.get();tabs->set_on_selection_changed([page](auto index){if(index)page(static_cast<CameraPage>(*index));});
    window.add_child(std::move(tabs),ShowcaseFrame::at({24,88,width,44}));window.register_focus_scope(*_tabs);
    for(std::size_t index=0;index<3;++index) {
        const auto& list=actions[index];
        const float button_width=std::max(180.0f,(width-52)/5);
        for(std::size_t first=0;first<list.size();first+=5) {
            auto scroll=std::make_unique<UiScrollContainer>(elysia::core::Rect{0,0,width,48});
            scroll->set_scroll_axis(UiScrollAxis::Horizontal);
            auto row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,5*(button_width+8),44});
            row->set_direction(UiListDirection::Horizontal);row->set_item_spacing(8);
            for(std::size_t i=first;i<std::min(first+5,list.size());++i) {
                const auto id=list[i];
                auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,button_width,44});
                button->set_text_content(ui_text_key(std::string("showcase.camera.")+camera_action_key(id)));
                button->set_on_click([action,id]{action(id);});
                _page_buttons[index][static_cast<std::size_t>(id)]=button.get();row->add_back(std::move(button));
            }
            scroll->set_content(std::move(row));scroll->set_visible(index==0);scroll->set_active(index==0);
            auto* scope=scroll.get();_rows[index][first/5]=scope;
            window.add_child(std::move(scroll),ShowcaseFrame::at({24,132+48.0f*(first/5),width,48}));window.register_focus_scope(*scope);
        }
    }
    auto reset_row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,166,40});
    auto reset=std::make_unique<UiButton>(elysia::core::Rect{0,0,166,40});
    reset->set_text_content(ui_text_key("showcase.camera.reset"));reset->set_on_click([action]{action(A::Reset);});
    reset_row->add_back(std::move(reset));auto* reset_scope=reset_row.get();_reset_scope=reset_scope;
    window.add_child(std::move(reset_row),ShowcaseFrame::at({window.screen_rect().width()-368,window.screen_rect().height()-62,166,40}));window.register_focus_scope(*reset_scope);
    ShowcaseFrame::build_chrome(window,"showcase.camera.title","showcase.camera.description",std::move(back));
    _back_scope=dynamic_cast<UiFocusScope*>(window.child_at(window.child_count()-1));
    auto status=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width,26});_status=status.get();status->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(status),ShowcaseFrame::at({24,264,width,26}));
    auto detail=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width,26});_detail=detail.get();detail->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(detail),ShowcaseFrame::at({24,292,width,26}));
    auto legend=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width-384,38},0,ui_text_key("showcase.camera.legend"));
    legend->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(legend),ShowcaseFrame::at({24,window.screen_rect().height()-68,width-384,38}));
    window.focus_first_available_scope();
}
void CameraControlsView::update(const CameraDemoState& state,elysia::ui::UiTextContent status,elysia::ui::UiTextContent detail) {
    if(_tabs && _tabs->selected_index()!=static_cast<std::size_t>(state.page))_tabs->set_selected_index(static_cast<std::size_t>(state.page));
    if(_status)_status->set_text_content(std::move(status));
    if(_detail)_detail->set_text_content(std::move(detail));
    for(std::size_t p=0;p<3;++p)for(auto* row:_rows[p])if(row){row->set_visible(p==static_cast<std::size_t>(state.page));row->set_active(p==static_cast<std::size_t>(state.page));}
    if(_window) {
        auto* first=dynamic_cast<elysia::ui::UiFocusScope*>(_rows[static_cast<std::size_t>(state.page)][0]);
        auto* second=dynamic_cast<elysia::ui::UiFocusScope*>(_rows[static_cast<std::size_t>(state.page)][1]);
        _window->set_scope_neighbors(*_tabs,{.up=_reset_scope,.down=first});
        _window->set_scope_neighbors(*first,{.up=_tabs,.down=second});
        _window->set_scope_neighbors(*second,{.up=first,.down=_reset_scope});
        _window->set_scope_neighbors(*_reset_scope,{.up=second,.down=_tabs,.right=_back_scope});
        _window->set_scope_neighbors(*_back_scope,{.up=second,.down=_tabs,.left=_reset_scope});
    }
    for(std::size_t p=0;p<3;++p)for(std::size_t i=0;i<_page_buttons[p].size();++i)
        if(auto* button=_page_buttons[p][i])button->set_enabled(p==static_cast<std::size_t>(state.page) && camera_action_enabled(state,static_cast<A>(i)));
}
void CameraControlsView::clear() noexcept {
    if(_tabs)_tabs->set_on_selection_changed({});
    _window=nullptr;_reset_scope=_back_scope=nullptr;
    _tabs=nullptr;_status=_detail=nullptr;for(auto& rows:_rows)rows.fill(nullptr);for(auto& buttons:_page_buttons)buttons.fill(nullptr);
}
}
