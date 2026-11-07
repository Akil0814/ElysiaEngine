#pragma once

#include "camera_demo_state.h"
#include "engine/camera/camera_service.h"

namespace example::showcase::camera
{
// External camera choreography: no Scene or SceneCameraRuntime access.
class CameraCinematicController final
{
public:
    void start(elysia::core::Vector2 anchor, bool full, bool cut);
    void return_main();
    void update(double delta);
    void pause();
    void resume();
    // Safe after the camera service has already detached.
    void clear() noexcept;
    void sync(CameraDemoState& view) const;
    [[nodiscard]] bool controls_locked() const noexcept { return _phase != CinematicPhase::Idle; }

private:
    void cancel_playback() noexcept;
    CinematicPhase _phase = CinematicPhase::Idle;
    std::optional<elysia::camera::CameraMotionOperation> _motion;
    std::optional<elysia::camera::CameraBlendOperation> _blend;
    elysia::camera::CameraSlot _blend_target = elysia::camera::CameraSlot::Main;
    std::array<elysia::core::Vector2, 3> _nodes{};
    bool _full = false;
    bool _paused = false;
    double _hold_time = 0;
};
}
