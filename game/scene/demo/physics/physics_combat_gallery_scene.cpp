#include "physics_combat_gallery_scene.h"
#include "physics_combat_layout.h"
#include "physics_demo_payload.h"
#include "../../example_scene_keys.h"
#include "../../../../engine/localization/localization_service.h"
#include "../../../../engine/ui/containers/ui_list_container.h"
#include "../../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../../engine/ui/widgets/label/ui_label.h"
#include "../../../../engine/ui/widgets/ui_button.h"
#include "../../../../engine/ui/window/ui_window.h"
#include <stdexcept>

namespace example::scene
{
namespace
{
using namespace elysia::ui;
std::unique_ptr<UiButton> button(std::string text,float width=420)
{
    auto result=std::make_unique<UiButton>(elysia::core::Rect{0,0,width,42});
    result->set_text_content(ui_raw_text(std::move(text)));
    result->set_sounds({.press="system.button_click_down",.click="system.button_click_up"});
    return result;
}
std::string tr(std::string_view key){return std::string(ELYSIA_LOCALIZATION->tr(key));}
}
void PhysicsCombatGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* p=elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if(!p||!elysia::scene::SceneKeys::is_supported(p->return_route.target))
        throw std::logic_error("Physics gallery requires a return route");
    _return_route=p->return_route;
    build_ui();
}
void PhysicsCombatGalleryScene::on_exit()
{
    if(_window){_window->set_active(false);_window->set_visible(false);}
}
void PhysicsCombatGalleryScene::reset()
{
    if(_window)_window->destroy();
    _window=nullptr;_return_route={};_category=0;_pressure_tier=0;
}
void PhysicsCombatGalleryScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                             const std::vector<elysia::input::RawInputEvent> &events)
{
    for(const auto& event:events)
        if (event.type == elysia::input::RawInputEventType::ControlPressed &&
            event.control == elysia::input::RawInputControl::KeyEscape)
        {
            consume_input(event);
            return_to_caller();
            return;
        }
}
void PhysicsCombatGalleryScene::build_ui()
{
    using namespace example::demo::physics;
    if(_window)_window->destroy();
    const float w=float(runtime_context().logical_width()),h=float(runtime_context().logical_height());
    _window=create_and_add_object<UiWindow>(elysia::core::Rect{0,0,w,h},100);
    _window->set_on_cancel([this]{return_to_caller();});
    auto title=std::make_unique<UiLabel>(elysia::core::Rect{24,14,w-48,32},0,ui_text_key("physics_tests.title"));
    title->set_visual_role(UiLabelVisualRole::Title);
    _window->add_child(std::move(title),physics_combat_layout_options({24,14,w-48,32}));
    auto tabs=std::make_unique<UiListContainer>(elysia::core::Rect{24,54,w-48,42});
    tabs->set_direction(UiListDirection::Horizontal);tabs->set_item_spacing(12);
    const char* names[]={"physics","combat","stress"};
    for(int i=0;i<3;++i){auto b=button(tr("physics_tests."+std::string(names[i])),(w-72)/3);
        b->set_on_click([this,i]{_category=i;_rebuild_requested=true;});tabs->add_back(std::move(b));}
    auto* tabs_scope=tabs.get();_window->add_child(std::move(tabs),physics_combat_layout_options({24,54,w-48,42}));_window->register_focus_scope(*tabs_scope);
    const float top=_category==2?154.f:108.f;
    if(_category==2){
        auto tiers=std::make_unique<UiListContainer>(elysia::core::Rect{24,104,w-48,40});tiers->set_direction(UiListDirection::Horizontal);tiers->set_item_spacing(12);
        const char* labels[]={"low","medium","high"};
        for(int i=0;i<3;++i){auto b=button((_pressure_tier==i?"✓ ":"")+tr("physics_tests."+std::string(labels[i])),(w-72)/3);
            b->set_on_click([this,i]{_pressure_tier=i;_rebuild_requested=true;});tiers->add_back(std::move(b));}
        auto* scope=tiers.get();_window->add_child(std::move(tiers),physics_combat_layout_options({24,104,w-48,40}));_window->register_focus_scope(*scope);
    }
    auto scroll=std::make_unique<UiScrollContainer>(elysia::core::Rect{24,top,w-48,h-top-66});scroll->set_scroll_axis(UiScrollAxis::Vertical);
    auto list=std::make_unique<UiListContainer>(elysia::core::Rect{0,0,w-72,1});list->set_item_spacing(10);
    if(_category==1){
        for(const auto& c:physics_scenarios())if(c.category==ScenarioCategory::Combat){auto b=button(tr("physics_tests.free_play")+" — "+tr(c.title_key()),w-72);
            b->set_on_click([this,id=std::string(c.id),scene=c.scene]{request_scene_switch(scene,PhysicsDemoPayload{make_menu_route(),id,ScenarioMode::FreePlay,0},elysia::scene::SceneReloadMode::Recreate);});list->add_back(std::move(b));}
    }
    for(const auto& c:physics_scenarios())if(int(c.category)==_category){
        const int tier=c.category==ScenarioCategory::Stress?_pressure_tier:0;
        const auto* recent=recent_scenario_result(c.id,tier);
        auto b=button(tr(c.title_key())+" — "+tr(scenario_status_key(recent?recent->status:ScenarioStatus::Ready)),w-72);
        b->set_on_click([this,id=std::string(c.id),scene=c.scene,tier]{request_scene_switch(scene,PhysicsDemoPayload{make_menu_route(),id,ScenarioMode::Verify,tier},elysia::scene::SceneReloadMode::Recreate);});
        list->add_back(std::move(b));
        auto description=std::make_unique<UiLabel>(elysia::core::Rect{0,0,w-72,30},0,ui_text_key(c.purpose_key()));
        description->set_text_fit_mode(UiLabelTextFitMode::ShrinkToFit);list->add_back(std::move(description));
    }
    scroll->set_content(std::move(list));auto* scope=scroll.get();_window->add_child(std::move(scroll),physics_combat_layout_options({24,top,w-48,h-top-66}));_window->register_focus_scope(*scope);
    auto footer=std::make_unique<UiListContainer>(elysia::core::Rect{24,h-52,w-48,42});
    auto back=button(tr("physics_tests.back"),w-48);back->set_on_click([this]{return_to_caller();});footer->add_back(std::move(back));
    auto* footer_scope=footer.get();_window->add_child(std::move(footer),physics_combat_layout_options({24,h-52,w-48,42}));_window->register_focus_scope(*footer_scope);
    _window->focus_first_available_scope();
}
void PhysicsCombatGalleryScene::on_update(double delta)
{
    if(_rebuild_requested){_rebuild_requested=false;build_ui();}
    elysia::scene::Scene::on_update(delta);
}
void PhysicsCombatGalleryScene::return_to_caller()
{
    if(elysia::scene::SceneKeys::is_supported(_return_route.target))request_scene_switch(_return_route);
}
elysia::scene::SceneRoute PhysicsCombatGalleryScene::make_menu_route() const
{
    return {.target=example::scene_keys::PhysicsCombatGallery,.payload=DemoScenePayload{.return_route=_return_route},.reload_mode=elysia::scene::SceneReloadMode::Reuse};
}
}
