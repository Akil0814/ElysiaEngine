#include "game/showcase/shared/collider_render.h"
#include "game/showcase/gameplay/runtime/demo_obstacle.h"

#include "engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace example::showcase::gameplay
{
namespace
{
elysia::physics::Collider make_collider(const ObstacleConfig& config)
{
    elysia::physics::Collider collider;
    collider.shape = config.shape;
    collider.filter = {collision_layers::World,
        collision_layers::Body | collision_layers::Trigger, 0};
    collider.response = config.response;
    collider.detection_mode = config.detection;
    collider.one_way = config.one_way;
    collider.material = config.material;
    collider.tag = config.response == elysia::physics::CollisionResponse::Overlap
        ? "trigger" : "world";
    return collider;
}

void submit_obstacle_shape(
    const elysia::physics::Collider& collider,
    elysia::core::Vector2 owner_origin,
    elysia::core::Color color,
    std::vector<elysia::core::RenderCommand>& out_commands)
{
    example::showcase::submit_collider_shape(collider.shape,{owner_origin,0},color,out_commands);
}
}

StaticBlockObstacle::StaticBlockObstacle(ObstacleConfig config)
    : ColoredBlockObject(elysia::core::DepthLayer::Terrain,
          config.rect, config.color),
      _collider(make_collider(config))
{
}

void StaticBlockObstacle::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    submit_obstacle_shape(_collider, render_rect().position(), display_color(), out_commands);
}

DynamicBlockObstacle::DynamicBlockObstacle(
    ObstacleConfig config,
    elysia::core::Vector2 velocity)
    : ColoredBlockObject(elysia::core::DepthLayer::Item,
          config.rect, config.color),
      _collider(make_collider(config))
{
    _collider.filter = {collision_layers::Body,
        collision_layers::World | collision_layers::Body, 0};
    _body.type = elysia::physics::BodyType::Dynamic;
    _body.velocity = velocity;
    _body.mass_properties = elysia::physics::ExplicitMass{1.0f};
}

void DynamicBlockObstacle::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    submit_obstacle_shape(_collider, render_rect().position(), display_color(), out_commands);
}

KinematicMovingPlatform::KinematicMovingPlatform(
    ObstacleConfig config,
    float left,
    float right,
    float speed)
    : ColoredBlockObject(elysia::core::DepthLayer::Terrain,
          config.rect, config.color),
      _collider(make_collider(config)),
      _left(std::min(left, right)),
      _right(std::max(left, right)),
      _speed(std::fabs(speed))
{
    _body.type = elysia::physics::BodyType::Kinematic;
    _body.gravity_scale = 0.0f;
    _body.velocity = {_speed, 0.0f};
}

void KinematicMovingPlatform::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    submit_obstacle_shape(_collider, render_rect().position(), display_color(), out_commands);
}

void KinematicMovingPlatform::fixed_update(double delta)
{
    (void)delta;
    if (position().x <= _left)
        set_velocity_x(_speed);
    else if (position().x >= _right)
        set_velocity_x(-_speed);
}
}
