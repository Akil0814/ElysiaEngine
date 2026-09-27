#pragma once

#include "../../camera/camera_manager.h"
#include "../../physics/physics_world_config.h"

#include <cstdint>
#include <optional>

namespace elysia::scene
{
struct FixedStepConfig
{
    double delta_seconds = 1.0 / 60.0;
    std::uint32_t max_steps_per_frame = 8;
};

enum class CameraUpdateMode
{
    Static,
    Dynamic
};

struct CameraSceneConfig
{
    elysia::camera::CameraSlot render_slot = elysia::camera::CameraSlot::Main;
    CameraUpdateMode update_mode = CameraUpdateMode::Static;
};

struct SceneRuntimeFeatures
{
    std::optional<FixedStepConfig> fixed_step;
    std::optional<elysia::physics::PhysicsWorldConfig> physics;
    CameraSceneConfig camera;
};
} // namespace elysia::scene
