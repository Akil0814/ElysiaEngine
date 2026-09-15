#include "physics_combat_demo_scene_base.h"
#include "physics_combat_layout.h"
#include "../../../demo/physics/physics_scenario_presentation.h"
#include "../../example_scene_keys.h"
#include "../../../../engine/physics/physics_debug_draw.h"
#include "../../../../engine/ui/widgets/ui_button.h"
#include "../../../../engine/ui/containers/ui_list_container.h"
#include "../../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../../engine/localization/localization_service.h"

#include "../../../../engine/camera/camera_manager.h"
#include "../../../../engine/core/render/colors.h"
#include "../../../../engine/input/raw_input_types.h"
#include "../../../../engine/tools/debug_draw.h"
#include "../../../../engine/ui/style/ui_visual_styles.h"
#include "../../../../engine/ui/containers/ui_panel.h"
#include "../../../../engine/ui/text/ui_text_content.h"
#include "../../../../engine/ui/widgets/label/ui_label.h"
#include "../../../../engine/ui/widgets/ui_bar.h"
#include "../../../../engine/ui/window/ui_window.h"
#include "../../../../engine/scene/runtime/scene_runtime_context.h"
#include "../../../../engine/tools/logger.h"

#if ELYSIA_ENABLE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace example::scene
{
PhysicsCombatDemoSceneBase::PhysicsCombatDemoSceneBase(
    elysia::scene::SceneKey own_key,
    std::string scene_name,
    elysia::physics::PhysicsWorldConfig config,
    std::string title,
    std::string controls)
    : GameplayScene(config), _own_key(own_key),
      _scene_name(std::move(scene_name)), _title(std::move(title)),
      _controls(std::move(controls)),
      _combat(physics_world(), collision_runtime())
{
    _combat.set_death_callback(
        [this](auto& actor) { handle_actor_death(actor); });
}

PhysicsCombatDemoSceneBase::~PhysicsCombatDemoSceneBase()
{
    unregister_physics_inspector();
}

void PhysicsCombatDemoSceneBase::on_enter(
    const elysia::scene::ScenePayload& payload)
{
    using namespace example::demo::physics;
    const auto* test_payload=elysia::scene::try_scene_payload<PhysicsDemoPayload>(payload);
    const auto* demo_payload=elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (test_payload)
    {
        _return_route=test_payload->return_route;
        _scenario_id=test_payload->scenario_id;
        _mode=test_payload->mode;
        _pressure_tier=test_payload->pressure_tier;
    }
    else if (demo_payload)
    {
        _return_route=demo_payload->return_route;
        _scenario_id=default_physics_scenario(_own_key);
        _mode=_own_key==example::scene_keys::Box2DLab?ScenarioMode::Verify:ScenarioMode::FreePlay;
    }
    const auto* selected=find_physics_scenario(_scenario_id);
    if ((!test_payload && !demo_payload) || !elysia::scene::SceneKeys::is_supported(_return_route.target)
        || !selected || selected->scene!=_own_key || _pressure_tier<0 || _pressure_tier>2
        || (_mode==ScenarioMode::FreePlay && _own_key==example::scene_keys::Box2DLab))
    {
        throw std::logic_error(
            _scene_name
            + " requires DemoScenePayload with a valid return route.");
    }

    auto* debug = elysia::tools::DebugDraw::instance();
    _previous_debug_enabled = debug->enabled();
    _previous_debug_categories = debug->enabled_categories();
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
        if (_mode==ScenarioMode::Verify)
        {
            _scenario=std::make_unique<PhysicsScenario>(_scenario_id,_pressure_tier);
            create_and_add_object<PhysicsScenarioPresentation>(*_scenario);
            if(selected->category==ScenarioCategory::Stress)debug->set_enabled(false);
        }
        else { build_demo(); build_hud(); }
        build_test_hud();
        _built = true;
    }
    configure_fixed_camera();
    if (_scenario)
    {
        const auto layout=make_physics_test_layout(float(runtime_context().logical_width()),float(runtime_context().logical_height()));
        const auto bounds=_scenario->bounds();
        const float zoom=std::min(layout.arena.width()/bounds.width(),layout.arena.height()/bounds.height())*.9f;
        auto* cameras=elysia::camera::CameraManager::instance();
        cameras->set_follow_strategy(elysia::camera::CameraSlot::Main,std::make_unique<elysia::camera::HardFollowStrategy>());
        cameras->set_focus_rect(elysia::camera::CameraSlot::Main,std::nullopt);
        cameras->set_world_bounds(elysia::camera::CameraSlot::Main,std::nullopt);
        cameras->set_zoom(elysia::camera::CameraSlot::Main,zoom);
        cameras->set_center(elysia::camera::CameraSlot::Main,bounds.center()-(layout.arena.center()-layout.viewport.center())/zoom);
    }
    if (_tile_map && physics_world().tile_world() != _tile_map)
        (void)physics_world().set_tile_world(*_tile_map);
    register_physics_inspector();
}

