#include "game/showcase/scenarios/scenario_hud_view.h"
#include "game/showcase/shared/showcase_layout.h"
#include "game/navigation/showcase_scene_keys.h"
#include "engine/localization/localization_service.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include <iomanip>
#include <sstream>

namespace example::showcase::scenarios
{
namespace
{
std::string tr(std::string_view key){return std::string(ELYSIA_LOCALIZATION->tr(key));}
std::unique_ptr<elysia::ui::UiLabel> label(elysia::core::Rect rect,elysia::ui::UiTextContent content={})
{
    auto l=std::make_unique<elysia::ui::UiLabel>(rect,0,std::move(content));
    l->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit);
    return l;
}
}
void ScenarioHudView::build(elysia::ui::UiWindow& window_ref,const ShowcaseScenario& scenario,ScenarioActions actions)
{
    using namespace elysia::ui;
    using namespace example::scene;
    const auto l=make_scenario_layout(window_ref.screen_rect().width(),window_ref.screen_rect().height());
    clear();
    auto* window=&window_ref;
    UiWindowStyleOverrides style;style.draw_background=false;style.draw_border=false;window->set_style_overrides(style);
    
    {
        window->add_child(label(l.title,ui_text_key(scenario.descriptor().title_key())),showcase_layout_options(l.title));
        window->add_child(label(l.purpose,ui_text_key(scenario.descriptor().purpose_key())),showcase_layout_options(l.purpose));
        window->add_child(label(l.expected,ui_text_key(scenario.descriptor().expected_key())),showcase_layout_options(l.expected));
        auto scroll=std::make_unique<UiScrollContainer>(l.details);
        scroll->set_scroll_axis(UiScrollAxis::Vertical);
        auto list=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,l.details.width()-20,1});
        list->set_item_spacing(6);
        _checks=list.get();
        for(int i=0;i<9;++i){auto row=label({0,0,l.details.width()-20,24});_labels[i]=row.get();list->add_back(std::move(row));}
        scroll->set_content(std::move(list));
        auto* scope=scroll.get();window->add_child(std::move(scroll),showcase_layout_options(l.details));window->register_focus_scope(*scope);
    }
    const float y=l.actions[0].y();
    auto toolbar=std::make_unique<UiListContainer>(elysia::core::Rect{16,y,l.viewport.width()-32,38});
    toolbar->set_direction(UiListDirection::Horizontal); toolbar->set_item_spacing(8);
    const char* keys[]={"run","pause","single_step","reset","back"};
    std::function<void()> callbacks[]={actions.run,actions.pause,actions.step,actions.reset,actions.back};
    for(int i=0;i<5;++i) {
        auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,(l.viewport.width()-64)/5,38});
        button->set_text_content(ui_text_key("physics_tests."+std::string(keys[i])));
        button->set_on_click(std::move(callbacks[i])); toolbar->add_back(std::move(button));
    }
    auto* scope=toolbar.get();window->add_child(std::move(toolbar),showcase_layout_options({16,y,l.viewport.width()-32,38}));window->register_focus_scope(*scope);
    window->set_on_cancel(actions.back); window->focus_first_available_scope();
    update(scenario);
}
void ScenarioHudView::update(const ShowcaseScenario& scenario)
{
    if (!_checks) return;
    using namespace example::scene;
    using namespace elysia::ui;
    const auto& r=scenario.result();
    _labels[0]->set_text_content(ui_raw_text(tr(scenario_status_key(r.status))+(scenario.paused()?" | "+tr("physics_tests.paused"):"")));
    _labels[1]->set_text_content(ui_raw_text(tr("physics_tests.steps")+": "+std::to_string(r.steps)+" / "+std::to_string(scenario.descriptor().max_steps)));
    _labels[2]->set_text_content(ui_raw_text(tr("physics_tests.objects")+": "+std::to_string(r.stats.registered_objects)+" / "+std::to_string(r.stats.registered_colliders)));
    _labels[3]->set_text_content(ui_raw_text(tr("physics_tests.contacts")+": "+std::to_string(r.stats.contacts)+" | "+tr("physics_tests.awake")+": "+std::to_string(r.stats.awake_bodies)));
    _labels[4]->set_text_content(ui_raw_text(tr("physics_tests.dropped")+": "+std::to_string(r.fixed_step_stats.dropped_steps)));
    std::ostringstream timing;timing<<std::fixed<<std::setprecision(3)<<tr("physics_tests.timing")<<": "<<r.median_ms<<" / "<<r.p95_ms<<" / "<<r.max_ms;
    _labels[5]->set_text_content(ui_raw_text(timing.str()));
    _labels[6]->set_text_content(ui_raw_text(tr("physics_tests.samples")+": "+std::to_string(r.samples)));
    _labels[7]->set_text_content(ui_raw_text(tr("physics_tests.debug")+": "+tr(r.mixed_debug_samples?"physics_tests.mixed":r.debug_geometry?"physics_tests.on":"physics_tests.off")));
    _labels[8]->set_text_content(ui_raw_text(tr("physics_tests.failure")+": "+(r.failure.empty()?"—":r.failure)));
    while(_displayed<r.checks.size())
    {
        const auto& c=r.checks[_displayed++];
        _checks->add_back(label({0,0,260,24},ui_raw_text((c.passed?"✓ ":"✗ ")+c.id)));
        std::ostringstream text;text<<std::setprecision(5)<<tr("physics_tests.actual")<<" "<<c.actual<<" | "<<tr("physics_tests.expected_value")<<" "<<c.expected<<" ± "<<c.tolerance;
        _checks->add_back(label({0,0,260,24},ui_raw_text(text.str())));
    }
}
}
