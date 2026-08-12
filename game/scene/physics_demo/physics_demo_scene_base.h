#pragma once

#include "../../physics_demo/block_actor.h"
#include "../../physics_demo/demo_tile_map.h"
#include "../demo/demo_scene_payload.h"
#include "../../../engine/gameplay/scene/gameplay_scene.h"
#include "../../../engine/tools/debug_draw.h"

#include <string>
#include <vector>

namespace elysia::ui
{
class UiBar;
class UiLabel;
class UiWindow;
}

namespace example::scene
{
class PhysicsDemoSceneBase : public elysia::gameplay::GameplayScene
{
public:
    PhysicsDemoSceneBase(
        elysia::scene::SceneKey own_key,
        std::string scene_name,
        elysia::physics::PhysicsWorldConfig config,
        std::string title,
        std::string controls);
    ~PhysicsDemoSceneBase() override;

    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_update(double delta) override;
    void on_input(const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;

protected:
    virtual void build_demo() = 0;
    [[nodiscard]] std::optional<elysia::core::Rect> resolve_camera_focus_rect() const override;

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

    void set_player(example::physics_demo::BlockCombatActor& player) noexcept;
    void set_demo_camera_center(const elysia::core::Vector2& center) noexcept;
    void bind_tile_map(example::physics_demo::DemoTileMap& tile_map);
    [[nodiscard]] example::physics_demo::DemoCombatSession& combat() noexcept { return _combat; }

private:
    void build_hud();
    void configure_fixed_camera();
    void update_hud();
    void handle_actor_death(example::physics_demo::BlockCombatActor& actor);
    void request_restart();
    void return_to_caller();

    elysia::scene::SceneKey _own_key{};
    elysia::scene::SceneRoute _return_route;
    std::string _scene_name;
    std::string _title;
    std::string _controls;
    example::physics_demo::DemoCombatSession _combat;
    example::physics_demo::BlockCombatActor* _player = nullptr;
    std::optional<elysia::core::Vector2> _demo_camera_center;
    example::physics_demo::DemoTileMap* _tile_map = nullptr;
    std::vector<example::physics_demo::BlockCombatActor*> _actors;
    elysia::ui::UiWindow* _hud = nullptr;
    elysia::ui::UiLabel* _stats_label = nullptr;
    elysia::ui::UiLabel* _status_label = nullptr;
    elysia::ui::UiBar* _health_bar = nullptr;
    double _restart_remaining = -1.0;
    bool _built = false;
    bool _restart_requested = false;
    bool _previous_debug_enabled = false;
    elysia::tools::DebugDrawCategory _previous_debug_categories =
        elysia::tools::DebugDrawCategory::All;
};
}
