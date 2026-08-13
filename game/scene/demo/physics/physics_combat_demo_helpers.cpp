#include "physics_combat_demo_helpers.h"

#include "../../../demo/physics/demo_collision_layers.h"
#include "../../../../engine/core/render/colors.h"
#include "../../../../engine/tools/debug_draw.h"

namespace example::scene::detail
{
elysia::physics::PhysicsWorldConfig make_gravity_config(
    float gravity) noexcept
{
    elysia::physics::PhysicsWorldConfig config;
    config.gravity = {0, gravity};
    return config;
}

example::demo::physics::ObstacleConfig make_aabb_obstacle(
    const elysia::core::Rect& rect,
    elysia::core::Color color) noexcept
{
    example::demo::physics::ObstacleConfig config;
    config.rect = rect;
    config.color = color;
    config.shape = elysia::physics::AabbShape{
        {0, 0, rect.width(), rect.height()}};
    return config;
}

QueryProbe::QueryProbe(
    elysia::physics::PhysicsWorld& world,
    example::demo::physics::BlockCombatActor& player) noexcept
    : GameObject(elysia::core::DepthLayer::Item)
    , _world(&world)
    , _player(&player)
{
}

void QueryProbe::on_gameplay_input_frame(
    const elysia::gameplay::GameplayInputFrame& input)
{
    using example::demo::physics::Facing;
    namespace collision_layers = example::demo::physics::collision_layers;

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
    query.filter = {
        collision_layers::Body,
        collision_layers::World,
        0};

    auto* debug = elysia::tools::DebugDraw::instance();
    debug->clear_categories(elysia::tools::DebugDrawCategory::Gameplay);
    const auto hit = _world->segment_cast(query);
    debug->draw_line(
        elysia::tools::DebugDrawCategory::Gameplay,
        start,
        hit ? hit->point : end,
        elysia::core::colors::green_500,
        2.0f);
    if (!hit)
        return;

    debug->draw_point(
        elysia::tools::DebugDrawCategory::Gameplay,
        hit->point,
        8.0f,
        elysia::core::colors::red_500);
    debug->draw_line(
        elysia::tools::DebugDrawCategory::Gameplay,
        hit->point,
        hit->point + hit->normal * 28.0f,
        elysia::core::colors::yellow_500,
        2.0f);
}
}
