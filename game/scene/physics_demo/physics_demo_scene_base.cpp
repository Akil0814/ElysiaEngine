#include "physics_demo_scene_base.h"

#include "../example_scene_keys.h"
#include "../../../engine/core/render/colors.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/tools/debug_draw.h"
#include "../../../engine/ui/style/ui_visual_styles.h"
#include "../../../engine/ui/text/ui_text_content.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/ui_bar.h"
#include "../../../engine/ui/window/ui_window.h"

#include <algorithm>
#include <sstream>

namespace example::scene
{
PhysicsDemoSceneBase::PhysicsDemoSceneBase(
    elysia::scene::SceneKey own_key,
    elysia::physics::PhysicsWorldConfig config,
    std::string title,
    std::string controls)
    : GameplayScene(config), _own_key(own_key), _title(std::move(title)),
      _controls(std::move(controls)),
      _combat(physics_world(), collision_runtime())
{
    _combat.set_death_callback(
        [this](auto& actor) { handle_actor_death(actor); });
}

PhysicsDemoSceneBase::~PhysicsDemoSceneBase() = default;

void PhysicsDemoSceneBase::on_enter(const elysia::scene::ScenePayload&)
{
    auto* debug = elysia::tools::DebugDraw::instance();
    _previous_debug_enabled = debug->enabled();
    _previous_debug_categories = debug->enabled_categories();
    debug->set_enabled(true);
    debug->set_enabled_categories(
        elysia::tools::DebugDrawCategory::PhysicsCollider
        | elysia::tools::DebugDrawCategory::PhysicsContact
        | elysia::tools::DebugDrawCategory::PhysicsContactNormal
        | elysia::tools::DebugDrawCategory::PhysicsBroadPhase
        | elysia::tools::DebugDrawCategory::PhysicsCcd
        | elysia::tools::DebugDrawCategory::PhysicsVelocity
        | elysia::tools::DebugDrawCategory::Gameplay);
    if (!_built)
    {
        build_demo();
        build_hud();
        _built = true;
    }
    if (_tile_map && physics_world().tile_world() != _tile_map)
        (void)physics_world().set_tile_world(*_tile_map);
}

void PhysicsDemoSceneBase::on_exit()
{
    if (_tile_map && physics_world().tile_world() == _tile_map)
        (void)physics_world().clear_tile_world(*_tile_map);
    auto* debug = elysia::tools::DebugDraw::instance();
    debug->clear_categories(elysia::tools::DebugDrawCategory::Gameplay);
    debug->set_enabled_categories(_previous_debug_categories);
    debug->set_enabled(_previous_debug_enabled);
}

void PhysicsDemoSceneBase::reset()
{
    _restart_remaining = -1.0;
    _restart_requested = false;
}

void PhysicsDemoSceneBase::on_update(double delta)
{
    _combat.update(delta);
    elysia::gameplay::GameplayScene::on_update(delta);
    _combat.flush_deaths();
    update_hud();

    if (_restart_remaining >= 0.0)
    {
        _restart_remaining -= std::max(0.0, delta);
        if (_restart_remaining <= 0.0)
            request_restart();
    }
}

void PhysicsDemoSceneBase::on_render(SDL_Renderer* renderer)
{
    (void)_solid_texture.ensure(renderer);
    elysia::gameplay::GameplayScene::on_render(renderer);
}

void PhysicsDemoSceneBase::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
    {
        if (event.type != elysia::input::RawInputEventType::ControlPressed)
            continue;
        if (event.control == elysia::input::RawInputControl::KeyR)
        {
            request_restart();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyEscape)
        {
            return_to_demo_menu();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyF1)
        {
            auto* debug = elysia::tools::DebugDraw::instance();
            debug->set_enabled(!debug->enabled());
        }
    }
    elysia::gameplay::GameplayScene::on_input(input, events);
}

std::optional<elysia::core::Rect>
PhysicsDemoSceneBase::resolve_camera_focus_rect() const
{
    if (_player && !_player->is_destroyed())
        return _player->world_rect();
    return std::nullopt;
}

void PhysicsDemoSceneBase::set_player(
    example::physics_demo::BlockCombatActor& player) noexcept
{
    _player = &player;
}

void PhysicsDemoSceneBase::bind_tile_map(
    example::physics_demo::DemoTileMap& tile_map)
{
    _tile_map = &tile_map;
    (void)physics_world().set_tile_world(tile_map);
}

void PhysicsDemoSceneBase::build_hud()
{
    _hud = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0, 0, 1280, 720}, 100);
    if (!_hud)
        return;
    elysia::ui::UiWindowStyleOverrides style;
    style.draw_background = false;
    style.draw_border = false;
    _hud->set_style_overrides(style);

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{18, 12, 520, 34}, 0,
        elysia::ui::ui_raw_text(_title));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    _hud->add_child(std::move(title));

    auto controls = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{18, 50, 900, 28}, 0,
        elysia::ui::ui_raw_text(_controls + " | R Reset | F1 Debug | Esc Back"));
    _hud->add_child(std::move(controls));

    auto health = std::make_unique<elysia::ui::UiBar>(
        elysia::core::Rect{18, 84, 260, 18});
    health->set_range(0, 100);
    health->set_value(100);
    _health_bar = health.get();
    _hud->add_child(std::move(health));

    auto stats = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{18, 108, 900, 26});
    _stats_label = stats.get();
    _hud->add_child(std::move(stats));

    auto status = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{420, 310, 440, 80});
    status->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    status->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    _status_label = status.get();
    _hud->add_child(std::move(status));
}

void PhysicsDemoSceneBase::update_hud()
{
    if (_health_bar && _player)
    {
        _health_bar->set_range(0.0f,
            static_cast<float>(_player->health().maximum()));
        _health_bar->set_value(static_cast<float>(_player->health().current()));
    }
    std::size_t enemies = 0;
    for (auto* actor : _actors)
        if (actor && actor != _player && !actor->is_destroyed() && actor->alive())
            ++enemies;
    if (_stats_label)
    {
        const auto& stats = physics_world().last_step_stats();
        std::ostringstream text;
        text << "Enemies " << enemies
             << " | Pairs " << stats.broad_phase_pairs
             << " | Contacts " << stats.contacts
             << " | CCD " << stats.ccd_iterations
             << " | Tiles " << stats.tile_samples
             << " | Dropped " << stats.dropped_fixed_steps;
        _stats_label->set_text_content(elysia::ui::ui_raw_text(text.str()));
    }
}

void PhysicsDemoSceneBase::handle_actor_death(
    example::physics_demo::BlockCombatActor& actor)
{
    if (&actor == _player)
    {
        _restart_remaining = 1.5;
        if (_status_label)
            _status_label->set_text_content(
                elysia::ui::ui_raw_text("Defeated - restarting..."));
        return;
    }
    actor.set_visible(false);
    actor.set_active(false);
    actor.destroy();
}

void PhysicsDemoSceneBase::request_restart()
{
    if (_restart_requested)
        return;
    _restart_requested = true;
    request_scene_switch(_own_key, {}, elysia::scene::SceneReloadMode::Recreate);
}

void PhysicsDemoSceneBase::return_to_demo_menu()
{
    request_scene_switch(ExampleSceneKeys::PhysicsDemoMenu, {},
        elysia::scene::SceneReloadMode::Reuse);
}
}
