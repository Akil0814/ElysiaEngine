#pragma once
#include "game/showcase/shared/physics_inspector.h"
#include "game/showcase/gameplay/gameplay_hud_view.h"

#include "game/showcase/gameplay/runtime/block_actor.h"
#include "game/showcase/gameplay/runtime/demo_tile_map.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/tools/debug_draw.h"
#include "engine/tools/development_overlay.h"

#include <string>
#include <vector>

namespace elysia::ui
{
class UiBar;
class UiLabel;
class UiWindow;
class UiListContainer;
}

namespace example::scene
{
class GameplayDemoSceneBase : public elysia::gameplay::GameplayScene
{
public:
    GameplayDemoSceneBase(
        elysia::scene::SceneKey own_key,
        std::string scene_name,
        elysia::physics::PhysicsWorldConfig config);
    ~GameplayDemoSceneBase() override;

protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
    void on_before_update(double delta) override;
    void on_after_update(double delta) override;
    [[nodiscard]] double fixed_step_frame_delta(double delta) const override;
    void on_shortcuts(const elysia::input::RawInputFrame &input,
                      const std::vector<elysia::input::RawInputEvent> &events) override;
    void on_control_target_removing(elysia::core::SceneObject& object) override;

    elysia::gameplay::ControllerHandle& player_controller() { return _controller; }
    virtual void configure_player_controller(example::showcase::gameplay::BlockCombatActor&);
    virtual void build_demo() = 0;
    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus(
        elysia::camera::CameraSlot slot) const override;

    template <typename T, typename... Args>
    T* add_actor(Args&&... args)
    {
        T* actor = create_and_add_object<T>(
            std::forward<Args>(args)...);
        if (!actor || !actor->bind_combat(_combat))
            return nullptr;
        _actors.push_back(actor);
        return actor;
    }

    void set_player(example::showcase::gameplay::BlockCombatActor& player) noexcept;
    void set_demo_camera_center(const elysia::core::Vector2& center) noexcept;
    void bind_tile_map(example::showcase::gameplay::DemoTileMap& tile_map);
    [[nodiscard]] example::showcase::gameplay::DemoCombatSession& combat() noexcept { return _combat; }

private:
    elysia::gameplay::ControllerHandle _controller;
    void build_controls();
    void toggle_pause();
    void single_step();
    bool _demo_paused = false;
    bool _single_step = false;
    double _frame_simulation_delta = 0.0;
    bool _world_ui_visible = true;
    void toggle_world_ui();
    void show_speech();
    void register_physics_inspector();
    void unregister_physics_inspector() noexcept;
    void build_hud();
    void configure_fixed_camera();
    void update_hud();
    void handle_actor_death(example::showcase::gameplay::BlockCombatActor& actor);
    void request_restart();
    void return_to_caller();

    elysia::scene::SceneKey _own_key{};
    elysia::scene::SceneRoute _return_route;
    std::string _scene_name;
    example::showcase::gameplay::DemoCombatSession _combat;
    example::showcase::gameplay::BlockCombatActor* _player = nullptr;
    std::optional<elysia::core::Vector2> _demo_camera_center;
    example::showcase::gameplay::DemoTileMap* _tile_map = nullptr;
    std::vector<example::showcase::gameplay::BlockCombatActor*> _actors;
    elysia::ui::UiWindow* _hud = nullptr;
    example::showcase::gameplay::GameplayHudView _hud_view;
    double _restart_remaining = -1.0;
    bool _built = false;
    bool _restart_requested = false;
    example::showcase::PhysicsInspector _inspector;
    bool _previous_debug_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
