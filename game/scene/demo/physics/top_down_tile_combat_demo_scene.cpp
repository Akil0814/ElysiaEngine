#include "top_down_tile_combat_demo_scene.h"

#include "physics_combat_demo_helpers.h"
#include "../../example_scene_keys.h"

namespace example::scene
{
TopDownTileCombatDemoScene::TopDownTileCombatDemoScene()
    : PhysicsCombatDemoSceneBase(
          example::scene_keys::TopDownTileCombatDemo,
          "TopDownTileCombatDemoScene",
          detail::make_gravity_config(0.0f),
          "Top-Down Tile Combat Demo",
          "WASD Move | J Attack | Enemies require line of sight")
{
}

void TopDownTileCombatDemoScene::build_demo()
{
    using namespace example::demo::physics;
    set_demo_camera_center({576.0f, 344.0f});
    auto* map = create_and_add_object<DemoTileMap>(
        elysia::core::Vector2{-64, 80},
        elysia::core::Vector2{32, 24},
        40,
        22,
        elysia::physics::TileOutOfBoundsPolicy::Block);
    if (!map)
        return;

    map->fill_row(0, 0, 39, block_tile());
    map->fill_row(21, 0, 39, block_tile());
    map->fill_column(0, 0, 21, block_tile());
    map->fill_column(39, 0, 21, block_tile());
    map->fill_column(12, 3, 16, block_tile());
    map->fill_column(26, 5, 19, block_tile());
    map->fill_row(10, 12, 20, block_tile());
    map->fill_row(6, 27, 35, block_tile());
    map->fill_row(17, 5, 11, block_tile());
    map->fill_row(14, 16, 19, hazard_tile());
    map->fill_row(15, 16, 19, hazard_tile());
    bind_tile_map(*map);

    auto* player = add_actor<TopDownPlayerCharacter>(
        elysia::core::Rect{40, 130, 34, 40});
    if (!player)
        return;
    set_player(*player);
    (void)add_actor<TopDownChaseEnemy>(
        elysia::core::Rect{300, 150, 36, 42}, *player);
    (void)add_actor<TopDownChaseEnemy>(
        elysia::core::Rect{700, 250, 36, 42}, *player);
    (void)add_actor<TopDownChaseEnemy>(
        elysia::core::Rect{920, 500, 36, 42}, *player);


}
}
