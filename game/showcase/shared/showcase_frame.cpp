#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/ui_button.h"
#include <algorithm>

namespace example::showcase
{
using namespace elysia::ui;
void ShowcaseFrame::build_chrome(UiWindow& window,const char* title,const char* description,std::function<void()> back)
{
    const float width=window.screen_rect().width();
    auto heading=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width-48,34},0,ui_text_key(title));
    heading->set_visual_role(UiLabelVisualRole::Title);
    window.add_child(std::move(heading),at({24,12,width-48,34}));
    auto summary=std::make_unique<UiLabel>(elysia::core::Rect{0,0,width-48,28},0,ui_text_key(description));
    summary->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(summary),at({24,50,width-48,28}));
    auto footer=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,166,40});
    auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,166,40});
    button->set_text_content(ui_text_key("showcase.back"));button->set_on_click(back);
    footer->add_back(std::move(button));auto* scope=footer.get();
    window.add_child(std::move(footer),at({width-190,window.screen_rect().height()-62,166,40}));
    window.register_focus_scope(*scope);window.set_on_cancel(std::move(back));
}
UiLayoutChildOptions ShowcaseFrame::at(const elysia::core::Rect& rect)
{
    UiLayoutChildOptions options;
    options._anchor = UiLayoutAnchor::TopLeft;
    options._margin.left = rect.x(); options._margin.top = rect.y();
    options._size_override = rect.size(); options._use_size_override = true;
    return options;
}
void ShowcaseFrame::build(UiWindow& window, const char* title, const char* description,
                         std::function<void()> back, bool overlay)
{
    clear();
    const auto bounds = window.screen_rect();
    const float width = std::max(0.0f, bounds.width()-48);
    UiWindowStyleOverrides style;
    style.draw_background = !overlay; style.draw_border = false;
    window.set_style_overrides(style);
    auto heading = std::make_unique<UiLabel>(elysia::core::Rect{24,12,width,34},0,ui_text_key(title));
    heading->set_visual_role(UiLabelVisualRole::Title);
    window.add_child(std::move(heading),at({24,12,width,34}));
    auto summary = std::make_unique<UiLabel>(elysia::core::Rect{24,50,width,28},0,ui_text_key(description));
    summary->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);
    window.add_child(std::move(summary),at({24,50,width,28}));
    auto scroll = std::make_unique<UiScrollContainer>(elysia::core::Rect{24,88,width,std::max(40.0f,bounds.height()-170)});
    scroll->set_scroll_axis(UiScrollAxis::Vertical);
    auto list = std::make_unique<UiListContainer>(elysia::core::Rect{0,0,width-20,1});
    list->set_item_spacing(10); _content = list.get();
    scroll->set_content(std::move(list));
    auto* scope=scroll.get();
    window.add_child(std::move(scroll),at({24,88,width,std::max(40.0f,bounds.height()-170)}));
    window.register_focus_scope(*scope);
    auto status=std::make_unique<UiLabel>(elysia::core::Rect{24,bounds.height()-72,width-180,26});
    _status=status.get(); window.add_child(std::move(status),at({24,bounds.height()-72,width-180,26}));
    auto button=std::make_unique<UiButton>(elysia::core::Rect{bounds.width()-190,bounds.height()-62,166,40});
    button->set_text_content(ui_text_key("showcase.back")); button->set_on_click(back);
    auto footer=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,166,40});
    footer->add_back(std::move(button)); auto* footer_scope=footer.get();
    window.add_child(std::move(footer),at({bounds.width()-190,bounds.height()-62,166,40}));
    window.register_focus_scope(*footer_scope);
    window.set_on_cancel(std::move(back)); window.focus_first_available_scope();
}
UiButton* ShowcaseFrame::add_action(const char* key,std::function<void()> action)
{
    auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,_content->screen_rect().width(),42});
    button->set_text_content(ui_text_key(key));
    button->set_sounds({.press="system.button_click_down",.click="system.button_click_up"});
    button->set_on_click(std::move(action)); auto* result=button.get();
    _content->add_back(std::move(button)); return result;
}
void ShowcaseFrame::set_status(UiTextContent text)
{
    if (_status) _status->set_text_content(std::move(text));
}
}
