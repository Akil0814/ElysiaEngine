#include "physics_test_support.h"
#include <iostream>
int main()
{
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