void PhysicsCombatDemoSceneBase::on_exit()
{
    if(_scenario)example::demo::physics::remember_scenario_result(_scenario_id,_pressure_tier,_scenario->result());
    unregister_physics_inspector();
    if (_tile_map && physics_world().tile_world() == _tile_map)
        (void)physics_world().clear_tile_world(*_tile_map);
    auto* debug = elysia::tools::DebugDraw::instance();
    debug->clear_categories(elysia::tools::DebugDrawCategory::Gameplay);
    debug->set_enabled_categories(_previous_debug_categories);
    debug->set_enabled(_previous_debug_enabled);
}

void PhysicsCombatDemoSceneBase::reset()
{
    unregister_physics_inspector();
    _restart_remaining = -1.0;
    _restart_requested = false;
}

void PhysicsCombatDemoSceneBase::on_update(double delta)
{
    if(_scenario)
    {
        // Only the shared runner advances the verification world.
        elysia::scene::Scene::on_update(delta);
        auto* debug=elysia::tools::DebugDraw::instance();
        _scenario->set_debug_geometry(debug->enabled());
        _scenario->advance(delta);
        if(debug->enabled())elysia::physics::submit_physics_debug_snapshot(_scenario->world().debug_snapshot(),*debug);
        update_test_hud();
        return;
    }
    const bool single=_test_single_step;
    _test_single_step=false;
    if(single)resume();
    const double simulation_delta=single?1.0/60:(_test_paused?0:delta);
    _combat.update(simulation_delta);
    elysia::gameplay::GameplayScene::on_update(single?1.0/60:delta);
    if(_test_paused)pause();
    _combat.flush_deaths();
    update_hud();

    if (_restart_remaining >= 0.0)
    {
        _restart_remaining -= std::max(0.0, simulation_delta);
        if (_restart_remaining <= 0.0)
            request_restart();
    }
}

void PhysicsCombatDemoSceneBase::on_input(
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
            return_to_caller();
            return;
        }
        if (event.control == elysia::input::RawInputControl::KeyF1)
        {
            auto* debug = elysia::tools::DebugDraw::instance();
            debug->set_enabled(!debug->enabled());
        }
        if(event.control==elysia::input::RawInputControl::KeyP){toggle_test_pause();return;}
        if(event.control==elysia::input::RawInputControl::KeyN){test_single_step();return;}
    }
    if(_scenario || _test_paused)elysia::scene::Scene::on_input(input,events);
    else elysia::gameplay::GameplayScene::on_input(input, events);
}

std::optional<elysia::core::Rect>
PhysicsCombatDemoSceneBase::resolve_camera_focus_rect() const
{
    if (!_player)
        return std::nullopt;

    return _player->render_rect();
}

void PhysicsCombatDemoSceneBase::set_player(
    example::demo::physics::BlockCombatActor& player) noexcept
{
    _player = &player;
}

void PhysicsCombatDemoSceneBase::set_demo_camera_center(
    const elysia::core::Vector2& center) noexcept
{
    _demo_camera_center = center;
}

void PhysicsCombatDemoSceneBase::bind_tile_map(
    example::demo::physics::DemoTileMap& tile_map)
{
    _tile_map = &tile_map;
    (void)physics_world().set_tile_world(tile_map);
}

