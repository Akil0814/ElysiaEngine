#include "platform_tile_combat_demo_scene.h"

#include "physics_combat_demo_helpers.h"
#include "../../example_scene_keys.h"

namespace example::scene
{
PlatformTileCombatDemoScene::PlatformTileCombatDemoScene()
    : PhysicsCombatDemoSceneBase(
          example::scene_keys::PlatformTileCombatDemo,
          "PlatformTileCombatDemoScene",
          detail::make_gravity_config(1400.0f),
          "Platform Tile Combat Demo",
          "A/D Move | Space Jump | Down+Space Drop | J Attack")
{
}

void PlatformTileCombatDemoScene::build_demo()
{
    using namespace example::demo::physics;
    set_demo_camera_center({320.0f, 360.0f});
    auto* map = create_and_add_object<DemoTileMap>(
        elysia::core::Vector2{-320, 40},
        elysia::core::Vector2{32, 32},
        40,
        20,
        elysia::physics::TileOutOfBoundsPolicy::Block);
    if (!map)
        return;

    map->fill_row(19, 0, 39, block_tile());
    map->fill_row(18, 0, 39, block_tile());
    map->fill_column(0, 0, 19, block_tile());
    map->fill_column(39, 0, 19, block_tile());
    map->fill_row(14, 8, 15, one_way_tile());
    map->fill_row(11, 20, 28, one_way_tile());
    map->fill_row(15, 31, 36, one_way_tile());
    map->fill_column(18, 16, 18, block_tile());
    map->fill_column(19, 17, 18, block_tile());
    bind_tile_map(*map);

    auto* player = add_actor<PlatformPlayerCharacter>(
        elysia::core::Rect{-180, 520, 34, 56});
    if (!player)
        return;
    set_player(*player);
    (void)add_actor<PlatformPatrolEnemy>(
        elysia::core::Rect{180, 552, 38, 52},
        *player,
        80.0f,
        360.0f);
    (void)add_actor<PlatformPatrolEnemy>(
        elysia::core::Rect{650, 520, 38, 52},
        *player,
        560.0f,
        820.0f);
}
}
