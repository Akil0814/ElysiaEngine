#include "game/gameplay/control/local_controls.h"
#include "game/showcase/gameplay/gameplay_demo_scene_base.h"
#include "game/showcase/shared/showcase_layout.h"
#include "game/navigation/showcase_scene_keys.h"
#include "engine/physics/physics_debug_draw.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/localization/localization_service.h"

#include "engine/camera/camera_manager.h"
#include "engine/core/render/colors.h"
#include "engine/input/raw_input_types.h"
#include "engine/tools/debug_draw.h"
#include "engine/ui/style/ui_visual_styles.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/text/ui_text_content.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_bar.h"
#include "engine/ui/window/ui_window.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/tools/logger.h"

#if ELYSIA_ENABLE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace example::scene
{
GameplayDemoSceneBase::GameplayDemoSceneBase(
    elysia::scene::SceneKey own_key,
    std::string scene_name,
    elysia::physics::PhysicsWorldConfig config)
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .physics = config,
          .gameplay_collision = true,
          .camera = elysia::scene::CameraSceneConfig{
              .initial_slot = elysia::camera::CameraSlot::Main,
              .owned_slots = elysia::camera::CameraSlot::Main,
              .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}}),
      _own_key(own_key),
      _scene_name(std::move(scene_name)),
      _combat(physics_world(), collision_runtime())
{
    _combat.set_death_callback(
        [this](auto& actor) { handle_actor_death(actor); });
}

GameplayDemoSceneBase::~GameplayDemoSceneBase()
{
    unregister_physics_inspector();
}

void GameplayDemoSceneBase::on_enter(
    const elysia::scene::ScenePayload& payload)
{
    const auto* value = elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!value || !elysia::scene::SceneKeys::is_supported(value->return_route.target))
        throw std::logic_error(_scene_name + " requires ShowcaseEnterPayload with a valid return route.");
    _return_route = value->return_route;
    auto* debug = elysia::tools::DebugDraw::instance();
    _previous_debug_enabled = debug->enabled();
    _previous_debug_categories = debug->enabled_categories();
    try
    {
    debug->set_enabled(true);
    debug->set_enabled_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider
        | elysia::tools::DebugDrawCategory::PhysicsContact
        | elysia::tools::DebugDrawCategory::PhysicsContactNormal
        | elysia::tools::DebugDrawCategory::PhysicsJoint
        | elysia::tools::DebugDrawCategory::PhysicsVelocity
        | elysia::tools::DebugDrawCategory::Gameplay);
    if (!_built)
    {
        build_demo();
        build_hud();
        build_controls();
        _built = true;
    }
    if (_player && !_player->is_destroyed()) configure_player_controller(*_player);
    configure_fixed_camera();
    if (_tile_map && physics_world().tile_world() != _tile_map)
        (void)physics_world().set_tile_world(*_tile_map);
    register_physics_inspector();
    }
    catch (...)
    {
        debug->set_enabled_categories(_previous_debug_categories);
        debug->set_enabled(_previous_debug_enabled);
        throw;
    }
}

void GameplayDemoSceneBase::on_exit()
{
    unregister_physics_inspector();
    if (_tile_map && physics_world().tile_world() == _tile_map)
        (void)physics_world().clear_tile_world(*_tile_map);
    auto* debug = elysia::tools::DebugDraw::instance();
    debug->clear_categories(elysia::tools::DebugDrawCategory::Gameplay);
    debug->set_enabled_categories(_previous_debug_categories);
    debug->set_enabled(_previous_debug_enabled);
}

void GameplayDemoSceneBase::on_reset()
{
    unregister_physics_inspector();
    _restart_remaining = -1.0;
    _restart_requested = false;
}

void GameplayDemoSceneBase::on_before_update(double delta)
{
    const bool single=_single_step;
    _single_step=false;
    if(single)resume();
    _frame_simulation_delta=single?1.0/60:(_demo_paused?0:delta);
    _combat.update(_frame_simulation_delta);
}

void GameplayDemoSceneBase::on_after_update(double delta)
{
    (void)delta;
    if(_demo_paused)pause();
    _combat.flush_deaths();
    update_hud();

    if (_restart_remaining >= 0.0)
    {
        _restart_remaining -= std::max(0.0, _frame_simulation_delta);
        if (_restart_remaining <= 0.0)
            request_restart();
    }
}

double GameplayDemoSceneBase::fixed_step_frame_delta(double delta) const
{
    (void)delta;
    return _frame_simulation_delta;
}

void GameplayDemoSceneBase::on_shortcuts(const elysia::input::RawInputFrame &input,
                                              const std::vector<elysia::input::RawInputEvent> &events)
{
    for (const auto& event : events)
    {
        if (event.type != elysia::input::RawInputEventType::ControlPressed)
            continue;
        if (event.control == elysia::input::RawInputControl::KeyR)
        {
            consume_input(event);
            request_restart();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyEscape)
        {
            consume_input(event);
            return_to_caller();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyF1)
        {
            consume_input(event);
            auto* debug = elysia::tools::DebugDraw::instance();
            debug->set_enabled(!debug->enabled());
        }
        if (event.control == elysia::input::RawInputControl::KeyP)
        {
            consume_input(event);
            toggle_pause();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyN)
        {
            consume_input(event);
            single_step();
            return;
        }
    }
}

