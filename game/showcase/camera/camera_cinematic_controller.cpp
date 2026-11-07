#include "camera_cinematic_controller.h"

#include <algorithm>

namespace example::showcase::camera
{
using namespace elysia::camera;
using elysia::core::Vector2;

void CameraCinematicController::cancel_playback() noexcept
{
    // The result may already be terminal, or belong to a detached scene.
    if (_motion) (void)ELYSIA_CAMERA->cancel_motion(*_motion);
    if (_blend) (void)ELYSIA_CAMERA->cancel_blend(*_blend);
    _motion.reset();
    _blend.reset();
}

void CameraCinematicController::clear() noexcept
{
    cancel_playback();
    _phase = CinematicPhase::Idle;
    _full = false;
    _paused = false;
    _hold_time = 0;
}

void CameraCinematicController::start(Vector2 anchor, bool full, bool cut)
{
    clear();
    _nodes = {anchor + Vector2{-400, -240}, anchor + Vector2{0, -160}, anchor};
    ELYSIA_CAMERA->set_center(CameraSlot::Cinematic, _nodes[0]).value();
    ELYSIA_CAMERA->set_zoom(CameraSlot::Cinematic, .75f).value();
    _full = full;
    _phase = cut ? CinematicPhase::Manual : CinematicPhase::Entering;
    if (cut) ELYSIA_CAMERA->cut_to(CameraSlot::Cinematic).value();
    else
    {
        if (full) ELYSIA_CAMERA->cut_to(CameraSlot::Main).value();
        _blend_target = CameraSlot::Cinematic;
        _blend = ELYSIA_CAMERA->blend_to(_blend_target, {.duration_seconds = .6}).value();
        // A no-op blend is already complete and can advance without a frame.
        if (_blend->reached_target()) update(0);
    }
}

void CameraCinematicController::return_main()
{
    if (_motion) (void)ELYSIA_CAMERA->cancel_motion(*_motion);
    _motion.reset();
    _paused = false;
    _full = false;
    ELYSIA_CAMERA->clear_effects(CameraSlot::Cinematic).value();
    _phase = CinematicPhase::Returning;
    _blend_target = CameraSlot::Main;
    _blend = ELYSIA_CAMERA->blend_to(_blend_target, {.duration_seconds = .6}).value();
    if (_blend->reached_target()) update(0);
}

void CameraCinematicController::pause()
{
    if (_motion) ELYSIA_CAMERA->pause_motion(*_motion).value();
    if (_blend) ELYSIA_CAMERA->pause_blend(*_blend).value();
    _paused = true;
}

void CameraCinematicController::resume()
{
    if (_motion) ELYSIA_CAMERA->resume_motion(*_motion).value();
    if (_blend) ELYSIA_CAMERA->resume_blend(*_blend).value();
    _paused = false;
}

void CameraCinematicController::update(double delta)
{
    const auto interrupted = [](const auto& operation) {
        if (!operation) return false;
        const auto status = operation->status();
        return status == CameraOperationStatus::Cancelled || status == CameraOperationStatus::Superseded
            || status == CameraOperationStatus::SceneEnded;
    };
    if (interrupted(_motion) || interrupted(_blend)) { clear(); return; }
    if (_paused) return;

    if (_blend && _blend->reached_target())
    {
        _blend.reset();
        if (_phase == CinematicPhase::Returning) _phase = CinematicPhase::Idle;
        else if (_phase == CinematicPhase::Entering)
        {
            if (!_full) _phase = CinematicPhase::Manual;
            else
            {
                _phase = CinematicPhase::Touring;
                _motion = ELYSIA_CAMERA->play_path(CameraSlot::Cinematic, {{
                    {{.center = _nodes[1], .zoom = 1.0f}, 1.5, CameraEasing::SmoothStep},
                    {{.center = _nodes[2], .zoom = 1.6f}, 1.2, CameraEasing::SmoothStep}},
                    CameraMotionEndBehavior::Hold}).value();
            }
        }
        return;
    }
    if (_phase == CinematicPhase::Touring && _motion && _motion->reached_target())
    {
        _phase = CinematicPhase::Holding;
        _hold_time = 0;
        ELYSIA_CAMERA->request_shake(CameraSlot::Cinematic, {}).value();
        return; // The completion frame's delta does not count toward the hold.
    }
    if (_phase == CinematicPhase::Holding)
    {
        _hold_time += std::max(0.0, delta);
        if (_hold_time >= 1) return_main();
    }
}

void CameraCinematicController::sync(CameraDemoState& view) const
{
    view.phase = _phase;
    view.motion = _motion;
    view.blend = _blend;
    view.blend_target = _blend_target;
    view.paused = _paused;
    std::copy(_nodes.begin(), _nodes.end(), view.nodes.begin());
    view.node_count = controls_locked() ? _nodes.size() : 0;
}
}
