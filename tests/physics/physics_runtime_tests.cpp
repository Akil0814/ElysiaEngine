#include "physics_test_support.h"
#include <functional>
#include <iostream>
#include <sstream>

namespace
{
std::string capture_logs(const std::function<void()>& action)
{
    std::ostringstream captured;
    std::streambuf* const previous_buffer = std::clog.rdbuf(captured.rdbuf());
    action();
    std::clog.rdbuf(previous_buffer);
    return captured.str();
}

std::size_t count_occurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}
}

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

    Probe debug_actor;
    PhysicsWorld debug_world;
    debug_actor.add(debug_world);
    debug_world.set_debug_capture(PhysicsDebugCapture::Shapes);
    step(debug_world);
    require(debug_world.debug_snapshot().shapes.size() == 1,
            "Shape capture produces a debug snapshot");
    debug_world.set_debug_capture(PhysicsDebugCapture::Shapes);
    require(debug_world.advance(1.0 / 120.0) == 0 &&
                debug_world.debug_snapshot().shapes.size() == 1,
            "Repeated capture mode preserves the snapshot between fixed steps");
    debug_world.set_debug_capture(static_cast<PhysicsDebugCapture>(0xff));
    require(debug_world.debug_capture() == PhysicsDebugCapture::All &&
                debug_world.debug_snapshot().shapes.size() == 1,
            "Capture mode masks unsupported bits and immediately refreshes paused diagnostics");
    step(debug_world);
    require(debug_world.debug_snapshot().shapes.size() == 1,
            "Changed capture mode produces a fresh snapshot");
    debug_world.set_debug_capture(PhysicsDebugCapture::None);
    require(debug_world.debug_snapshot().shapes.empty(),
            "Disabling capture clears the active snapshot");

    Probe super_elastic;
    super_elastic.collider.material.restitution = 1.25f;
    PhysicsWorld material_world;
    const auto super_elastic_handle = super_elastic.add(material_world);
    require(super_elastic_handle.is_valid(),
        "Restitution above one must register successfully");
    super_elastic.collider.material.restitution = 1.5f;
    require(material_world.update_collider(
                material_world.collider_id(super_elastic_handle, 0), super_elastic.collider),
        "Restitution above one must update successfully");

    Probe invalid_registration;
    invalid_registration.collider.material.friction = -0.25f;
    PhysicsWorld validation_world;
    PhysicsObjectHandle invalid_registration_handle{};
    const std::string registration_log = capture_logs([&]
    {
        invalid_registration_handle = invalid_registration.add(validation_world);
    });
    require(!invalid_registration_handle.is_valid()
            && registration_log.find("[WARN]") != std::string::npos
            && registration_log.find("[physics]") != std::string::npos
            && registration_log.find("collider_index=0") != std::string::npos
            && registration_log.find("friction must be finite and non-negative")
                != std::string::npos,
        "Invalid collider registration must be rejected and logged with its reason");

    Probe invalid_update;
    const auto invalid_update_handle = invalid_update.add(validation_world);
    const auto invalid_update_id = validation_world.collider_id(invalid_update_handle, 0);
    invalid_update.collider.material.restitution = -0.5f;
    const std::string update_log = capture_logs([&]
    {
        require(!validation_world.update_collider(invalid_update_id, invalid_update.collider),
            "Invalid collider update must be rejected");
        require(!validation_world.update_collider(invalid_update_id, invalid_update.collider),
            "Repeated invalid collider update must remain rejected");
    });
    require(update_log.find("collider_id=" + std::to_string(invalid_update_id))
                != std::string::npos
            && update_log.find("restitution must be finite and non-negative")
                != std::string::npos
            && count_occurrences(update_log, "Rejected collider update") == 1,
        "Repeated invalid collider updates must log once per collider and reason");

    std::cout << "physics simulation tests passed\n";
}
