#pragma once
#include "engine/camera/camera_motion.h"
#include <array>
#include <optional>
namespace example::showcase::camera {
enum class CameraPage { Follow, Motion, Cinematic };
enum class FollowMode { Hard, Smooth, DeadZone, MultiTarget };
enum class CinematicPhase { Idle, Entering, Touring, Holding, Returning, Manual };
enum class CameraAction { Strategy, DeadZone, Swap, Automatic, Teleport, Zoom, Bounds, Edge,
    Move, Path, Easing, EndBehavior, Pause, Resume, Cancel, Follow, Shake, ClearShake,
    Cut, Blend, Return, Play, Skip, Replay, Reset, Count };
struct CameraDemoState {
    CameraPage page = CameraPage::Follow;
    FollowMode strategy = FollowMode::MultiTarget;
    CinematicPhase phase = CinematicPhase::Idle;
    elysia::camera::CameraEasing easing = elysia::camera::CameraEasing::SmoothStep;
    elysia::camera::CameraMotionEndBehavior end = elysia::camera::CameraMotionEndBehavior::ResumeFollow;
    std::optional<elysia::camera::CameraMotionId> motion;
    std::optional<elysia::camera::CameraBlendId> blend;
    elysia::camera::CameraSlot blend_target = elysia::camera::CameraSlot::Main;
    bool automatic = false, dead_zone = true, bounds = false, frozen = false, paused = false;
    bool saved_automatic = false, full_show = false;
    std::size_t primary = 0;
    double time = 0, hold_time = 0;
    elysia::core::Vector2 anchor{};
    std::array<elysia::core::Vector2,4> nodes{};
    std::size_t node_count = 0;
    const char* feedback = "showcase.camera.ready";
};
} // namespace example::showcase::camera
