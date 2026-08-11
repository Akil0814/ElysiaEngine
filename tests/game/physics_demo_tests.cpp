#define SDL_MAIN_HANDLED

#include "game/physics_demo/block_actor.h"
#include "game/physics_demo/demo_tile_map.h"
#include "game/application/example_game_module.h"
#include "game/scene/example_scene_keys.h"
#include "game/scene/demo/demo_scene_payload.h"
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

struct DemoReturnPayload
{
    int marker = 0;
};

class DemoReturnScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const auto* value =
            elysia::scene::try_scene_payload<DemoReturnPayload>(payload);
        marker = value ? value->marker : 0;
    }
    void on_exit() override {}
    void reset() override {}

    static inline int marker = 0;
};

void press_and_release_key(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control)
{
    for (const auto type : {
            elysia::input::RawInputEventType::ControlPressed,
            elysia::input::RawInputEventType::ControlReleased})
    {
        scene_manager.on_input(
            elysia::input::RawInputFrame{},
            {elysia::input::RawInputEvent{
                .control = control,
                .type = type,
                .device = elysia::input::InputDevice::Keyboard}});
    }
}

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
        ExampleSceneKeys::AnimationPreview,
        ExampleSceneKeys::PhysicsDemoMenu,
        ExampleSceneKeys::PhysicsCollisionTest,
        ExampleSceneKeys::PlatformTilePhysicsTest,
        ExampleSceneKeys::TopDownTilePhysicsTest,
        ExampleSceneKeys::DemoGallery,
        ExampleSceneKeys::UiTest,
        ExampleSceneKeys::EngineFeatureTest};
    for (std::size_t i = 0; i < keys.size(); ++i)
        for (std::size_t j = i + 1; j < keys.size(); ++j)
            require(keys[i] != keys[j], "Game scene keys must be unique");
}

void test_game_module_registers_demo_scenes()
{
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);

    for (const elysia::scene::SceneKey key : {
            ExampleSceneKeys::DemoGallery,
            ExampleSceneKeys::AnimationPreview,
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
            "GameModule must register every project-owned demo scene");
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
        .payload = example::scene::DemoScenePayload{
            .return_route = {
                .target = ExampleSceneKeys::MainMenu,
                .reload_mode = elysia::scene::SceneReloadMode::Reuse}},
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

void test_physics_demo_navigation_and_recreate_route()
{
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);
    scene_manager.register_game_scene<DemoReturnScene>(1);
    scene_manager.set_runtime_context(context);

    const elysia::scene::SceneRoute caller{
        .target = 1,
        .payload = DemoReturnPayload{.marker = 73},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse};
    scene_manager.start({
        .target = ExampleSceneKeys::PhysicsDemoMenu,
        .payload = example::scene::DemoScenePayload{
            .return_route = caller},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse});

    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key()
            == ExampleSceneKeys::PhysicsCollisionTest,
        "The first Physics menu entry must open Collider Combat");

    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyR);
    require(scene_manager.current_scene_key()
            == ExampleSceneKeys::PhysicsCollisionTest,
        "Recreating a Physics demo must keep the active scene route");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key()
            == ExampleSceneKeys::PhysicsDemoMenu,
        "A recreated Physics demo must retain its Physics menu return route");

    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == 1
            && DemoReturnScene::marker == 73,
        "Physics menu Escape must preserve the complete Gallery caller route");
    scene_manager.shutdown();
}

void test_animation_preview_returns_complete_caller_route()
{
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);
    scene_manager.register_game_scene<DemoReturnScene>(1);
    scene_manager.set_runtime_context(context);
    scene_manager.start({
        .target = ExampleSceneKeys::AnimationPreview,
        .payload = example::scene::DemoScenePayload{
            .return_route = {
                .target = 1,
                .payload = DemoReturnPayload{.marker = 91},
                .reload_mode = elysia::scene::SceneReloadMode::Reset}},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse});

    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == 1
            && DemoReturnScene::marker == 91,
        "Animation Preview Escape must preserve the complete Gallery caller route");
    scene_manager.shutdown();
}

void test_main_menu_uses_gallery_as_its_primary_demo_entry()
{
    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    elysia::scene::SceneManager scene_manager;
    example::application::GameModule{}.register_scenes(scene_manager);
    scene_manager.set_runtime_context(context);
    scene_manager.start({
        .target = ExampleSceneKeys::MainMenu,
        .reload_mode = elysia::scene::SceneReloadMode::Reuse});

    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEnter);
    require(scene_manager.current_scene_key() == ExampleSceneKeys::DemoGallery,
        "The first Main Menu action must open the unified Demo Gallery");
    press_and_release_key(
        scene_manager, elysia::input::RawInputControl::KeyEscape);
    require(scene_manager.current_scene_key() == ExampleSceneKeys::MainMenu,
        "Demo Gallery must return to the Main Menu caller route");
    scene_manager.shutdown();
}
}

int main()
{
    test_health_contract();
    test_tile_adapter_contract();
    test_actor_provider_and_damage_flow();
    test_scene_keys_are_unique();
    test_game_module_registers_demo_scenes();
    test_physics_demo_layout_contract();
    test_physics_demo_fixed_cameras();
    test_physics_demo_navigation_and_recreate_route();
    test_animation_preview_returns_complete_caller_route();
    test_main_menu_uses_gallery_as_its_primary_demo_entry();
    std::cout << "physics demo tests passed\n";
    return 0;
}