void PhysicsCombatDemoSceneBase::register_physics_inspector()
{
#if ELYSIA_ENABLE_IMGUI
    if (_physics_inspector_panel.is_valid())
        return;
    _development_panels = runtime_context().development_panels();
    if (!_development_panels)
        return;
    _physics_inspector_panel = _development_panels->register_panel(
        "physics_demo.inspector",
        [this]() { draw_physics_inspector(); });
    if (!_physics_inspector_panel.is_valid())
        _development_panels = nullptr;
#endif
}

void PhysicsCombatDemoSceneBase::unregister_physics_inspector() noexcept
{
    if (_development_panels && _physics_inspector_panel.is_valid())
        (void)_development_panels->unregister_panel(_physics_inspector_panel);
    _development_panels = nullptr;
    _physics_inspector_panel = {};
}

#if ELYSIA_ENABLE_IMGUI
void PhysicsCombatDemoSceneBase::draw_physics_inspector()
{
    if (!ImGui::Begin("Physics Inspector###physics_demo.inspector"))
    {
        ImGui::End();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextUnformatted(_scene_name.c_str());
    ImGui::Text("Frame %.3f ms (%.1f FPS)",
        io.DeltaTime * 1000.0f, io.Framerate);

    const auto& inspected = _scenario ? _scenario->world() : physics_world();
    const auto& config = inspected.config();
    if (ImGui::CollapsingHeader(
            "World Configuration", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Fixed step: %.6f s", config.fixed_delta_seconds);
        ImGui::Text("Gravity: (%.2f, %.2f)",
            config.gravity.x, config.gravity.y);
        ImGui::Text("Max catch-up steps: %u",
            config.max_steps_per_advance);
        ImGui::Text("Sub-steps: %u", config.sub_steps);
    }

    const auto& stats = inspected.last_step_stats();
    if (ImGui::CollapsingHeader(
            "Last Fixed Step", ImGuiTreeNodeFlags_DefaultOpen)
        && ImGui::BeginTable("physics_step_stats", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        const auto row = [](const char* label, unsigned long long value)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu", value);
        };
        row("Registered objects", stats.registered_objects);
        row("Registered colliders", stats.registered_colliders);
        row("Contacts", stats.contacts);
        row("Awake bodies", stats.awake_bodies);
        row("Joints", stats.joints);
        ImGui::Text("Step: %.3f ms", stats.step_milliseconds);
        row("Dropped fixed steps", stats.dropped_fixed_steps);
        ImGui::EndTable();
    }

    const auto& snapshot = inspected.debug_snapshot();
    if (ImGui::CollapsingHeader("Debug Snapshot"))
    {
        ImGui::Text("Shapes: %zu", snapshot.shapes.size());
        ImGui::Text("Contacts: %zu", snapshot.contacts.size());
        ImGui::Text("Velocities: %zu", snapshot.velocities.size());
    }

    if (ImGui::CollapsingHeader(
            "Debug Draw", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* debug_draw = elysia::tools::DebugDraw::instance();
        bool enabled = debug_draw->enabled();
        if (ImGui::Checkbox("Enabled", &enabled))
            debug_draw->set_enabled(enabled);

        const auto category_checkbox = [debug_draw](
            const char* label,
            elysia::tools::DebugDrawCategory category)
        {
            bool selected = debug_draw->is_enabled(category);
            if (!ImGui::Checkbox(label, &selected))
                return;
            auto categories = debug_draw->enabled_categories();
            const auto bits = static_cast<std::uint32_t>(categories);
            const auto category_bits = static_cast<std::uint32_t>(category);
            categories = static_cast<elysia::tools::DebugDrawCategory>(
                selected ? bits | category_bits : bits & ~category_bits);
            debug_draw->set_enabled_categories(categories);
        };
        category_checkbox("Collider (render pose)",
            elysia::tools::DebugDrawCategory::PhysicsCollider);
        category_checkbox("Contact",
            elysia::tools::DebugDrawCategory::PhysicsContact);
        category_checkbox("Contact normal",
            elysia::tools::DebugDrawCategory::PhysicsContactNormal);
        category_checkbox("Native AABB (physics step)",
            elysia::tools::DebugDrawCategory::PhysicsBroadPhase);
        category_checkbox("Previous / current physics poses",
            elysia::tools::DebugDrawCategory::PhysicsPoseHistory);
        category_checkbox("Velocity",
            elysia::tools::DebugDrawCategory::PhysicsVelocity);
        category_checkbox("Joint anchors",
            elysia::tools::DebugDrawCategory::PhysicsJoint);
        ImGui::TextWrapped("Green: awake | Blue: asleep/static | Purple: sensor. Contacts and AABBs show the latest physics step.");
        category_checkbox("Gameplay",
            elysia::tools::DebugDrawCategory::Gameplay);
    }

    ImGui::End();
}
#endif

void PhysicsCombatDemoSceneBase::build_hud()
{
    const PhysicsCombatLayout layout = make_physics_combat_layout(
        static_cast<float>(runtime_context().logical_width()),
        static_cast<float>(runtime_context().logical_height()));
    _hud = create_and_add_object<elysia::ui::UiWindow>(
        layout.viewport, 100);
    if (!_hud)
        return;
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
        std::move(panel), physics_combat_layout_options(layout.hud_panel));

    auto title = std::make_unique<elysia::ui::UiLabel>(
        layout.title, 0,
        elysia::ui::ui_raw_text(_title));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    _hud->add_child(
        std::move(title), physics_combat_layout_options(layout.title));

    auto controls = std::make_unique<elysia::ui::UiLabel>(
        layout.controls, 0,
        elysia::ui::ui_raw_text(
#if ELYSIA_ENABLE_IMGUI
            _controls + " | R Reset | F1 Debug | F2 Inspector | Esc Back"));
#else
            _controls + " | R Reset | F1 Debug | Esc Back"));
#endif
    _hud->add_child(
        std::move(controls), physics_combat_layout_options(layout.controls));

    auto health = std::make_unique<elysia::ui::UiBar>(
        layout.health);
    health->set_range(0, 100);
    health->set_value(100);
    _health_bar = health.get();
    _hud->add_child(
        std::move(health), physics_combat_layout_options(layout.health));

    auto stats = std::make_unique<elysia::ui::UiLabel>(
        layout.stats);
    _stats_label = stats.get();
    _hud->add_child(
        std::move(stats), physics_combat_layout_options(layout.stats));

    auto status = std::make_unique<elysia::ui::UiLabel>(
        layout.status);
    status->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    status->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    _status_label = status.get();
    _hud->add_child(
        std::move(status), physics_combat_layout_options(layout.status));
}

