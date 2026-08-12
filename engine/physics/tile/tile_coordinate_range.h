#pragma once

#include "../../core/geometry/rect.h"
#include "tile_collision_world.h"

#include <cstdint>
#include <optional>

namespace elysia::physics
{
enum class TileRangeBoundary : std::uint8_t
{
    HalfOpen,
    InclusiveTouching
};

struct TileCoordinateRange
{
    int min_x = 0;
    int min_y = 0;
    int max_x = -1;
    int max_y = -1;
    std::uint64_t candidate_count = 0;
};

[[nodiscard]] std::optional<TileCoordinate> checked_world_to_tile(
    elysia::core::Vector2 position,
    elysia::core::Vector2 origin,
    elysia::core::Vector2 tile_size) noexcept;

[[nodiscard]] std::optional<TileCoordinateRange> checked_tile_range(
    const elysia::core::Rect& bounds,
    elysia::core::Vector2 origin,
    elysia::core::Vector2 tile_size,
    TileRangeBoundary boundary,
    std::uint32_t maximum_candidates) noexcept;
}
