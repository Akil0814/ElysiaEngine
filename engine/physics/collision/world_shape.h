#pragma once

#include "collider.h"
#include "collision_contact.h"

#include <optional>
#include <variant>

namespace elysia::physics
{
struct WorldAabb
{
    elysia::core::Rect rect{};
};

struct WorldCircle
{
    elysia::core::Vector2 center{};
    float radius = 0.0f;
};

using WorldColliderShape = std::variant<WorldAabb, WorldCircle>;

[[nodiscard]] bool finite_vector(const elysia::core::Vector2& value) noexcept;
[[nodiscard]] bool valid_shape(const ColliderShape& shape) noexcept;
[[nodiscard]] std::optional<WorldColliderShape> make_world_shape(
    const ColliderShape& shape,
    const elysia::core::Vector2& owner_origin) noexcept;
[[nodiscard]] elysia::core::Rect shape_bounds(
    const WorldColliderShape& shape) noexcept;
[[nodiscard]] elysia::core::Rect swept_shape_bounds(
    const WorldColliderShape& previous,
    const WorldColliderShape& current) noexcept;
[[nodiscard]] WorldColliderShape translated_shape(
    const WorldColliderShape& shape,
    const elysia::core::Vector2& offset) noexcept;
[[nodiscard]] bool collision_filters_allow(
    const CollisionFilter& first,
    const CollisionFilter& second) noexcept;
[[nodiscard]] CollisionResponse combine_collision_responses(
    CollisionResponse first,
    CollisionResponse second) noexcept;
[[nodiscard]] std::optional<CollisionHit> detect_discrete_shapes(
    const WorldColliderShape& first,
    const WorldColliderShape& second) noexcept;
[[nodiscard]] std::optional<CollisionHit> detect_swept_aabbs(
    const WorldAabb& first_previous,
    const WorldAabb& first_current,
    const WorldAabb& second_previous,
    const WorldAabb& second_current,
    float epsilon) noexcept;
}
