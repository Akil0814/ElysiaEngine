#include "demo_tile_map.h"

#include "demo_collision_layers.h"
#include "../../engine/core/render/colors.h"
#include "../../engine/core/render/render_command.h"

#include <algorithm>
#include <stdexcept>

namespace example::physics_demo
{
DemoTileMap::DemoTileMap(
    SolidColorTexture& texture,
    elysia::core::Vector2 origin,
    elysia::core::Vector2 tile_size,
    int columns,
    int rows,
    elysia::physics::TileOutOfBoundsPolicy out_of_bounds)
    : ColoredBlockObject(texture, elysia::core::DepthLayer::Terrain,
          {origin, {tile_size.x * columns, tile_size.y * rows}},
          elysia::core::colors::gray_700),
      _origin(origin), _tile_size(tile_size), _columns(columns), _rows(rows),
      _out_of_bounds(out_of_bounds)
{
    if (columns <= 0 || rows <= 0 || tile_size.x <= 0.0f || tile_size.y <= 0.0f)
        throw std::invalid_argument("DemoTileMap requires positive dimensions.");
    _cells.resize(static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows));
}

bool DemoTileMap::contains(elysia::physics::TileCoordinate c) const noexcept
{
    return c.x >= 0 && c.y >= 0 && c.x < _columns && c.y < _rows;
}

std::size_t DemoTileMap::index(elysia::physics::TileCoordinate c) const noexcept
{
    return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(_columns)
        + static_cast<std::size_t>(c.x);
}

bool DemoTileMap::set_cell(
    elysia::physics::TileCoordinate c,
    elysia::physics::TileCollisionCell cell) noexcept
{
    if (!contains(c))
        return false;
    _cells[index(c)] = cell;
    return true;
}

void DemoTileMap::fill_row(int y, int x_begin, int x_end,
    elysia::physics::TileCollisionCell cell) noexcept
{
    for (int x = std::max(0, x_begin); x <= std::min(_columns - 1, x_end); ++x)
        (void)set_cell({x, y}, cell);
}

void DemoTileMap::fill_column(int x, int y_begin, int y_end,
    elysia::physics::TileCollisionCell cell) noexcept
{
    for (int y = std::max(0, y_begin); y <= std::min(_rows - 1, y_end); ++y)
        (void)set_cell({x, y}, cell);
}

elysia::physics::TileCollisionCell DemoTileMap::cell_at(
    elysia::physics::TileCoordinate c) const noexcept
{
    if (contains(c))
        return _cells[index(c)];
    if (_out_of_bounds == elysia::physics::TileOutOfBoundsPolicy::Block)
        return block_tile();
    return {};
}

void DemoTileMap::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    SDL_Texture* texture = color_texture().get();
    if (!texture)
        return;
    for (int y = 0; y < _rows; ++y)
    {
        for (int x = 0; x < _columns; ++x)
        {
            const auto cell = _cells[index({x, y})];
            elysia::core::Color color;
            switch (cell.type)
            {
            case elysia::physics::TileCollisionType::Block:
                color = elysia::core::colors::gray_700; break;
            case elysia::physics::TileCollisionType::OneWay:
                color = elysia::core::colors::yellow_700; break;
            case elysia::physics::TileCollisionType::Overlap:
                color = elysia::core::Color{0, 188, 212, 120}; break;
            case elysia::physics::TileCollisionType::Empty:
            default:
                continue;
            }
            elysia::core::RenderCommand command;
            command.texture = texture;
            command.command_rect = {
                _origin.x + x * _tile_size.x,
                _origin.y + y * _tile_size.y,
                _tile_size.x, _tile_size.y};
            command.alpha = color.a;
            command.texture_color_modulation = elysia::core::TextureColorModulation{
                color.r, color.g, color.b};
            out_commands.push_back(command);
        }
    }
}

elysia::physics::TileCollisionCell block_tile() noexcept
{
    return {elysia::physics::TileCollisionType::Block,
        {collision_layers::World, collision_layers::Body, 0}, std::nullopt, "block"};
}

elysia::physics::TileCollisionCell one_way_tile() noexcept
{
    return {elysia::physics::TileCollisionType::OneWay,
        {collision_layers::World, collision_layers::Body, 0},
        elysia::physics::OneWayCollision{elysia::physics::PassThroughDirection::Up, 0.02f},
        "one_way"};
}

elysia::physics::TileCollisionCell hazard_tile() noexcept
{
    return {elysia::physics::TileCollisionType::Overlap,
        {collision_layers::World, collision_layers::Body, 0}, std::nullopt, "hazard"};
}
}
