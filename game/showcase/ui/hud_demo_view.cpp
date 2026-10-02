#include "game/showcase/ui/hud_demo_view.h"
#include <string>
namespace example::showcase::ui {
void HudDemoView::build(elysia::ui::UiListContainer& page,HudDemoActions actions)
{
    _actions=std::move(actions);
    using namespace elysia::ui;
    auto description = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,760,32 },0,
        ui_text_key("ui_component_gallery.hud.description"));
    description->set_visual_role(UiLabelVisualRole::Muted);
    page.add_back(std::move(description));

    auto row = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,760,92 });
    row->set_direction(UiListDirection::Horizontal);
    row->set_item_spacing(12);
    const auto make_slot = [](const char* content,const char* hint) {
        auto slot = std::make_unique<UiActionButton>(elysia::core::Rect{ 0,0,128,80 });
        slot->set_content(ui_text_key(content));
        slot->set_key_hint(ui_raw_text(hint));
        return slot;
    };
    auto skill = make_slot("ui_component_gallery.hud.skill","E");
    this->skill = skill.get();
    skill->set_on_interaction([this](const UiActionButtonInteraction& event) {
        if (event.phase == UiActionButtonInteractionPhase::Pressed)
            _actions.cast();
    });
    row->add_back(std::move(skill));
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        auto item = make_slot(index == 0 ? "ui_component_gallery.hud.item_one" : "ui_component_gallery.hud.item_two",
            index == 0 ? "1" : "2");
        items[index] = item.get();
        item->set_on_interaction([this,index](const UiActionButtonInteraction& event) {
            if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
                _actions.use(index);
        });
        row->add_back(std::move(item));
    }
    auto pause = make_slot("ui_component_gallery.hud.pause","P");
    this->pause = pause.get();
    pause->set_on_interaction([this](const UiActionButtonInteraction& event) {
        if (event.phase == UiActionButtonInteractionPhase::Released && event.clicked)
            _actions.pause();
    });
    row->add_back(std::move(pause));
    page.add_back(std::move(row));
    auto status = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,760,32 });
    this->status = status.get();
    page.add_back(std::move(status));
}

void HudDemoView::sync(const HudDemoState& state)
{
    using namespace elysia::ui;
    if (!skill)
        return;
    skill->set_enabled(!state.paused && state.cooldown == 0);
    skill->set_overlay_ratio(static_cast<float>(state.cooldown / HudDemoState::kCooldown));
    skill->set_badge_text(ui_raw_text(std::to_string(state.casts)));
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        items[index]->set_enabled(!state.paused && state.item_counts[index] > 0);
        items[index]->set_selected(index == state.selected_item);
        items[index]->set_badge_text(ui_raw_text(std::to_string(state.item_counts[index])));
    }
    pause->set_selected(state.paused);
    pause->set_content(ui_text_key(state.paused ? "ui_component_gallery.hud.resume" : "ui_component_gallery.hud.pause"));
    status->set_text_content(ui_text_key(state.paused ? "ui_component_gallery.hud.paused"
        : state.cooldown > 0 ? "ui_component_gallery.hud.cooldown" : "ui_component_gallery.hud.ready"));
}

void HudDemoView::cancel()
{
    if (skill)
        skill->cancel_input_interaction();
    for (auto* item : items)
        if (item) item->cancel_input_interaction();
    if (pause)
        pause->cancel_input_interaction();
}

}
