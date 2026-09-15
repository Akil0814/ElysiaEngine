#include "physics_test_support.h"
#include "engine/physics/contracts/physics_participant.h"
#include <iostream>
#include <limits>

struct VelocityProbe : Probe, PhysicsParticipant
{
    std::span<const Collider> collider_definitions() const override
    {
        return {&collider, 1};
    }
};

void velocity_command_order()
{
    VelocityProbe actor;
    PhysicsWorld world;
    auto handle = actor.add(world);
    actor.bind_physics(world, handle);
    require(world.set_velocity_x(handle, 12) && world.set_velocity_y(handle, 24),
            "Immediate component setters accept live handle");
    require(near(actor.velocity().x, 12) && near(actor.velocity().y, 24),
            "Immediate component setters preserve other component");
    require(!world.set_velocity_x({}, 1) && !world.set_velocity_y({}, 1) &&
                !world.set_velocity_x(handle, std::numeric_limits<float>::infinity()) &&
                !world.set_velocity_y(handle, std::numeric_limits<float>::quiet_NaN()),
            "Component setters reject invalid handles and nonfinite values");
    actor.set_velocity({});
    actor.tick = [&] {
        actor.set_velocity_x(100);
        actor.set_velocity_y(200);
    };
    step(world);
    auto velocity = actor.velocity();
    require(near(velocity.x, 100) && near(velocity.y, 200),
            "Queued X and Y setters preserve both components");
    actor.tick = [&] {
        actor.set_velocity_y(300);
        actor.set_velocity_x(400);
    };
    step(world);
    velocity = actor.velocity();
    require(near(velocity.x, 400) && near(velocity.y, 300),
            "Queued Y and X setters preserve both components");
    actor.tick = [&] {
        actor.set_velocity_x(500);
        actor.set_velocity({10, 20});
        actor.set_velocity_y(30);
    };
    step(world);
    velocity = actor.velocity();
    require(near(velocity.x, 10) && near(velocity.y, 30),
            "Full velocity and component writes follow submission order");
    Probe spawned;
    spawned.set_position({1000, 1000});
    PhysicsObjectHandle created;
    actor.tick = [&] {
        created = spawned.add(world);
        require(world.set_velocity_x(created, 40) && world.set_velocity_y(created, 50),
                "Pending registration accepts component writes");
    };
    step(world);
    velocity = world.body_state(created)->velocity;
    require(near(velocity.x, 40) && near(velocity.y, 50),
            "Component writes execute after pending registration");
    actor.tick = [&] {
        require(world.set_velocity_x(created, 60) && world.set_velocity_y(created, 70),
                "Pre-removal component writes accepted");
        require(world.unregister_object(created), "Removal queued after component writes");
        require(!world.set_velocity_x(created, 80) && !world.set_velocity_y(created, 90),
                "Post-removal component writes rejected");
    };
    step(world);
    require(!world.contains_object(created), "Queued component writes and removal complete");
}

