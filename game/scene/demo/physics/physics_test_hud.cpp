#include "physics_combat_demo_scene_base.h"
#include "physics_combat_layout.h"
#include "../../example_scene_keys.h"
#include "../../../../engine/localization/localization_service.h"
#include "../../../../engine/ui/window/ui_window.h"
#include "../../../../engine/ui/containers/ui_list_container.h"
#include "../../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../../engine/ui/widgets/ui_button.h"
#include "../../../../engine/ui/widgets/label/ui_label.h"
#include <iomanip>
#include <sstream>

namespace example::scene
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
void PhysicsCombatDemoSceneBase::change_mode(example::demo::physics::ScenarioMode mode)
{
    if(mode==example::demo::physics::ScenarioMode::FreePlay && _own_key==example::scene_keys::Box2DLab)return;
    _mode=mode;
    request_restart();
}
void PhysicsCombatDemoSceneBase::toggle_test_pause()
{
    _test_paused=!_test_paused;
    if(_scenario)_scenario->set_paused(_test_paused);
    else if(_test_paused)pause();else resume();
}
void PhysicsCombatDemoSceneBase::test_single_step()
{
    if(!_test_paused)toggle_test_pause();
    if(_scenario)_scenario->single_step();else _test_single_step=true;
}
void PhysicsCombatDemoSceneBase::build_test_hud()
{
    using namespace elysia::ui;
    using namespace example::demo::physics;
    const auto l=make_physics_test_layout(float(runtime_context().logical_width()),float(runtime_context().logical_height()));
    auto* window=create_and_add_object<UiWindow>(l.viewport,101);
    UiWindowStyleOverrides style;style.draw_background=false;style.draw_border=false;window->set_style_overrides(style);
    if(_scenario)
    {
        window->add_child(label(l.title,ui_text_key(_scenario->descriptor().title_key())),physics_combat_layout_options(l.title));
        window->add_child(label(l.purpose,ui_text_key(_scenario->descriptor().purpose_key())),physics_combat_layout_options(l.purpose));
        window->add_child(label(l.expected,ui_text_key(_scenario->descriptor().expected_key())),physics_combat_layout_options(l.expected));
        auto scroll=std::make_unique<UiScrollContainer>(l.details);
        scroll->set_scroll_axis(UiScrollAxis::Vertical);
        auto list=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,l.details.width()-20,1});
        list->set_item_spacing(6);
        _check_list=list.get();
        for(int i=0;i<9;++i){auto row=label({0,0,l.details.width()-20,24});_test_labels.push_back(row.get());list->add_back(std::move(row));}
        scroll->set_content(std::move(list));
        auto* scope=scroll.get();window->add_child(std::move(scroll),physics_combat_layout_options(l.details));window->register_focus_scope(*scope);
    }
    const float y=_scenario?l.actions[0].y():l.viewport.height()-50;
    auto toolbar=std::make_unique<UiListContainer>(elysia::core::Rect{16,y,l.viewport.width()-32,38});
    toolbar->set_direction(UiListDirection::Horizontal);toolbar->set_item_spacing(8);
    const char* keys[]={"run","pause","single_step","free_play","reset","back"};
    for(int i=0;i<6;++i){
        auto button=std::make_unique<UiButton>(elysia::core::Rect{0,0,l.actions[i].width(),38});
        button->set_text_content(ui_text_key("physics_tests."+std::string(keys[i])));
        if(i==0)button->set_on_click([this]{if(_scenario&&_scenario->result().status==ScenarioStatus::Ready)_scenario->start();else change_mode(ScenarioMode::Verify);});
        if(i==1)button->set_on_click([this]{toggle_test_pause();});
        if(i==2)button->set_on_click([this]{test_single_step();});
        if(i==3){button->set_on_click([this]{change_mode(ScenarioMode::FreePlay);});if(_own_key==example::scene_keys::Box2DLab)button->set_enabled(false);}
        if(i==4)button->set_on_click([this]{request_restart();});
        if(i==5)button->set_on_click([this]{return_to_caller();});
        toolbar->add_back(std::move(button));
    }
    auto* scope=toolbar.get();window->add_child(std::move(toolbar),physics_combat_layout_options({16,y,l.viewport.width()-32,38}));window->register_focus_scope(*scope);
    update_test_hud();
}
void PhysicsCombatDemoSceneBase::update_test_hud()
{
    if(!_scenario||_test_labels.size()!=9)return;
    using namespace example::demo::physics;
    using namespace elysia::ui;
    const auto& r=_scenario->result();
    _test_labels[0]->set_text_content(ui_raw_text(tr(scenario_status_key(r.status))+(_scenario->paused()?" | "+tr("physics_tests.paused"):"")));
    _test_labels[1]->set_text_content(ui_raw_text(tr("physics_tests.steps")+": "+std::to_string(r.steps)+" / "+std::to_string(_scenario->descriptor().max_steps)));
    _test_labels[2]->set_text_content(ui_raw_text(tr("physics_tests.objects")+": "+std::to_string(r.stats.registered_objects)+" / "+std::to_string(r.stats.registered_colliders)));
    _test_labels[3]->set_text_content(ui_raw_text(tr("physics_tests.contacts")+": "+std::to_string(r.stats.contacts)+" | "+tr("physics_tests.awake")+": "+std::to_string(r.stats.awake_bodies)));
    _test_labels[4]->set_text_content(ui_raw_text(tr("physics_tests.dropped")+": "+std::to_string(r.stats.dropped_fixed_steps)));
    std::ostringstream timing;timing<<std::fixed<<std::setprecision(3)<<tr("physics_tests.timing")<<": "<<r.median_ms<<" / "<<r.p95_ms<<" / "<<r.max_ms;
    _test_labels[5]->set_text_content(ui_raw_text(timing.str()));
    _test_labels[6]->set_text_content(ui_raw_text(tr("physics_tests.samples")+": "+std::to_string(r.samples)));
    _test_labels[7]->set_text_content(ui_raw_text(tr("physics_tests.debug")+": "+tr(r.mixed_debug_samples?"physics_tests.mixed":r.debug_geometry?"physics_tests.on":"physics_tests.off")));
    _test_labels[8]->set_text_content(ui_raw_text(tr("physics_tests.failure")+": "+(r.failure.empty()?"—":r.failure)));
    while(_displayed_checks<r.checks.size())
    {
        const auto& c=r.checks[_displayed_checks++];
        _check_list->add_back(label({0,0,260,24},ui_raw_text((c.passed?"✓ ":"✗ ")+c.id)));
        std::ostringstream text;text<<std::setprecision(5)<<tr("physics_tests.actual")<<" "<<c.actual<<" | "<<tr("physics_tests.expected_value")<<" "<<c.expected<<" ± "<<c.tolerance;
        _check_list->add_back(label({0,0,260,24},ui_raw_text(text.str())));
    }
}
}
