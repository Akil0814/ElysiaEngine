#include "game/showcase/ui/ui_component_gallery_scene.h"
namespace example::scene {
void UiComponentGalleryScene::add_hud_demo(elysia::ui::UiListContainer& page) {
    _hud_view.build(page,{[this]{trigger_hud_skill();},[this]{toggle_hud_pause();},[this](std::size_t index){use_hud_item(index);}});
    sync_hud_demo();
}
void UiComponentGalleryScene::trigger_hud_skill() { _hud_state.cast(); sync_hud_demo(); }
void UiComponentGalleryScene::use_hud_item(std::size_t index) { _hud_state.use(index); sync_hud_demo(); }
void UiComponentGalleryScene::toggle_hud_pause() { _hud_state.paused=!_hud_state.paused; sync_hud_demo(); }
void UiComponentGalleryScene::sync_hud_demo() { _hud_view.sync(_hud_state); }
void UiComponentGalleryScene::cancel_hud_demo() { _hud_view.cancel(); }
void UiComponentGalleryScene::on_before_update(double delta)
{
    if (_rebuild_requested) { _rebuild_requested=false;rebuild_ui(); }
    if (!_hud_view.page || !_hud_view.page->is_active())
        return;
    _hud_state.advance(delta);
    sync_hud_demo();
}

void UiComponentGalleryScene::on_routed_input(const elysia::input::InputSnapshot& input)
{
    using namespace elysia::input;
    if (!_hud_view.page || !_hud_view.page->is_active())
        return;
    if (input.focus_lost)
    {
        cancel_hud_demo();
        return;
    }
    // The game owns key mapping and action requests. UI receives feedback only.
    for (const auto& event : input.events)
        if (event.type == RawInputEventType::ControlPressed)
        {
            switch (event.control)
            {
            case RawInputControl::KeyE: trigger_hud_skill(); break;
            case RawInputControl::Key1: use_hud_item(0); break;
            case RawInputControl::Key2: use_hud_item(1); break;
            case RawInputControl::KeyP: toggle_hud_pause(); break;
            default: break;
            }
        }
    bool skill_held = false;
    for (const auto& source : input.sources)
        if (source.source.is_keyboard())
            skill_held = skill_held || source.frame.state.is_pressed(RawInputControl::KeyE);
    _hud_view.skill->set_external_pressed(skill_held);
}
}
