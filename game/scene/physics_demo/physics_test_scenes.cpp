#include "physics_test_scenes.h"

#include "../example_scene_keys.h"
#include "../../physics_demo/demo_obstacle.h"
#include "../../../engine/core/render/colors.h"
#include "../../../engine/gameplay/input/contracts/gameplay_input_frame_receiver.h"
#include "../../../engine/tools/debug_draw.h"

#include <memory>

namespace example::scene
{
namespace
{
using namespace example::physics_demo;

elysia::physics::PhysicsWorldConfig gravity_config(float gravity)
{
    elysia::physics::PhysicsWorldConfig config;
    config.gravity = {0, gravity};
    return config;
}

ObstacleConfig aabb_obstacle(const elysia::core::Rect& rect,
    elysia::core::Color color)
{
    ObstacleConfig config;
    config.rect = rect;
    config.color = color;
    config.shape = elysia::physics::AabbShape{{0, 0, rect.width(), rect.height()}};
    return config;
}

class QueryProbe final
    : public elysia::core::GameObject
    , public elysia::gameplay::GameplayInputFrameReceiver
{
public:
    QueryProbe(elysia::physics::PhysicsWorld& world, BlockCombatActor& player)
        : GameObject(elysia::core::DepthLayer::Item), _world(&world), _player(&player) {}

    void on_gameplay_input_frame(
        const elysia::gameplay::GameplayInputFrame& input) override
    {
        if (!input.secondary_pressed() || !_world || !_player)
            return;
        elysia::core::Vector2 direction{1, 0};
        switch (_player->facing())
        {
        case Facing::Left: direction = {-1, 0}; break;
        case Facing::Right: direction = {1, 0}; break;
        case Facing::Up: direction = {0, -1}; break;
        case Facing::Down: direction = {0, 1}; break;
        }
        const auto start = _player->center();
        const auto end = start + direction * 260.0f;
        elysia::physics::SegmentCastQuery query;
        query.start = start;
        query.end = end;
        query.filter = {collision_layers::Body, collision_layers::World, 0};
        auto* debug = elysia::tools::DebugDraw::instance();
        debug->clear_categories(elysia::tools::DebugDrawCategory::Gameplay);
        const auto hit = _world->segment_cast(query);
        debug->draw_line(elysia::tools::DebugDrawCategory::Gameplay,
            start, hit ? hit->point : end, elysia::core::colors::green_500, 2.0f);
        if (hit)
        {
            debug->draw_point(elysia::tools::DebugDrawCategory::Gameplay,
                hit->point, 8.0f, elysia::core::colors::red_500);
            debug->draw_line(elysia::tools::DebugDrawCategory::Gameplay,
                hit->point, hit->point + hit->normal * 28.0f,
                elysia::core::colors::yellow_500, 2.0f);
        }
    }
private:
    elysia::physics::PhysicsWorld* _world = nullptr;
    BlockCombatActor* _player = nullptr;
};
}

PhysicsCollisionTestScene::PhysicsCollisionTestScene()
    : PhysicsDemoSceneBase(ExampleSceneKeys::PhysicsCollisionTest,
          "PhysicsCollisionTestScene",
          gravity_config(1200.0f), "Collider Combat Demo",
          "A/D Move | Space Jump | J Attack | K Query")
{
}

void PhysicsCollisionTestScene::build_demo()
{
    using namespace example::physics_demo;
    set_demo_camera_center({640.0f, 360.0f});
    create_and_add_object<StaticBlockObstacle>(solid_texture(),
        aabb_obstacle({0, 650, 1280, 70}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(solid_texture(),
        aabb_obstacle({0, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(solid_texture(),
        aabb_obstacle({1252, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(solid_texture(),
        aabb_obstacle({900, 500, 28, 150}, elysia::core::colors::gray_700));

    auto platform = aabb_obstacle({350, 520, 240, 18},
        elysia::core::colors::yellow_700);
    platform.one_way = elysia::physics::OneWayCollision{
        elysia::physics::PassThroughDirection::Up, 0.02f};
    create_and_add_object<StaticBlockObstacle>(solid_texture(), platform);

    auto trigger = aabb_obstacle({620, 570, 100, 80},
        elysia::core::Color{0, 188, 212, 120});
    trigger.response = elysia::physics::CollisionResponse::Overlap;
    create_and_add_object<StaticBlockObstacle>(solid_texture(), trigger);

    auto* player = add_actor<PlatformPlayerCharacter>(
        elysia::core::Rect{120, 580, 34, 56});
    if (!player)
        return;
    set_player(*player);
    (void)add_actor<StationaryEnemy>(
        elysia::core::Rect{690, 594, 38, 52}, *player);
    (void)add_actor<PlatformPatrolEnemy>(
        elysia::core::Rect{1000, 594, 38, 52}, *player, 950.0f, 1180.0f);

    auto box = aabb_obstacle({470, 430, 38, 38}, elysia::core::colors::purple_500);
    box.material = {1.1f, 0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(solid_texture(), box);
    auto circle = aabb_obstacle({540, 420, 36, 36}, elysia::core::colors::pink_500);
    circle.shape = elysia::physics::CircleShape{{18, 18}, 18};
    circle.material = {0.2f, 0.1f, 0.65f};
    create_and_add_object<DynamicBlockObstacle>(solid_texture(), circle);

    auto moving_platform = aabb_obstacle(
        {670, 430, 180, 18}, elysia::core::colors::purple_700);
    moving_platform.material = {1.0f, 0.8f, 0.0f};
    create_and_add_object<KinematicMovingPlatform>(
        solid_texture(), moving_platform, 650.0f, 850.0f, 90.0f);
    auto passenger = aabb_obstacle(
        {735, 390, 34, 38}, elysia::core::colors::orange_500);
    passenger.material = {1.0f, 0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(solid_texture(), passenger);

    auto ccd = aabb_obstacle({70, 210, 24, 24}, elysia::core::colors::cyan_500);
    ccd.detection = elysia::physics::CollisionDetectionMode::Continuous;
    auto* fast = create_and_add_object<DynamicBlockObstacle>(
        solid_texture(), ccd, elysia::core::Vector2{1500, 0});
    if (fast)
        fast->physics_body()->gravity_scale = 0.0f;

    create_and_add_object<QueryProbe>(physics_world(), *player);
}

PlatformTilePhysicsTestScene::PlatformTilePhysicsTestScene()
    : PhysicsDemoSceneBase(ExampleSceneKeys::PlatformTilePhysicsTest,
          "PlatformTilePhysicsTestScene",
          gravity_config(1400.0f), "Platform Tile Combat Demo",
          "A/D Move | Space Jump | Down+Space Drop | J Attack")
{
}

void PlatformTilePhysicsTestScene::build_demo()
{
    using namespace example::physics_demo;
    set_demo_camera_center({320.0f, 360.0f});
    auto* map = create_and_add_object<DemoTileMap>(solid_texture(),
        elysia::core::Vector2{-320, 40}, elysia::core::Vector2{32, 32},
        40, 20, elysia::physics::TileOutOfBoundsPolicy::Block);
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
        elysia::core::Rect{180, 552, 38, 52}, *player, 80.0f, 360.0f);
    (void)add_actor<PlatformPatrolEnemy>(
        elysia::core::Rect{650, 520, 38, 52}, *player, 560.0f, 820.0f);
}

TopDownTilePhysicsTestScene::TopDownTilePhysicsTestScene()
    : PhysicsDemoSceneBase(ExampleSceneKeys::TopDownTilePhysicsTest,
          "TopDownTilePhysicsTestScene",
          gravity_config(0.0f), "Top-Down Tile Combat Demo",
          "WASD Move | J Attack | Enemies require line of sight")
{
}

void TopDownTilePhysicsTestScene::build_demo()
{
    using namespace example::physics_demo;
    set_demo_camera_center({576.0f, 344.0f});
    auto* map = create_and_add_object<DemoTileMap>(solid_texture(),
        elysia::core::Vector2{-64, 80}, elysia::core::Vector2{32, 24},
        40, 22, elysia::physics::TileOutOfBoundsPolicy::Block);
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
