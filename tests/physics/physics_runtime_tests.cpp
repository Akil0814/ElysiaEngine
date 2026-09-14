#include "physics_test_support.h"
#include <iostream>
int main()
{
    // Circle CCD and resting/sleep support.
    Probe floor, ball;
    floor.definition.type = BodyType::Static;
    floor.set_position({0, 200});
    floor.collider.shape = AabbShape{{0, 0, 400, 5}};
    ball.collider.shape = CircleShape{{10, 10}, 10};
    ball.definition.bullet = true;
    ball.definition.velocity = {0, 9000};
    ball.set_position({100, 0});
    PhysicsWorldConfig c;
    c.gravity = {0, 1000};
    PhysicsWorld w(c);
    auto f = floor.add(w), b = ball.add(w);
    step(w);
    require(ball.position().y < 200, "Fast circle does not tunnel through thin wall");
    step(w, 240);
    require(ball.position().y < 200 && ball.position().y > 160, "Circle rests above floor");
    require(!w.body_state(b)->awake, "Resting body sleeps");
    w.apply_impulse(b, {0, -100});
    require(w.body_state(b)->awake, "Impulse wakes body");
    // Rotation and actual narrow overlap against rotated rectangle.
    Probe rotated;
    rotated.collider.shape = AabbShape{{-50, -5, 100, 10}};
    rotated.definition.angle = 1.57079632679f;
    rotated.definition.fixed_rotation = false;
    rotated.definition.type = BodyType::Static;
    PhysicsWorld queries;
    auto h = rotated.add(queries);
    std::vector<CollisionOverlapQueryHit> hits;
    queries.overlap_aabb({{20, -2, 4, 4}, {}}, hits);
    require(hits.empty(), "Rotated box query does not use its unrotated bounds");
    auto ray = queries.raycast({{-100, 0}, {1, 0}, 200, {}});
    require(ray && near(ray->distance, 95, 0.5f), "Ray sees rotated shape");
    queries.teleport_object(h, {200, 0});
    require(!queries.raycast({{-100, 0}, {1, 0}, 200, {}}), "Query sees teleport immediately");
    // Rigid distance and spring.
    Probe anchor, bob;
    anchor.definition.type = BodyType::Static;
    bob.set_position({100, 0});
    bob.definition.fixed_rotation = false;
    PhysicsWorld pendulum(c);
    auto ah = anchor.add(pendulum), bh = bob.add(pendulum);
    auto j = pendulum.create_distance_joint({ah, bh, {}, {}, 100});
    step(pendulum, 120);
    auto js = pendulum.joint_state(j);
    require(js && near((js->anchor_second - js->anchor_first).length(), 100, 1),
            "Distance constraint holds");
    pendulum.destroy_joint(j);
    auto spring = pendulum.create_distance_joint({ah, bh, {}, {}, 100, true, 3, 0.7f});
    step(pendulum, 120);
    require(pendulum.joint_state(spring).has_value(), "Spring remains valid");
    // Clockwise motor with limits.
    Probe pivot, arm;
    pivot.definition.type = BodyType::Static;
    arm.definition.fixed_rotation = false;
    PhysicsWorld motor;
    auto p = pivot.add(motor), a = arm.add(motor);
    RevoluteJointDefinition rd;
    rd.first = p;
    rd.second = a;
    rd.enable_motor = true;
    rd.motor_speed = 1;
    rd.max_motor_torque = 100;
    rd.enable_limit = true;
    rd.lower_angle = 0;
    rd.upper_angle = 0.5f;
    auto rj = motor.create_revolute_joint(rd);
    step(motor, 120);
    auto state = motor.body_state(a);
    require(state && state->angle > 0.3f && state->angle < 0.6f,
            "Clockwise motor respects radian limits");
    std::cout << "physics simulation tests passed\n";
}
