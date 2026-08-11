#define SDL_MAIN_HANDLED

#include "game/physics_demo/block_actor.h"
#include "game/physics_demo/demo_tile_map.h"
#include "game/scene/example_scene_keys.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <iostream>

using elysia::tests::require;

namespace
{
void test_health_contract()
{
    example::physics_demo::Health health(50);
    require(health.maximum() == 50 && health.current() == 50 && health.alive(),
        "Health must start at its configured maximum");
    require(health.apply_damage(0) == 0 && health.apply_damage(-3) == 0,
        "Non-positive damage must be ignored");
    require(health.apply_damage(20) == 20 && health.current() == 30,
        "Damage must subtract the applied amount");
    require(health.apply_damage(100) == 30 && !health.alive(),
        "Lethal damage must clamp at zero");
    require(health.apply_damage(1) == 0,
        "A dead health value must not be damaged repeatedly");
    health.reset();
    require(health.current() == 50 && health.alive(),
        "Health reset must restore the maximum");
}

void test_tile_adapter_contract()
{
    using namespace example::physics_demo;
    SolidColorTexture texture;
    DemoTileMap map(texture, {-64, 24}, {16, 32}, 4, 3,
        elysia::physics::TileOutOfBoundsPolicy::Block);
    require(map.world_origin() == elysia::core::Vector2(-64, 24)
            && map.tile_size() == elysia::core::Vector2(16, 32),
        "Tile adapter must preserve non-zero origin and non-square size");
    require(map.set_cell({1, 1}, one_way_tile())
            && map.cell_at({1, 1}).type == elysia::physics::TileCollisionType::OneWay,
        "Tile adapter must store collision cells by coordinate");
    require(!map.set_cell({-1, 0}, block_tile()),
        "Tile adapter must reject coordinates outside its storage");
    require(map.cell_at({-1, 0}).type == elysia::physics::TileCollisionType::Block,
        "Block out-of-bounds policy must synthesize a blocking cell");
    map.fill_row(2, -10, 10, hazard_tile());
    for (int x = 0; x < 4; ++x)
        require(map.cell_at({x, 2}).tag == "hazard",
            "Row filling must clip and retain stable hazard tags");
}

void test_actor_provider_and_damage_flow()
{
    using namespace example::physics_demo;
    SolidColorTexture texture;
    PlatformPlayerCharacter player(texture, {0, 0, 34, 56});
    StationaryEnemy enemy(texture, {40, 0, 38, 52}, player);
    elysia::physics::PhysicsWorldConfig config;
    config.gravity = {};
    elysia::physics::PhysicsWorld world(config);

    auto player_span = player.colliders();
    auto enemy_span = enemy.colliders();
    require(player_span.size() == 3 && enemy_span.size() == 3,
        "Combat actors must expose Body, HurtBox and HitBox storage");
    require(player_span.data() == player.colliders().data(),
        "Collider provider storage must stay address-stable");
    require(world.register_object(player, &player, &player).is_valid()
            && world.register_object(enemy, &enemy, &enemy).is_valid(),
        "Combat actors must register with body and collider providers");

    elysia::gameplay::collision::GameplayCollisionRuntime runtime(world);
    DemoCombatSession combat(world, runtime);
    require(player.bind_combat(combat) && enemy.bind_combat(combat),
        "Registered actors must bind atomically to gameplay collision");
    require(player_span[0].filter.category == collision_layers::Body
            && player_span[1].filter.category == collision_layers::HurtBox
            && player_span[2].filter.category == collision_layers::HitBox
            && !player_span[2].enabled,
        "Actor collider roles and initial HitBox state must match the contract");

    player.start_attack();
    player.update(0.10);
    require(player_span[2].enabled,
        "Player HitBox must enable during the active attack window");
    (void)world.advance(1.0 / 60.0);
    require(enemy.health().current() == 25,
        "A hostile player attack must apply its configured damage");
    (void)world.advance(1.0 / 60.0);
    require(enemy.health().current() == 25,
        "Stay contacts from one attack instance must not repeat damage");

    player.update(0.30);
    (void)world.advance(1.0 / 60.0);
    player.update(0.40);
    player.start_attack();
    player.update(0.10);
    (void)world.advance(1.0 / 60.0);
    require(!enemy.alive() && enemy.health().current() == 0,
        "A new attack instance must be able to deal damage and kill");
    combat.flush_deaths();
    require(!enemy_span[0].enabled && !enemy_span[1].enabled,
        "Death processing must leave all physical participation disabled");
}

void test_scene_keys_are_unique()
{
    constexpr std::array keys{
        ExampleSceneKeys::MainMenu,
        ExampleSceneKeys::Sandbox,
        ExampleSceneKeys::PhysicsDemoMenu,
        ExampleSceneKeys::PhysicsCollisionTest,
        ExampleSceneKeys::PlatformTilePhysicsTest,
        ExampleSceneKeys::TopDownTilePhysicsTest};
    for (std::size_t i = 0; i < keys.size(); ++i)
        for (std::size_t j = i + 1; j < keys.size(); ++j)
            require(keys[i] != keys[j], "Game scene keys must be unique");
}
}

int main()
{
    test_health_contract();
    test_tile_adapter_contract();
    test_actor_provider_and_damage_flow();
    test_scene_keys_are_unique();
    std::cout << "physics demo tests passed\n";
    return 0;
}
