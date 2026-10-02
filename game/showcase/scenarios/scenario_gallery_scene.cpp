#include "game/showcase/scenarios/scenario_gallery_scene.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/scenarios/scenario_enter_payload.h"
#include "game/showcase/scenarios/showcase_scenario.h"
#include "game/navigation/showcase_scene_keys.h"
#include "engine/localization/localization_service.h"
#include <stdexcept>
namespace example::scene
{
using namespace example::showcase::scenarios;
void ScenarioGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* entry=elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!entry || !elysia::scene::SceneKeys::is_supported(entry->return_route.target))
        throw std::logic_error("ScenarioGalleryScene requires a valid ShowcaseEnterPayload.");
    _return_route=entry->return_route; build_ui();
}
void ScenarioGalleryScene::on_exit() { if (_window) { _window->set_visible(false); _window->set_active(false); } }
void ScenarioGalleryScene::on_reset() { _view.clear(); if (_window) _window->destroy(); _window=nullptr; _return_route={}; _tier=0; _rebuild=false; }
void ScenarioGalleryScene::on_before_update(double) { if (_rebuild) { _rebuild=false; build_ui(); } }
elysia::scene::SceneRoute ScenarioGalleryScene::route() const
{ return {.target=_key,.payload=ShowcaseEnterPayload{_return_route},.reload_mode=elysia::scene::SceneReloadMode::Reuse}; }
void ScenarioGalleryScene::back() { request_scene_switch(_return_route); }
void ScenarioGalleryScene::on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event:events) if (event.control==elysia::input::RawInputControl::KeyEscape && event.type==elysia::input::RawInputEventType::ControlPressed) { consume_input(event); back(); return; }
}
void ScenarioGalleryScene::build_ui()
{
    _view.clear(); if (_window) _window->destroy();
    _window=create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},100);
    _view.build(*_window,_gameplay?"showcase.gameplay.title":"showcase.physics.title",_gameplay?"showcase.gameplay.description":"showcase.physics.description",[this]{back();});
    using elysia::scene::SceneReloadMode;
    if (_gameplay) {
        const elysia::scene::SceneKey scenes[]={example::scene_keys::ColliderCombatDemo,example::scene_keys::PlatformTileCombatDemo,example::scene_keys::TopDownTileCombatDemo};
        const char* keys[]={"showcase.gameplay.collider","showcase.gameplay.platform","showcase.gameplay.topdown"};
        for (int i=0;i<3;++i) _view.add_action(keys[i],[this,key=scenes[i]] { request_scene_switch(key,ShowcaseEnterPayload{route()},SceneReloadMode::Recreate); });
        auto capabilities=std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{0,0,_view.content().screen_rect().width(),44},0,elysia::ui::ui_text_key("showcase.gameplay.capabilities"));
        capabilities->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit); _view.content().add_back(std::move(capabilities));
    } else {
        const char* tiers[]={"physics_tests.low","physics_tests.medium","physics_tests.high"};
        for (int i=0;i<3;++i) _view.add_action(tiers[i],[this,i]{_tier=i; _rebuild=true;});
    }
    for (const auto& descriptor:showcase_scenarios()) {
        if ((descriptor.category==ScenarioCategory::Gameplay)!=_gameplay) continue;
        const int tier=descriptor.category==ScenarioCategory::Stress?_tier:0;
        const auto* recent=recent_scenario_result(descriptor.id,tier);
        auto* button=_view.add_action(descriptor.title_key().c_str(),[this,id=std::string(descriptor.id),tier] {
            request_scene_switch(_gameplay?example::scene_keys::GameplayVerification:example::scene_keys::Box2DLab,
                ScenarioEnterPayload{route(),id,tier},SceneReloadMode::Recreate);
        });
        button->set_text_content(elysia::ui::ui_raw_text(std::string(ELYSIA_LOCALIZATION->tr(descriptor.title_key()))+" — "+std::string(ELYSIA_LOCALIZATION->tr(scenario_status_key(recent?recent->status:ScenarioStatus::Ready)))));
        auto description=std::make_unique<elysia::ui::UiLabel>(elysia::core::Rect{0,0,_view.content().screen_rect().width(),28},0,elysia::ui::ui_text_key(descriptor.purpose_key()));
        description->set_text_fit_mode(elysia::ui::UiLabelTextFitMode::ShrinkToFit); _view.content().add_back(std::move(description));
    }
    if (!_gameplay) _view.set_status(elysia::ui::ui_text_key(_tier==0?"physics_tests.low":_tier==1?"physics_tests.medium":"physics_tests.high"));
    _window->focus_first_available_scope();
}
}
