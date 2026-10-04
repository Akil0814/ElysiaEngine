#pragma once
#include "game/showcase/camera/camera_overlay_view.h"
#include "game/showcase/camera/camera_controls_view.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/camera/multi_target_follow_strategy.h"
#include "game/showcase/shared/showcase_enter_payload.h"
namespace example::scene {
class CameraShowcaseScene final : public elysia::gameplay::GameplayScene {
public:
    CameraShowcaseScene();
    const example::showcase::camera::CameraDemoState& state() const noexcept { return _state; }
    void select_page(example::showcase::camera::CameraPage);
    void perform(example::showcase::camera::CameraAction);
protected:
    void on_enter(const elysia::scene::ScenePayload&) override;
    void on_exit() override;
    void on_reset() override;
    void on_after_update(double) override;
    void on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>&) override;
    std::optional<elysia::camera::CameraFocus> resolve_camera_focus(elysia::camera::CameraSlot) const override;
    void on_control_target_removing(elysia::core::SceneObject&) override;
    void on_game_fixed_update(std::uint64_t,double) override;
    void on_camera_motion_completed(elysia::camera::CameraMotionId,elysia::camera::CameraSlot) override;
    void on_camera_blend_completed(elysia::camera::CameraBlendId,elysia::camera::CameraSlot) override;
private:
    void reset_page();
    void cleanup_activity();
    void freeze(bool);
    void install_strategy();
    void request_primary(std::size_t);
    void finish_primary_request();
    void build_controls();
    void refresh_status();
    void return_to_caller();
    void enter_cinematic(bool full,bool cut);
    void return_main();
    example::showcase::camera::CameraDemoState _state;
    example::showcase::camera::CameraControlsView _view;
    example::showcase::camera::CameraOverlayView* _overlay = nullptr;
    elysia::ui::UiWindow* _controls = nullptr;
    elysia::scene::SceneRoute _return_route;
    std::array<elysia::core::GameObject*,2> _targets{};
    elysia::camera::MultiTargetFollowStrategy* _strategy = nullptr;
    elysia::gameplay::ControllerHandle _controller;
    std::optional<elysia::gameplay::ControllerOperation> _primary_request;
    std::size_t _requested_primary = 0;
    bool _hold_started = false;
};
}
