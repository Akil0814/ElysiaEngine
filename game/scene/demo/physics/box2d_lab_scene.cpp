#include "box2d_lab_scene.h"
#include "../../../../engine/core/render/render_command.h"
#include "../../../../engine/physics/contracts/physics_participant.h"
#include "../../example_scene_keys.h"
#include "physics_combat_demo_helpers.h"
#include <cmath>
namespace example::scene
{
namespace
{
using namespace elysia::physics;
using elysia::core::Vector2;
class LabJoint final : public elysia::core::GameObject
{
  public:
    LabJoint(PhysicsWorld &world, JointHandle handle)
        : GameObject(elysia::core::DepthLayer::Item), _world(world), _handle(handle)
    {
    }
    void submit_render_commands(std::vector<elysia::core::RenderCommand> &out) const override
    {
        if (auto state = _world.joint_state(_handle))
            out.push_back(elysia::core::make_world_draw_line_command(
                state->anchor_first, state->anchor_second, {160, 210, 235}, 2));
    }

  private:
    PhysicsWorld &_world;
    JointHandle _handle;
};
class LabBody final : public elysia::core::GameObject, public PhysicsParticipant
{
  public:
    LabBody(Vector2 pos, Vector2 size, BodyDefinition body, bool circle = false)
        : GameObject(elysia::core::DepthLayer::Item), _body(body), _circle(circle)
    {
        set_world_rect({pos, size});
        _collider.shape =
            circle ? ColliderShape(CircleShape{{}, size.x / 2})
                   : ColliderShape(AabbShape{{-size.x / 2, -size.y / 2, size.x, size.y}});
        _collider.material = {0.6f, circle ? 0.4f : 0.0f};
    }
    BodyDefinition body_definition() const override
    {
        return _body;
    }
    std::span<const Collider> collider_definitions() const override
    {
        return {&_collider, 1};
    }
    void submit_render_commands(std::vector<elysia::core::RenderCommand> &out) const override
    {
        PhysicsPose pose{position(), _body.angle};
        if (physics_world())
        {
            auto p = physics_world()->render_pose(physics_handle());
            if (p)
                pose = *p;
        }
        auto state = physics_state();
        elysia::core::Color color = state && !state->awake ? elysia::core::Color{85, 125, 150}
                                                           : elysia::core::Color{240, 175, 70};
        if (_body.type == BodyType::Static)
            color = {90, 100, 115};
        if (_circle)
        {
            out.push_back(
                elysia::core::make_world_fill_circle_command(pose.position, size().x / 2, color));
            return;
        }
        auto rotate = [&](Vector2 v) {
            float c = std::cos(pose.angle), s = std::sin(pose.angle);
            return pose.position + Vector2{c * v.x - s * v.y, s * v.x + c * v.y};
        };
        auto half = size() * 0.5f;
        Vector2 a = rotate({-half.x, -half.y}), b = rotate({half.x, -half.y}),
                c = rotate({half.x, half.y}), d = rotate({-half.x, half.y});
        out.push_back(elysia::core::make_world_fill_triangle_command(a, b, c, color));
        out.push_back(elysia::core::make_world_fill_triangle_command(a, c, d, color));
    }

  private:
    BodyDefinition _body;
    Collider _collider;
    bool _circle;
};
} // namespace
Box2DLabScene::Box2DLabScene()
    : PhysicsCombatDemoSceneBase(
          example::scene_keys::Box2DLab, "Box2DLabScene", detail::make_gravity_config(980),
          "Box2D: Stacks, Springs & Motors", "Space Wake / Kick | Blue = Sleeping")
{
}
void Box2DLabScene::build_demo()
{
    _dynamic_bodies.clear();
    set_demo_camera_center({640, 360});
    BodyDefinition fixed;
    fixed.type = BodyType::Static;
    BodyDefinition moving;
    moving.fixed_rotation = false;
    auto add = [&](Vector2 p, Vector2 size, BodyDefinition d, bool circle = false) {
        auto *o = create_and_add_object<LabBody>(p, size, d, circle);
        if (d.type == BodyType::Dynamic)
            _dynamic_bodies.push_back(o->physics_handle());
        return o->physics_handle();
    };
    add({640, 510}, {590, 20}, fixed);
    add({925, 390}, {10, 220}, fixed);
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 3; ++x)
            add({405.f + x * 25, 495.f - y * 25}, {24, 24}, moving);
    auto anchor = add({595, 285}, {12, 12}, fixed);
    auto bob = add({595, 355}, {28, 28}, moving, true);
    auto spring = physics_world().create_distance_joint({anchor, bob, {}, {}, 70, true, 2, 0.4f});
    create_and_add_object<LabJoint>(physics_world(), spring);
    auto pivot = add({750, 330}, {12, 12}, fixed);
    auto arm = add({750, 365}, {16, 90}, moving);
    RevoluteJointDefinition pendulum;
    pendulum.first = pivot;
    pendulum.second = arm;
    pendulum.local_anchor_second = {0, -35};
    pendulum.enable_limit = true;
    pendulum.lower_angle = -0.7f;
    pendulum.upper_angle = 0.7f;
    auto pendulum_joint = physics_world().create_revolute_joint(pendulum);
    create_and_add_object<LabJoint>(physics_world(), pendulum_joint);
    physics_world().apply_impulse(arm, {80, 0});
    auto motor_anchor = add({840, 425}, {12, 12}, fixed);
    auto blade = add({840, 425}, {90, 12}, moving);
    RevoluteJointDefinition motor;
    motor.first = motor_anchor;
    motor.second = blade;
    motor.enable_motor = true;
    motor.motor_speed = 2;
    motor.max_motor_torque = 200000;
    physics_world().create_revolute_joint(motor);
    auto bullet = moving;
    bullet.bullet = true;
    bullet.gravity_scale = 0;
    bullet.velocity = {8000, 0};
    add({510, 480}, {10, 10}, bullet, true);
}
void Box2DLabScene::on_input(const elysia::input::RawInputFrame &f,
                             const std::vector<elysia::input::RawInputEvent> &events)
{
    for (auto &e : events)
        if (e.type == elysia::input::RawInputEventType::ControlPressed &&
            e.control == elysia::input::RawInputControl::KeySpace)
            for (auto h : _dynamic_bodies)
            {
                auto state = physics_world().body_state(h);
                if (state)
                    physics_world().apply_impulse(h, {state->mass * 80, -state->mass * 180});
            }
    PhysicsCombatDemoSceneBase::on_input(f, events);
}
} // namespace example::scene
