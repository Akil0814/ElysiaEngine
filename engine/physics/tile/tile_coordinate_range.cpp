#include "tile_coordinate_range.h"

#include "../collision/world_shape.h"

#include <cmath>
#include <limits>

namespace elysia::physics
{
namespace
{
[[nodiscard]] std::optional<int> checked_floor_to_int(double value) noexcept
{
    if (!std::isfinite(value))
        return std::nullopt;
    const double floored = std::floor(value);
    if (floored < static_cast<double>(std::numeric_limits<int>::min())
        || floored > static_cast<double>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }
    return static_cast<int>(floored);
}

[[nodiscard]] std::optional<int> coordinate_index(
    float position,
    float origin,
    float size) noexcept
{
    if (!std::isfinite(position) || !std::isfinite(origin)
        || !std::isfinite(size) || size <= 0.0f)
    {
        return std::nullopt;
    }
    return checked_floor_to_int(
        (static_cast<double>(position) - static_cast<double>(origin))
        / static_cast<double>(size));
}
}

std::optional<TileCoordinate> checked_world_to_tile(
    elysia::core::Vector2 position,
    elysia::core::Vector2 origin,
    elysia::core::Vector2 tile_size) noexcept
{
    const auto x = coordinate_index(position.x, origin.x, tile_size.x);
    const auto y = coordinate_index(position.y, origin.y, tile_size.y);
    if (!x || !y)
        return std::nullopt;
    return TileCoordinate{*x, *y};
}

std::optional<TileCoordinateRange> checked_tile_range(
    const elysia::core::Rect& bounds,
    elysia::core::Vector2 origin,
    elysia::core::Vector2 tile_size,
    TileRangeBoundary boundary,
    std::uint32_t maximum_candidates) noexcept
{
    if (!finite_vector(bounds.position()) || !finite_vector(bounds.size())
        || bounds.is_empty() || !finite_vector(origin) || !finite_vector(tile_size)
        || tile_size.x <= 0.0f || tile_size.y <= 0.0f
        || maximum_candidates == 0)
    {
        return std::nullopt;
    }

    float minimum_x = bounds.left();
    float minimum_y = bounds.top();
    float maximum_x = bounds.right();
    float maximum_y = bounds.bottom();
    if (boundary == TileRangeBoundary::InclusiveTouching)
    {
        minimum_x = std::nextafter(minimum_x, -std::numeric_limits<float>::infinity());
        minimum_y = std::nextafter(minimum_y, -std::numeric_limits<float>::infinity());
        maximum_x = std::nextafter(maximum_x, std::numeric_limits<float>::infinity());
        maximum_y = std::nextafter(maximum_y, std::numeric_limits<float>::infinity());
    }
    else
    {
        maximum_x = std::nextafter(maximum_x, -std::numeric_limits<float>::infinity());
        maximum_y = std::nextafter(maximum_y, -std::numeric_limits<float>::infinity());
    }

    const auto min_x = coordinate_index(minimum_x, origin.x, tile_size.x);
    const auto min_y = coordinate_index(minimum_y, origin.y, tile_size.y);
    const auto max_x = coordinate_index(maximum_x, origin.x, tile_size.x);
    const auto max_y = coordinate_index(maximum_y, origin.y, tile_size.y);
    if (!min_x || !min_y || !max_x || !max_y || *min_x > *max_x || *min_y > *max_y)
        return std::nullopt;

    const auto width = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(*max_x) - static_cast<std::int64_t>(*min_x) + 1);
    const auto height = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(*max_y) - static_cast<std::int64_t>(*min_y) + 1);
    if (height != 0 && width > std::numeric_limits<std::uint64_t>::max() / height)
        return std::nullopt;
    const std::uint64_t count = width * height;
    if (count > maximum_candidates)
        return std::nullopt;
    return TileCoordinateRange{*min_x, *min_y, *max_x, *max_y, count};
}
}
