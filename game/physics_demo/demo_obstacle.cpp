#include "demo_obstacle.h"

namespace example::physics_demo
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
    collider.tag = config.response == elysia::physics::CollisionResponse::Overlap
        ? "trigger" : "world";
    return collider;
}
}

StaticBlockObstacle::StaticBlockObstacle(
    SolidColorTexture& texture, ObstacleConfig config)
    : ColoredBlockObject(texture, elysia::core::DepthLayer::Terrain,
          config.rect, config.color),
      _collider(make_collider(config))
{
}

DynamicBlockObstacle::DynamicBlockObstacle(
    SolidColorTexture& texture, ObstacleConfig config,
    elysia::core::Vector2 velocity)
    : ColoredBlockObject(texture, elysia::core::DepthLayer::Item,
          config.rect, config.color),
      _collider(make_collider(config))
{
    _collider.filter = {collision_layers::Body,
        collision_layers::World | collision_layers::Body, 0};
    _body.type = elysia::physics::BodyType::Dynamic;
    _body.velocity = velocity;
    _body.mass = 1.0f;
    _body.max_speed = {2000.0f, 2000.0f};
}
}
