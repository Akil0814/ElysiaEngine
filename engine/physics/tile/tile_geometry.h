#pragma once

#include "tile_collision_world.h"
#include "../contracts/collision_strategy.h"

namespace elysia::physics::detail
{
[[nodiscard]] inline TileCollisionCell tile_cell(
    const ITileCollisionWorld& world, TileCoordinate coordinate) noexcept
{
    if (coordinate.x < 0 || coordinate.y < 0
        || coordinate.x >= world.columns() || coordinate.y >= world.rows())
        return {world.out_of_bounds_policy() == TileOutOfBoundsPolicy::Block
            ? TileCollisionType::Block : TileCollisionType::Empty};
    return world.cell_at(coordinate);
}

[[nodiscard]] inline elysia::core::Rect tile_rect(
    const ITileCollisionWorld& world, TileCoordinate coordinate) noexcept
{
    const auto origin = world.world_origin();
    const auto size = world.tile_size();
    return {origin.x + static_cast<float>(coordinate.x) * size.x,
        origin.y + static_cast<float>(coordinate.y) * size.y, size.x, size.y};
}

[[nodiscard]] inline CollisionResponse tile_response(const TileCollisionCell& cell) noexcept
{
    if (cell.type == TileCollisionType::Empty) return CollisionResponse::Ignore;
    return cell.type == TileCollisionType::Overlap ? CollisionResponse::Overlap : CollisionResponse::Block;
}

[[nodiscard]] inline CollisionShapeView tile_view(const ITileCollisionWorld& world,
    TileCoordinate coordinate, const TileCollisionCell& cell) noexcept
{
    const auto rect = tile_rect(world, coordinate);
    CollisionShapeView view;
    view.target = CollisionTarget::from_tile(coordinate);
    view.previous_shape = WorldAabb{rect};
    view.current_shape = view.previous_shape;
    view.current_bounds = rect;
    view.swept_bounds = rect;
    view.filter = cell.filter;
    view.response = tile_response(cell);
    if (cell.type == TileCollisionType::OneWay) view.one_way = cell.one_way;
    view.material = cell.material;
    return view;
}
}
