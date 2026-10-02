#include "game/showcase/input/local_controls_view.h"
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/widgets/ui_text_input.h"
namespace example::showcase::input {
void LocalControlsView::build(elysia::ui::UiWindow& window,std::array<std::function<void()>,11> actions,std::function<void()> open,std::function<void()> back)
{
    using namespace elysia::ui;
    ShowcaseFrame::build_chrome(window,"showcase.input.title","showcase.input.description",std::move(back));
    auto text=std::make_unique<UiLabel>(elysia::core::Rect{0,0,1248,28});status=text.get();window.add_child(std::move(text),ShowcaseFrame::at({16,88,1248,28}));
    auto toolbar=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,220,40});
    auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,220,40});button->set_text_content(ui_text_key("showcase.input.menu"));button->set_on_click(std::move(open));toolbar->add_back(std::move(button));
    auto* scope=toolbar.get();window.add_child(std::move(toolbar),ShowcaseFrame::at({16,124,220,40}));window.register_focus_scope(*scope);
    auto list=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,600,540});menu=list.get();list->set_item_spacing(6);
    const char* keys[]={"showcase.input.bind_one","showcase.input.bind_two","showcase.input.release","showcase.input.keyboard_one","showcase.input.keyboard_two","showcase.input.mouse_one","showcase.input.mouse_two","showcase.input.pad_one","showcase.input.pad_two","showcase.input.resume","showcase.back"};
    for(int i=0;i<11;++i){
        if(i==9){auto field=std::make_unique<UiTextInput>(elysia::core::Rect{0,0,580,38});field->set_placeholder_content(ui_text_key("showcase.input.typing"));list->add_back(std::move(field));}
        auto action=std::make_unique<UiButton>(elysia::core::Rect{0,0,580,38});action->set_text_content(ui_text_key(keys[i]));action->set_on_click(std::move(actions[i]));list->add_back(std::move(action));
    }
    window.add_child(std::move(list),{._anchor=UiLayoutAnchor::Center});
    (void)window.register_overlay(*menu,{.open=false,.modal=true});
}
}
