#pragma once

#include "collision/collision_contact.h"
#include "collision/world_shape.h"
#include "contracts/broad_phase_index.h"
#include "physics_object_handle.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace elysia::physics
{
enum class PhysicsDebugCapture : std::uint8_t
{
    None = 0,
    Shapes = 1u << 0,
    BroadPhase = 1u << 1,
    Contacts = 1u << 2,
    Velocities = 1u << 3,
    All = (1u << 4) - 1u
};

[[nodiscard]] constexpr PhysicsDebugCapture operator|(
    PhysicsDebugCapture first,
    PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(
        static_cast<std::uint8_t>(first) | static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr PhysicsDebugCapture operator&(
    PhysicsDebugCapture first,
    PhysicsDebugCapture second) noexcept
{
    return static_cast<PhysicsDebugCapture>(
        static_cast<std::uint8_t>(first) & static_cast<std::uint8_t>(second));
}

constexpr PhysicsDebugCapture& operator|=(
    PhysicsDebugCapture& first,
    PhysicsDebugCapture second) noexcept
{
    first = first | second;
    return first;
}

[[nodiscard]] constexpr bool captures_physics_debug(
    PhysicsDebugCapture capture,
    PhysicsDebugCapture requested) noexcept
{
    return (capture & requested) != PhysicsDebugCapture::None;
}

struct PhysicsStepStats
{
    std::size_t registered_objects = 0;
    std::size_t registered_colliders = 0;
    std::size_t broad_phase_proxies = 0;
    std::size_t broad_phase_pairs = 0;
    std::size_t narrow_phase_tests = 0;
    std::size_t contacts = 0;
    std::size_t tile_samples = 0;
    std::size_t rejected_tile_candidate_ranges = 0;
    std::size_t ccd_hits = 0;
    std::size_t ccd_iterations = 0;
    std::size_t solver_iterations = 0;
    std::uint64_t dropped_fixed_steps = 0;
};

struct PhysicsDebugShape
{
    CollisionTarget target{};
    WorldColliderShape previous{WorldAabb{}};
    WorldColliderShape current{WorldAabb{}};
    elysia::core::Rect swept_bounds{};
};

struct PhysicsDebugVelocity
{
    PhysicsObjectHandle object{};
    elysia::core::Vector2 origin{};
    elysia::core::Vector2 velocity{};
};

struct PhysicsDebugSnapshot
{
    std::vector<PhysicsDebugShape> shapes;
    std::vector<BroadPhasePair> broad_phase_pairs;
    std::vector<TileCoordinate> tile_candidates;
    std::vector<CollisionContact> contacts;
    std::vector<PhysicsDebugVelocity> velocities;

    void clear() noexcept
    {
        shapes.clear();
        broad_phase_pairs.clear();
        tile_candidates.clear();
        contacts.clear();
        velocities.clear();
    }
};
}
