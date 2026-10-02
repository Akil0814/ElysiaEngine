#include "game/showcase/scenarios/scenario_scene.h"
#include "game/showcase/scenarios/showcase_scenario_presentation.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "game/showcase/shared/showcase_layout.h"
#include "game/navigation/showcase_scene_keys.h"
#include "engine/physics/physics_debug_draw.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include <algorithm>
#include <stdexcept>
namespace example::scene
{
using namespace example::showcase::scenarios;
ScenarioScene::ScenarioScene(elysia::scene::SceneKey key,bool gameplay)
    : Scene(elysia::scene::SceneRuntimeFeatures{
        .camera=elysia::scene::CameraSceneConfig{.initial_slot=elysia::camera::CameraSlot::Main,
            .owned_slots=elysia::camera::CameraSlot::Main}}),_key(key),_gameplay(gameplay) {}
GameplayVerificationScene::GameplayVerificationScene():ScenarioScene(example::scene_keys::GameplayVerification,true) {}
ScenarioScene::~ScenarioScene() { release_global_state(); }
void ScenarioScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* entry=elysia::scene::try_scene_payload<ScenarioEnterPayload>(payload);
    if (!entry) throw std::logic_error("ScenarioScene requires ScenarioEnterPayload.");
    const auto* descriptor=find_showcase_scenario(entry->scenario_id);
    if (!descriptor || (descriptor->category==ScenarioCategory::Gameplay)!=_gameplay
        || !elysia::scene::SceneKeys::is_supported(entry->return_route.target)
        || entry->pressure_tier<0 || entry->pressure_tier>2
        || (descriptor->category!=ScenarioCategory::Stress && entry->pressure_tier!=0))
        throw std::logic_error("ScenarioScene received an invalid scenario route.");
    _entry=*entry;
    clear_presentation();
    auto* debug=elysia::tools::DebugDraw::instance();
    _previous_enabled=debug->enabled(); _previous_categories=debug->enabled_categories(); _captured=true;
    try {
        _scenario=std::make_unique<ShowcaseScenario>(_entry.scenario_id,_entry.pressure_tier);
        _presentation=create_and_add_object<ShowcaseScenarioPresentation>(*_scenario);
        debug->set_enabled(descriptor->category!=ScenarioCategory::Stress);
        debug->set_enabled_categories(elysia::tools::DebugDrawCategory::All);
        const auto layout=make_scenario_layout(float(runtime_context().logical_width()),float(runtime_context().logical_height()));
        _window=create_and_add_object<elysia::ui::UiWindow>(layout.viewport,101);
        _hud.build(*_window,*_scenario,{[this]{run();},[this]{pause_case();},[this]{step();},[this]{restart();},[this]{back();}});
        _inspector.attach(runtime_context().development_panels(),_entry.scenario_id,
            [this]() -> const elysia::physics::PhysicsWorld& {return _scenario->world();},
            [this]{return _scenario->result().fixed_step_stats;},elysia::scene::FixedStepConfig{});
        const auto bounds=_scenario->bounds();
        const float zoom=std::min(layout.arena.width()/bounds.width(),layout.arena.height()/bounds.height())*.9f;
        auto& cameras=camera_runtime(); const auto slot=elysia::camera::CameraSlot::Main;
        cameras.set_zoom(slot,zoom); cameras.set_world_bounds(slot,std::nullopt); cameras.set_focus(slot,std::nullopt);
        cameras.set_center(slot,bounds.center()-(layout.arena.center()-layout.viewport.center())/zoom);
    } catch (...) { release_global_state(); clear_presentation(); throw; }
}
void ScenarioScene::on_exit()
{
    if (_scenario) remember_scenario_result(_entry.scenario_id,_entry.pressure_tier,_scenario->result());
    release_global_state();
    clear_presentation();
}
void ScenarioScene::on_reset() { release_global_state(); clear_presentation(); }
void ScenarioScene::clear_presentation() noexcept
{
    _hud.clear();
    if (_window) _window->destroy();
    if (_presentation) _presentation->destroy();
    _window=nullptr;
    _presentation=nullptr;
    _scenario.reset();
}
void ScenarioScene::release_global_state() noexcept
{
    _inspector.detach();
    if (!_captured) return;
    auto* debug=elysia::tools::DebugDraw::instance();
    debug->clear_categories(elysia::tools::DebugDrawCategory::All);
    debug->set_enabled_categories(_previous_categories); debug->set_enabled(_previous_enabled); _captured=false;
}
void ScenarioScene::on_before_update(double delta)
{
    if (!_scenario) return;
    auto* debug=elysia::tools::DebugDraw::instance(); _scenario->set_debug_geometry(debug->enabled());
    _scenario->advance(delta);
    if (debug->enabled()) elysia::physics::submit_physics_debug_snapshot(_scenario->world().debug_snapshot(),*debug);
    _hud.update(*_scenario);
}
void ScenarioScene::run() { if (_scenario->result().status==ScenarioStatus::Ready) _scenario->start(); else restart(); }
void ScenarioScene::pause_case() { _scenario->set_paused(!_scenario->paused()); }
void ScenarioScene::step() { _scenario->set_paused(true); _scenario->single_step(); }
void ScenarioScene::restart() { request_scene_switch(_key,_entry,elysia::scene::SceneReloadMode::Recreate); }
void ScenarioScene::back() { request_scene_switch(_entry.return_route); }
void ScenarioScene::on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>& events)
{
    using namespace elysia::input;
    for (const auto& event:events) {
        if (event.type!=RawInputEventType::ControlPressed) continue;
        switch(event.control) {
        case RawInputControl::KeyEscape: consume_input(event); back(); return;
        case RawInputControl::KeyR: consume_input(event); restart(); return;
        case RawInputControl::KeyP: consume_input(event); pause_case(); return;
        case RawInputControl::KeyN: consume_input(event); step(); return;
        case RawInputControl::KeyF1: consume_input(event); elysia::tools::DebugDraw::instance()->set_enabled(!elysia::tools::DebugDraw::instance()->enabled()); return;
        default: break;
        }
    }
}
}
