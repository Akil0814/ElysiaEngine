#pragma once

#include "physics_body.h"
#include "../physics_object_handle.h"

#include <span>

namespace elysia::physics
{
struct PhysicsWorldConfig;

struct PhysicsBodyView
{
    PhysicsObjectHandle object{};
    PhysicsBody* body = nullptr;
    elysia::core::Vector2 previous_owner_origin{};
    elysia::core::Vector2 current_owner_origin{};
};

class PhysicsSystem
{
public:
    PhysicsSystem() = default;

    void integrate(
        std::span<PhysicsBodyView> body_views,
        const PhysicsWorldConfig& config,
        double fixed_delta_seconds) const noexcept;

    void clear_forces(std::span<PhysicsBodyView> body_views) const noexcept;
};
}
