#include "game/showcase/camera/camera_controls_view.h"
#include "game/showcase/shared/showcase_frame.h"
#include "engine/ui/containers/ui_scroll_container.h"
namespace example::showcase::camera {
void CameraControlsView::build(elysia::ui::UiWindow& window,std::array<std::function<void()>,7> actions,std::function<void()> back)
{
    using namespace elysia::ui;
    ShowcaseFrame::build_chrome(window,"showcase.camera.title","showcase.camera.description",std::move(back));
    auto scroll=std::make_unique<UiScrollContainer>(elysia::core::Rect{0,0,window.screen_rect().width()-32,44});
    scroll->set_scroll_axis(UiScrollAxis::Horizontal);
    auto row=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,7*172.0f,40});
    row->set_direction(UiListDirection::Horizontal);row->set_item_spacing(8);
    const char* keys[]={"showcase.camera.dead_zone","showcase.camera.swap","showcase.camera.auto_motion","showcase.camera.teleport","showcase.camera.zoom","showcase.camera.bounds","physics_tests.reset"};
    for(int i=0;i<7;++i){auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,164,40});button->set_text_content(ui_text_key(keys[i]));button->set_on_click(std::move(actions[i]));row->add_back(std::move(button));}
    scroll->set_content(std::move(row));auto* scope=scroll.get();window.add_child(std::move(scroll),ShowcaseFrame::at({16,88,window.screen_rect().width()-32,44}));window.register_focus_scope(*scope);
    auto label=std::make_unique<UiLabel>(elysia::core::Rect{0,0,window.screen_rect().width()-32,28});_status=label.get();
    window.add_child(std::move(label),ShowcaseFrame::at({16,136,window.screen_rect().width()-32,28}));window.focus_first_available_scope();
}
}