void teleport_command_order()
{
    Probe actor;
    actor.definition.type = BodyType::Static;
    PhysicsWorld world;
    auto handle = actor.add(world);
    actor.tick = [&] {
        world.set_transform(handle, {{0, 0}, 1});
        world.teleport_object(handle, {100, 100});
    };
    step(world);
    auto state = world.body_state(handle);
    require(near(state->angle, 1) && near(state->position.x, 100),
            "Queued teleport preserves preceding rotation");
    actor.tick = [&] {
        world.teleport_object(handle, {200, 200});
        world.set_transform(handle, {{300, 300}, 2});
    };
    step(world);
    state = world.body_state(handle);
    require(near(state->angle, 2) && near(state->position.x, 300),
            "Queued full transform overrides preceding teleport");
    world.set_transform(handle, {{300, 300}, 1});
    const float angle = world.body_state(handle)->angle;
    for (int i = 0; i < 100; ++i)
        world.teleport_object(handle, {300, 300});
    require(near(world.body_state(handle)->angle, angle, 0.0001f),
            "Position-only teleports do not accumulate angle conversion error");

    Probe spawned;
    spawned.definition.fixed_rotation = false;
    PhysicsObjectHandle created;
    actor.tick = [&] {
        created = spawned.add(world);
        world.set_transform(created, {{0, 0}, 1});
        world.set_velocity(created, {12, 24});
        world.set_angular_velocity(created, 3);
        world.teleport_object(created, {500, 500});
    };
    step(world);
    state = world.body_state(created);
    require(near(state->angle, 1.05f) && near(state->velocity.x, 12) &&
                near(state->velocity.y, 24) && near(state->angular_velocity, 3),
            "Pending registration accepts ordered rotation, velocity and preserving teleport");
    actor.tick = [&] {
        world.set_transform(created, {{0, 0}, 2});
        world.teleport_object(created, {600, 600}, TeleportVelocityMode::Clear);
    };
    step(world);
    state = world.body_state(created);
    require(near(state->angle, 2) && near(state->position.x, 600) &&
                near(state->velocity.length(), 0) && near(state->angular_velocity, 0),
            "Clearing teleport preserves queued angle and clears both velocities");
}

int main()
{
    velocity_command_order();
    teleport_command_order();
    Probe a, b;
    a.collider.response = CollisionResponse::Overlap;
    PhysicsWorld w;
    auto ah = a.add(w), bh = b.add(w);
    Events e;
    w.add_listener(e);
    step(w);
    require(e.count(CollisionEventPhase::Begin) == 1, "Sensor Begin");
    w.unregister_object(bh);
    require(!w.body_state(bh) && !w.set_velocity(bh, {1, 0}),
            "Removed handle rejects state and commands");
    step(w, 2);
    require(e.count(CollisionEventPhase::End) == 1, "Exactly one End after destroy");
    auto next = b.add(w);
    require(next != bh, "Engine IDs never reused");
    step(w);
    require(e.count(CollisionEventPhase::Begin) == 2, "Recreated shape begins independently");
    w.reset();
    require(!w.contains_object(ah) && !w.contains_object(next), "Reset invalidates objects");
    step(w);
    require(e.count(CollisionEventPhase::End) == 1, "Reset is silent");
    Probe source, spawned;
    PhysicsWorld ordered;
    auto sh = source.add(ordered);
    PhysicsObjectHandle created{};
    bool once = false;
    int ticks = 0;
    spawned.tick = [&] { ++ticks; };
    source.tick = [&] {
        if (once)
            return;
        once = true;
        created = spawned.add(ordered);
        require(ordered.teleport_object(created, {100, 20}),
                "Pending registration accepts teleport");
    };
    step(ordered);
    require(created.is_valid() && near(spawned.position().x, 100) && ticks == 0,
            "Register then teleport preserves order without running new callback");
    step(ordered);
    require(ticks == 1, "Spawn callback begins next step");
    source.tick = [&] {
        require(ordered.teleport_object(created, {200, 30}), "Pre-removal command accepted");
        require(ordered.unregister_object(created), "Removal accepted");
        require(!ordered.teleport_object(created, {300, 40}), "Post-removal command rejected");
    };
    step(ordered);
    require(near(spawned.position().x, 200), "Pre-removal command is committed before removal");
    source.tick = [&] { ordered.reset(); };
    step(ordered);
    require(!ordered.contains_object(sh), "Reset from fixed callback");
    Probe left, right;
    right.set_position({100, 0});
    PhysicsWorld joints;
    auto l = left.add(joints), r = right.add(joints);
    auto joint = joints.create_distance_joint({l, r, {}, {}, 100});
    require(joints.joint_state(joint).has_value(), "Joint created");
    joints.unregister_object(r);
    require(!joints.joint_state(joint), "Destroy body invalidates joint");
    std::cout << "physics lifecycle tests passed\n";
}
