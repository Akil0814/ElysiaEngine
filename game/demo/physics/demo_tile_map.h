#pragma once

#include "colored_block_object.h"

#include "../../../engine/physics/tile/tile_collision_world.h"

#include <vector>

namespace example::demo::physics
{
class DemoTileMap final
    : public ColoredBlockObject
    , public elysia::physics::ITileCollisionWorld
{
public:
    DemoTileMap(
        elysia::core::Vector2 origin,
        elysia::core::Vector2 tile_size,
        int columns,
        int rows,
        elysia::physics::TileOutOfBoundsPolicy out_of_bounds);

    [[nodiscard]] bool contains(elysia::physics::TileCoordinate coordinate) const noexcept;
    [[nodiscard]] bool set_cell(
        elysia::physics::TileCoordinate coordinate,
        elysia::physics::TileCollisionCell cell) noexcept;
    void fill_row(int y, int x_begin, int x_end,
        elysia::physics::TileCollisionCell cell) noexcept;
    void fill_column(int x, int y_begin, int y_end,
        elysia::physics::TileCollisionCell cell) noexcept;

    [[nodiscard]] elysia::core::Vector2 world_origin() const noexcept override { return _origin; }
    [[nodiscard]] elysia::core::Vector2 tile_size() const noexcept override { return _tile_size; }
    [[nodiscard]] int columns() const noexcept override { return _columns; }
    [[nodiscard]] int rows() const noexcept override { return _rows; }
    [[nodiscard]] elysia::physics::TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override { return _out_of_bounds; }
    [[nodiscard]] elysia::physics::TileCollisionCell cell_at(
        elysia::physics::TileCoordinate coordinate) const noexcept override;
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;

private:
    [[nodiscard]] std::size_t index(elysia::physics::TileCoordinate coordinate) const noexcept;

    elysia::core::Vector2 _origin{};
    elysia::core::Vector2 _tile_size{};
    int _columns = 0;
    int _rows = 0;
    elysia::physics::TileOutOfBoundsPolicy _out_of_bounds{};
    std::vector<elysia::physics::TileCollisionCell> _cells;
};

[[nodiscard]] elysia::physics::TileCollisionCell block_tile() noexcept;
[[nodiscard]] elysia::physics::TileCollisionCell one_way_tile() noexcept;
[[nodiscard]] elysia::physics::TileCollisionCell hazard_tile() noexcept;
}