void GameplayDemoSceneBase::on_control_target_removing(
    elysia::core::SceneObject& object)
{
    if (auto* actor = dynamic_cast<example::showcase::gameplay::BlockCombatActor*>(&object))
    {
        _combat.unregister_actor(*actor);
        std::erase(_actors, actor);
        if (_player == actor)
            _player = nullptr;
    }
    if (_tile_map == &object)
        _tile_map = nullptr;
}

std::optional<elysia::camera::CameraFocus>
GameplayDemoSceneBase::resolve_camera_focus(elysia::camera::CameraSlot slot) const
{
    if (slot != elysia::camera::CameraSlot::Main)
        return std::nullopt;
    if (!_player)
        return std::nullopt;

    const auto rect = _player->render_rect();
    return elysia::camera::CameraFocus{rect, rect};
}


void GameplayDemoSceneBase::configure_player_controller(example::showcase::gameplay::BlockCombatActor& player) {
    if(dynamic_cast<elysia::gameplay::ControlCommandReceiver*>(&player)) example::gameplay::configure_scene_player(*this,_controller,player);
}
void GameplayDemoSceneBase::set_player(
    example::showcase::gameplay::BlockCombatActor& player) noexcept
{
    _player = &player;

}

void GameplayDemoSceneBase::set_demo_camera_center(
    const elysia::core::Vector2& center) noexcept
{
    _demo_camera_center = center;
}

void GameplayDemoSceneBase::bind_tile_map(
    example::showcase::gameplay::DemoTileMap& tile_map)
{
    _tile_map = &tile_map;
    (void)physics_world().set_tile_world(tile_map);
}

void GameplayDemoSceneBase::register_physics_inspector()
{
    _inspector.attach(runtime_context().development_panels(),_scene_name,
        [this]() -> const elysia::physics::PhysicsWorld& {return physics_world();},
        [this]{return *fixed_step_stats();},fixed_step_config()?std::optional(*fixed_step_config()):std::nullopt);
}
void GameplayDemoSceneBase::unregister_physics_inspector() noexcept { _inspector.detach(); }

void GameplayDemoSceneBase::build_hud()
{
    _hud=create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},100);
    const char* title=_own_key==example::scene_keys::ColliderCombatDemo?"showcase.gameplay.collider":_own_key==example::scene_keys::PlatformTileCombatDemo?"showcase.gameplay.platform":"showcase.gameplay.topdown";
    const char* controls=_own_key==example::scene_keys::ColliderCombatDemo?"showcase.gameplay.collider_controls":_own_key==example::scene_keys::PlatformTileCombatDemo?"showcase.gameplay.platform_controls":"showcase.gameplay.topdown_controls";
    _hud_view.build(*_hud,title,controls);
}

void GameplayDemoSceneBase::configure_fixed_camera()
{
    if (!_demo_camera_center)
        return;

    auto& cameras = camera_runtime();
    constexpr auto slot = elysia::camera::CameraSlot::Main;
    cameras.set_follow_strategy(
        slot, std::make_unique<elysia::camera::HardFollowStrategy>());
    cameras.set_focus(slot, std::nullopt);
    cameras.set_world_bounds(slot, std::nullopt);
    cameras.set_zoom(slot, 2.0f);
    cameras.set_center(slot, *_demo_camera_center);
}

void GameplayDemoSceneBase::update_hud()
{
    std::size_t enemies=0;
    for (auto* actor:_actors) if(actor && actor!=_player && !actor->is_destroyed() && actor->alive()) ++enemies;
    const auto description=elysia::gameplay::ControllerService::instance()->describe(_controller);
    _hud_view.update({.health=_player?_player->health().current():0,.maximum=_player?_player->health().maximum():0,
        .enemies=enemies,.physics=physics_world().last_step_stats(),.dropped=fixed_step_stats()->dropped_steps,
        .bound=description && description->bound,
        .attacking=_player && _player->attacking(),.alive=_player && _player->alive()});
}

void GameplayDemoSceneBase::handle_actor_death(
    example::showcase::gameplay::BlockCombatActor& actor)
{
    if (&actor == _player)
    {
        _restart_remaining = 1.5;
        _hud_view.defeated();
        return;
    }
    actor.set_visible(false);
    actor.set_active(false);
    actor.destroy();
}

void GameplayDemoSceneBase::request_restart()
{
    if (_restart_requested)
        return;
    _restart_requested = true;
    request_scene_switch(
        _own_key,
        ShowcaseEnterPayload{.return_route = _return_route},
        elysia::scene::SceneReloadMode::Recreate);
}

void GameplayDemoSceneBase::return_to_caller()
{
    if (elysia::scene::SceneKeys::is_supported(_return_route.target))
        request_scene_switch(_return_route);
}
}