void PhysicsCombatDemoSceneBase::configure_fixed_camera()
{
    if (!_demo_camera_center)
        return;

    auto* cameras = elysia::camera::CameraManager::instance();
    constexpr auto slot = elysia::camera::CameraSlot::Main;
    cameras->set_follow_strategy(
        slot, std::make_unique<elysia::camera::HardFollowStrategy>());
    cameras->set_focus_rect(slot, std::nullopt);
    cameras->set_world_bounds(slot, std::nullopt);
    cameras->set_zoom(slot, 2.0f);
    cameras->set_center(slot, *_demo_camera_center);
}

void PhysicsCombatDemoSceneBase::update_hud()
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
             << " | Awake " << stats.awake_bodies
             << " | Contacts " << stats.contacts
             << " | Joints " << stats.joints
             << " | Step ms " << stats.step_milliseconds
             << " | Dropped " << stats.dropped_fixed_steps;
        _stats_label->set_text_content(elysia::ui::ui_raw_text(text.str()));
    }
}

void PhysicsCombatDemoSceneBase::handle_actor_death(
    example::demo::physics::BlockCombatActor& actor)
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

void PhysicsCombatDemoSceneBase::request_restart()
{
    if (_restart_requested)
        return;
    _restart_requested = true;
    request_scene_switch(
        _own_key,
        PhysicsDemoPayload{.return_route = _return_route,.scenario_id=_scenario_id,.mode=_mode,.pressure_tier=_pressure_tier},
        elysia::scene::SceneReloadMode::Recreate);
}

void PhysicsCombatDemoSceneBase::return_to_caller()
{
    if (elysia::scene::SceneKeys::is_supported(_return_route.target))
        request_scene_switch(_return_route);
}
}
