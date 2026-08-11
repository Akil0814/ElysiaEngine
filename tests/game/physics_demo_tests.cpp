#define SDL_MAIN_HANDLED

#include "game/physics_demo/block_actor.h"
#include "game/physics_demo/demo_tile_map.h"
#include "game/application/example_game_module.h"
#include "game/scene/example_scene_keys.h"
#include "game/scene/physics_demo/physics_demo_layout.h"
#include "engine/camera/camera_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

using elysia::tests::require;

namespace
{
class RegistrationProbeScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void reset() override {}
};

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
        ExampleSceneKeys::TopDownTilePhysicsTest,
        ExampleSceneKeys::TestbedHome,
        ExampleSceneKeys::UiTest,
        ExampleSceneKeys::EngineFeatureTest};
    for (std::size_t i = 0; i < keys.size(); ++i)
        for (std::size_t j = i + 1; j < keys.size(); ++j)
            require(keys[i] != keys[j], "Game scene keys must be unique");
}

void test_game_module_registers_testbed_scenes()
{
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);

    for (const elysia::scene::SceneKey key : {
            ExampleSceneKeys::TestbedHome,
            ExampleSceneKeys::UiTest,
            ExampleSceneKeys::EngineFeatureTest })
    {
        bool duplicate_rejected = false;
        try
        {
            scene_manager.register_game_scene<RegistrationProbeScene>(key);
        }
        catch (const std::logic_error& error)
        {
            duplicate_rejected =
                std::string_view(error.what()).find("duplicate SceneKey")
                != std::string_view::npos;
        }
        require(duplicate_rejected,
            "GameModule must register every project-owned Testbed scene");
    }
}

void test_physics_demo_layout_contract()
{
    for (const auto logical_size : {
            elysia::core::Vector2{1280.0f, 720.0f},
            elysia::core::Vector2{960.0f, 540.0f}})
    {
        const example::scene::PhysicsDemoLayout layout =
            example::scene::make_physics_demo_layout(
                logical_size.x, logical_size.y);
        require(layout.viewport.contains(layout.hud_panel)
                && layout.viewport.contains(layout.title)
                && layout.viewport.contains(layout.controls)
                && layout.viewport.contains(layout.health)
                && layout.viewport.contains(layout.stats)
                && layout.viewport.contains(layout.status)
                && layout.viewport.contains(layout.menu_list),
            "Physics demo screen layout must stay inside the logical viewport");
        require(!layout.title.intersects(layout.controls)
                && !layout.title.intersects(layout.health)
                && !layout.controls.intersects(layout.health)
                && !layout.controls.intersects(layout.stats),
            "Physics demo HUD regions must not overlap");
        require(layout.status.center().nearly_equals(layout.viewport.center())
                && layout.menu_list.center().nearly_equals(layout.viewport.center()),
            "Physics demo status and menu must remain centered");

        const auto options =
            example::scene::physics_demo_layout_options(layout.stats);
        require(options._anchor == elysia::ui::UiLayoutAnchor::TopLeft
                && options._use_size_override
                && options._margin.left == layout.stats.x()
                && options._margin.top == layout.stats.y()
                && options._size_override == layout.stats.size(),
            "Physics demo elements must use explicit window layout metadata");
    }
}

void require_demo_camera(
    elysia::scene::SceneKey scene_key,
    const elysia::core::Vector2& expected_center)
{
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);
    scene_manager.set_runtime_context(context);
    scene_manager.start({
        .target = scene_key,
        .reload_mode = elysia::scene::SceneReloadMode::Recreate});
    scene_manager.on_update(0.0);

    const auto& camera = elysia::camera::CameraManager::instance()->camera(
        elysia::camera::CameraSlot::Main);
    require(camera.center() == expected_center,
        "Each physics demo must restore its fixed room camera on entry");
    require(camera.zoom() == 1.0f,
        "Physics demo fixed rooms must use unit zoom");
    require(camera.world_to_screen(expected_center)
            == elysia::core::Vector2(640.0f, 360.0f),
        "The physics demo room center must project to the logical screen center");
    scene_manager.shutdown();
}

void test_physics_demo_fixed_cameras()
{
    require_demo_camera(
        ExampleSceneKeys::PhysicsCollisionTest, {640.0f, 360.0f});
    require_demo_camera(
        ExampleSceneKeys::PlatformTilePhysicsTest, {320.0f, 360.0f});
    require_demo_camera(
        ExampleSceneKeys::TopDownTilePhysicsTest, {576.0f, 344.0f});
}
}

int main()
{
    test_health_contract();
    test_tile_adapter_contract();
    test_actor_provider_and_damage_flow();
    test_scene_keys_are_unique();
    test_game_module_registers_testbed_scenes();
    test_physics_demo_layout_contract();
    test_physics_demo_fixed_cameras();
    std::cout << "physics demo tests passed\n";
    return 0;
}
