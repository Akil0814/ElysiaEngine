#pragma once
#include "../../../engine/gameplay/scene/gameplay_scene.h"
#include "../../../engine/camera/multi_target_follow_strategy.h"
#include "demo_scene_payload.h"

namespace elysia::ui { class UiLabel; class UiWindow; }
namespace example::scene
{
class MultiTargetCameraScene final : public elysia::gameplay::GameplayScene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_update(double delta) override;
    void on_shortcuts(const elysia::input::RawInputFrame &input,
                      const std::vector<elysia::input::RawInputEvent> &events) override;

  protected:
    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const override;

  protected:
    void on_control_target_removing(elysia::core::SceneObject&) override;
    void on_game_fixed_update(std::uint64_t tick, double delta) override;

  private:
    class CameraOverlay;
    CameraOverlay* _overlay = nullptr;
    elysia::gameplay::ControllerHandle _controller;
    void reset_demo();
    void install_strategy();
    void toggle_bounds();
    void return_to_caller();
    void build_controls();
    void refresh_status();
    elysia::scene::SceneRoute _return_route;
    std::array<elysia::core::GameObject*, 2> _targets{};
    elysia::ui::UiWindow* _controls = nullptr;
    elysia::ui::UiLabel* _status = nullptr;
    elysia::camera::MultiTargetFollowStrategy* _strategy = nullptr;
    std::size_t _primary = 0;
    bool _automatic = false;
    bool _dead_zone = true;
    bool _bounds = false;
    double _time = 0;
};
}
