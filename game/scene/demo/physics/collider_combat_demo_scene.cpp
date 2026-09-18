#include "../../../input/local_controls.h"
#include "collider_combat_demo_scene.h"

#include "physics_combat_demo_helpers.h"
#include "../../example_scene_keys.h"
#include "../../../demo/physics/demo_obstacle.h"
#include "../../../../engine/core/render/colors.h"

namespace example::scene
{
namespace
{
class QueryPlayerController final : public elysia::gameplay::LocalPlayerController
{
  public:
    QueryPlayerController(detail::QueryProbe &probe)
        : LocalPlayerController(elysia::input::PrimaryLocalPlayer,
                                example::input::make_gameplay_input_map()),
          _probe(&probe)
    {
    }
    void object_removed(elysia::core::SceneObject &object)
    {
        if (_probe == &object)
            _probe = nullptr;
        else if (_probe)
            _probe->target_removed(object);
    }

  protected:
    void on_mapped_input(const elysia::input::ActionInputResult &result) override
    {
        for (const auto &event : result.events)
            if (event.action == example::input::actions::Secondary &&
                event.phase == elysia::input::ActionInputPhase::Started)
                ++_queries;
    }
    void produce_intent(std::uint64_t, double) override
    {
        auto count = std::exchange(_queries, 0u);
        if (_probe && !_probe->is_destroyed())
            for (unsigned i = 0; i < count; ++i)
                _probe->execute();
    }
    void cancelled(elysia::gameplay::InputCancelReason) override
    {
        _queries = 0;
    }

  private:
    detail::QueryProbe *_probe;
    unsigned _queries = 0;
};
} // namespace
ColliderCombatDemoScene::ColliderCombatDemoScene()
    : PhysicsCombatDemoSceneBase(example::scene_keys::ColliderCombatDemo, "ColliderCombatDemoScene",
                                 detail::make_gravity_config(1200.0f), "Collider Combat Demo",
                                 "A/D Move | Space Jump | J Attack | K Query")
{
}

void ColliderCombatDemoScene::build_demo()
{
    using namespace example::demo::physics;
    set_demo_camera_center({640.0f, 360.0f});
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle({0, 650, 1280, 70}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle({0, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle({1252, 0, 28, 720}, elysia::core::colors::gray_700));
    create_and_add_object<StaticBlockObstacle>(
        detail::make_aabb_obstacle({900, 500, 28, 150}, elysia::core::colors::gray_700));

    auto platform = detail::make_aabb_obstacle({350, 520, 240, 18}, elysia::core::colors::yellow_700);
    platform.one_way = elysia::physics::OneWayCollision{elysia::physics::PassThroughDirection::Up, 0.02f};
    create_and_add_object<StaticBlockObstacle>(platform);

    auto trigger = detail::make_aabb_obstacle({620, 570, 100, 80}, elysia::core::Color{0, 188, 212, 120});
    trigger.response = elysia::physics::CollisionResponse::Overlap;
    create_and_add_object<StaticBlockObstacle>(trigger);

    auto *player = add_actor<PlatformPlayerCharacter>(elysia::core::Rect{120, 580, 34, 56});
    if (!player)
        return;
    set_player(*player);
    (void)add_actor<StationaryEnemy>(elysia::core::Rect{690, 594, 38, 52}, *player);
    (void)add_actor<PlatformPatrolEnemy>(elysia::core::Rect{1000, 594, 38, 52}, *player, 950.0f, 1180.0f);

    auto box = detail::make_aabb_obstacle({470, 430, 38, 38}, elysia::core::colors::purple_500);
    box.material = {0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(box);

    auto circle = detail::make_aabb_obstacle({540, 420, 36, 36}, elysia::core::colors::pink_500);
    circle.shape = elysia::physics::CircleShape{{18, 18}, 18};
    circle.material = {0.1f, 0.65f};
    create_and_add_object<DynamicBlockObstacle>(circle);

    auto moving_platform = detail::make_aabb_obstacle({670, 430, 180, 18}, elysia::core::colors::purple_700);
    moving_platform.material = {0.8f, 0.0f};
    create_and_add_object<KinematicMovingPlatform>(moving_platform, 650.0f, 850.0f, 90.0f);
    auto passenger = detail::make_aabb_obstacle({735, 390, 34, 38}, elysia::core::colors::orange_500);
    passenger.material = {0.8f, 0.0f};
    create_and_add_object<DynamicBlockObstacle>(passenger);

    auto ccd = detail::make_aabb_obstacle({70, 210, 24, 24}, elysia::core::colors::cyan_500);
    ccd.detection = elysia::physics::CollisionDetectionMode::Continuous;
    auto *fast = create_and_add_object<DynamicBlockObstacle>(ccd, elysia::core::Vector2{1500, 0});
    if (fast)
        physics_world().set_gravity_scale(fast->physics_handle(), 0.0f);

    _query_probe = create_and_add_object<detail::QueryProbe>(physics_world(), *player);
}
} // namespace example::scene

void example::scene::ColliderCombatDemoScene::on_control_target_removing(elysia::core::SceneObject &object)
{
    if (auto *controller =
            elysia::gameplay::ControllerService::instance()->get<QueryPlayerController>(player_controller()))
        controller->object_removed(object);
    if (_query_probe == &object)
        _query_probe = nullptr;
}

void example::scene::ColliderCombatDemoScene::configure_player_controller(
    example::demo::physics::BlockCombatActor &player)
{
    auto *service = elysia::gameplay::ControllerService::instance();
    if (!service->get<QueryPlayerController>(player_controller()))
    {
        auto controller = service->create<QueryPlayerController>(
            {elysia::gameplay::ControllerScope::Scene, control_context().token()},
            *static_cast<detail::QueryProbe *>(_query_probe));
        if (!controller)
            throw std::logic_error("Query controller creation failed.");
        player_controller() = *controller;
    }
    if (!service->bind_target(player_controller(), control_context(), player).succeeded())
        throw std::logic_error("Query controller binding failed.");
}
