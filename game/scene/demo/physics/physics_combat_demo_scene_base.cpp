#include "physics_combat_demo_scene_base.h"
#include "physics_combat_layout.h"

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
    const DemoScenePayload* demo_payload =
        elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (!demo_payload
        || !elysia::scene::SceneKeys::is_supported(
            demo_payload->return_route.target))
    {
        throw std::logic_error(
            _scene_name
            + " requires DemoScenePayload with a valid return route.");
    }
    _return_route = demo_payload->return_route;

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
    configure_fixed_camera();
    if (_tile_map && physics_world().tile_world() != _tile_map)
        (void)physics_world().set_tile_world(*_tile_map);
    register_physics_inspector();
}

void PhysicsCombatDemoSceneBase::on_exit()
{
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
    }
    elysia::gameplay::GameplayScene::on_input(input, events);
}

std::optional<elysia::core::Rect>
PhysicsCombatDemoSceneBase::resolve_camera_focus_rect() const
{
    return std::nullopt;
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

    const auto& config = physics_world().config();
    if (ImGui::CollapsingHeader(
            "World Configuration", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Fixed step: %.6f s", config.fixed_delta_seconds);
        ImGui::Text("Gravity: (%.2f, %.2f)",
            config.gravity.x, config.gravity.y);
        ImGui::Text("Max catch-up steps: %u",
            config.max_steps_per_advance);
        ImGui::Text("Solver iterations: %u", config.solver_iterations);
    }

    const auto& stats = physics_world().last_step_stats();
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
        row("Broad-phase proxies", stats.broad_phase_proxies);
        row("Broad-phase pairs", stats.broad_phase_pairs);
        row("Narrow-phase tests", stats.narrow_phase_tests);
        row("Contacts", stats.contacts);
        row("Tile samples", stats.tile_samples);
        row("Rejected tile ranges", stats.rejected_tile_candidate_ranges);
        row("CCD hits", stats.ccd_hits);
        row("CCD iterations", stats.ccd_iterations);
        row("Solver iterations", stats.solver_iterations);
        row("Dropped fixed steps", stats.dropped_fixed_steps);
        ImGui::EndTable();
    }

    const auto& snapshot = physics_world().debug_snapshot();
    if (ImGui::CollapsingHeader("Debug Snapshot"))
    {
        ImGui::Text("Shapes: %zu", snapshot.shapes.size());
        ImGui::Text("Pairs: %zu", snapshot.broad_phase_pairs.size());
        ImGui::Text("Tile candidates: %zu", snapshot.tile_candidates.size());
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
        category_checkbox("Collider",
            elysia::tools::DebugDrawCategory::PhysicsCollider);
        category_checkbox("Contact",
            elysia::tools::DebugDrawCategory::PhysicsContact);
        category_checkbox("Contact normal",
            elysia::tools::DebugDrawCategory::PhysicsContactNormal);
        category_checkbox("Broad phase",
            elysia::tools::DebugDrawCategory::PhysicsBroadPhase);
        category_checkbox("CCD",
            elysia::tools::DebugDrawCategory::PhysicsCcd);
        category_checkbox("Velocity",
            elysia::tools::DebugDrawCategory::PhysicsVelocity);
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
    cameras->set_follow_strategy(slot, nullptr);
    cameras->set_focus_rect(slot, std::nullopt);
    cameras->set_world_bounds(slot, std::nullopt);
    cameras->set_zoom(slot, 1.0f);
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
             << " | Pairs " << stats.broad_phase_pairs
             << " | Contacts " << stats.contacts
             << " | CCD " << stats.ccd_iterations
             << " | Tiles " << stats.tile_samples
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
        DemoScenePayload{.return_route = _return_route},
        elysia::scene::SceneReloadMode::Recreate);
}

void PhysicsCombatDemoSceneBase::return_to_caller()
{
    if (elysia::scene::SceneKeys::is_supported(_return_route.target))
        request_scene_switch(_return_route);
}
}
