#include "collider_combat_demo_scene.h"

#include "physics_combat_demo_helpers.h"
#include "../../example_scene_keys.h"
#include "../../../demo/physics/demo_obstacle.h"
#include "../../../../engine/core/render/colors.h"

namespace example::scene
{
ColliderCombatDemoScene::ColliderCombatDemoScene()
    : PhysicsCombatDemoSceneBase(
          example::scene_keys::ColliderCombatDemo,
          "ColliderCombatDemoScene",
          detail::make_gravity_config(1200.0f),
          "Collider Combat Demo",
          "A/D Move | Space Jump | J Attack | K Query")
{
}

void ColliderCombatDemoScene::build_demo()
{
    using namespace example::demo::physics;
    set_demo_camera_center({640.0f, 360.0f});
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle(
            {0, 650, 1280, 70}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle(
            {0, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle(
            {1252, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle(
            {900, 500, 28, 150}, elysia::core::colors::gray_700));

    auto platform = detail::make_aabb_obstacle(
        {350, 520, 240, 18}, elysia::core::colors::yellow_700);
    platform.one_way = elysia::physics::OneWayCollision{
        elysia::physics::PassThroughDirection::Up, 0.02f};
    create_and_add_object<StaticBlockObstacle>(platform);

    auto trigger = detail::make_aabb_obstacle(
        {620, 570, 100, 80}, elysia::core::Color{0, 188, 212, 120});
    trigger.response = elysia::physics::CollisionResponse::Overlap;
    create_and_add_object<StaticBlockObstacle>(trigger);

    auto* player = add_actor<PlatformPlayerCharacter>(
        elysia::core::Rect{120, 580, 34, 56});
    if (!player)
        return;
    set_player(*player);
    (void)add_actor<StationaryEnemy>(
        elysia::core::Rect{690, 594, 38, 52}, *player);
    (void)add_actor<PlatformPatrolEnemy>(
        elysia::core::Rect{1000, 594, 38, 52},
        *player,
        950.0f,
        1180.0f);

    auto box = detail::make_aabb_obstacle(
        {470, 430, 38, 38}, elysia::core::colors::purple_500);
    box.material = {1.1f, 0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(box);

    auto circle = detail::make_aabb_obstacle(
        {540, 420, 36, 36}, elysia::core::colors::pink_500);
    circle.shape = elysia::physics::CircleShape{{18, 18}, 18};
    circle.material = {0.2f, 0.1f, 0.65f};
    create_and_add_object<DynamicBlockObstacle>(circle);

    auto moving_platform = detail::make_aabb_obstacle(
        {670, 430, 180, 18}, elysia::core::colors::purple_700);
    moving_platform.material = {1.0f, 0.8f, 0.0f};
    create_and_add_object<KinematicMovingPlatform>(
        moving_platform, 650.0f, 850.0f, 90.0f);
    auto passenger = detail::make_aabb_obstacle(
        {735, 390, 34, 38}, elysia::core::colors::orange_500);
    passenger.material = {1.0f, 0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(passenger);

    auto ccd = detail::make_aabb_obstacle(
        {70, 210, 24, 24}, elysia::core::colors::cyan_500);
    ccd.detection = elysia::physics::CollisionDetectionMode::Continuous;
    auto* fast = create_and_add_object<DynamicBlockObstacle>(
        ccd, elysia::core::Vector2{1500, 0});
    if (fast)
        fast->physics_body()->gravity_scale = 0.0f;

    create_and_add_object<detail::QueryProbe>(physics_world(), *player);
}
}
