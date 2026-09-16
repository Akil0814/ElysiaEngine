#pragma once
#include "../../../engine/scene/scene.h"
#include "../../../engine/camera/multi_target_follow_strategy.h"
#include "demo_scene_payload.h"

namespace elysia::ui { class UiLabel; class UiWindow; }
namespace example::scene
{
class MultiTargetCameraScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_update(double delta) override;
    void on_render(SDL_Renderer* renderer) override;
    void on_input(const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;
protected:
    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const override;
private:
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
    elysia::core::Vector2 _movement{};
    float _separation_input = 0;
    std::size_t _primary = 0;
    bool _automatic = false;
    bool _dead_zone = true;
    bool _bounds = false;
    double _time = 0;
};
}
