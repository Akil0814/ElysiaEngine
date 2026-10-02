#include "game/showcase/gameplay/gameplay_hud_view.h"
#include "game/showcase/shared/showcase_layout.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/core/render/colors.h"
#include "engine/localization/localization_service.h"
#include <sstream>
namespace example::showcase::gameplay {
void GameplayHudView::build(elysia::ui::UiWindow& window,const char* title_key,const char* controls_key)
{
    const auto layout=example::scene::make_gameplay_layout(window.screen_rect().width(),window.screen_rect().height());
    auto* _hud=&window;
    elysia::ui::UiWindowStyleOverrides style;
    style.draw_background = false;
    style.draw_border = false;
    _hud->set_style_overrides(style);

    auto panel = std::make_unique<elysia::ui::UiPanel>(layout.hud_panel);
    elysia::ui::UiPanelStyleOverrides panel_style;
    panel_style.corner_radius = 10.0f;
    panel_style.draw_background = true;
    panel_style.draw_border = true;
    panel_style.background = elysia::core::colors::slate_blue;
    panel_style.border = elysia::core::colors::steel_blue;
    panel->set_style_overrides(panel_style);
    _hud->add_child(
        std::move(panel), example::scene::showcase_layout_options(layout.hud_panel));

    auto title = std::make_unique<elysia::ui::UiLabel>(
        layout.title, 0,
        elysia::ui::ui_text_key(title_key));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    _hud->add_child(
        std::move(title), example::scene::showcase_layout_options(layout.title));

    auto controls = std::make_unique<elysia::ui::UiLabel>(
        layout.controls, 0,
        elysia::ui::ui_text_key(controls_key));
    _hud->add_child(
        std::move(controls), example::scene::showcase_layout_options(layout.controls));

    auto health = std::make_unique<elysia::ui::UiBar>(
        layout.health);
    health->set_range(0, 100);
    health->set_value(100);
    _health_bar = health.get();
    _hud->add_child(
        std::move(health), example::scene::showcase_layout_options(layout.health));

    auto stats = std::make_unique<elysia::ui::UiLabel>(
        layout.stats);
    _stats_label = stats.get();
    _hud->add_child(
        std::move(stats), example::scene::showcase_layout_options(layout.stats));

    auto status = std::make_unique<elysia::ui::UiLabel>(
        layout.status);
    status->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    status->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    _status_label = status.get();
    _hud->add_child(
        std::move(status), example::scene::showcase_layout_options(layout.status));
}


void GameplayHudView::update(const GameplayHudData& data) {
    if (_health_bar) { _health_bar->set_range(0,float(data.maximum));_health_bar->set_value(float(data.health)); }
    if (!_stats_label) return;
    auto tr=[](const char* key){return std::string(ELYSIA_LOCALIZATION->tr(key));};
    std::ostringstream text;
    text<<tr("showcase.gameplay.health")<<" "<<data.health<<" / "<<data.maximum
        <<" | "<<tr("showcase.gameplay.enemies")<<" "<<data.enemies
        <<" | "<<tr("showcase.gameplay.bound")<<" "<<data.bound
        <<" | "<<tr("showcase.gameplay.attacking")<<" "<<data.attacking
        <<" | "<<tr("physics_tests.contacts")<<" "<<data.physics.contacts
        <<" | "<<tr("physics_tests.dropped")<<" "<<data.dropped;
    _stats_label->set_text_content(elysia::ui::ui_raw_text(text.str()));
}
void GameplayHudView::defeated() { if(_status_label)_status_label->set_text_content(elysia::ui::ui_text_key("showcase.gameplay.defeated")); }
void GameplayHudView::build_controls(elysia::ui::UiWindow& window,GameplayActions actions) {
    using namespace elysia::ui;
    const float width=window.screen_rect().width();
    auto toolbar=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,width-32,40});
    toolbar->set_direction(UiListDirection::Horizontal);toolbar->set_item_spacing(8);
    const char* keys[]={"physics_tests.pause","physics_tests.single_step","physics_tests.reset","showcase.gameplay.world_ui","showcase.gameplay.speech","showcase.back"};
    std::function<void()> callbacks[]={actions.pause,actions.step,actions.reset,actions.world_ui,actions.speech,actions.back};
    for(int i=0;i<6;++i){auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,(width-72)/6,40});button->set_text_content(ui_text_key(keys[i]));button->set_on_click(std::move(callbacks[i]));toolbar->add_back(std::move(button));}
    auto* scope=toolbar.get();window.add_child(std::move(toolbar),example::scene::showcase_layout_options({16,window.screen_rect().height()-50,width-32,40}));window.register_focus_scope(*scope);window.set_on_cancel(actions.back);
}
}
