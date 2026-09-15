#include "physics_test_support.h"
#include <iostream>
struct Tiles : ITileCollisionWorld
{
    int width = 1;
    int empty_column = -1;
    TileCollisionCell cell{TileCollisionType::Block};
    Vector2 world_origin() const noexcept override
    {
        return {0, 100};
    }
    Vector2 tile_size() const noexcept override
    {
        return {100, 20};
    }
    int columns() const noexcept override
    {
        return width;
    }
    int rows() const noexcept override
    {
        return 1;
    }
    TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
    {
        return TileOutOfBoundsPolicy::Empty;
    }
    TileCollisionCell cell_at(TileCoordinate coordinate) const noexcept override
    {
        if (coordinate.x == empty_column)
            return {};
        return cell;
    }
};

Vector2 run_one_way_case(PassThroughDirection directions, Vector2 start, Vector2 velocity)
{
    Probe platform;
    platform.definition.type = BodyType::Static;
    platform.set_position({100, 100});
    platform.collider.shape = AabbShape{{0, 0, 20, 20}};
    platform.collider.one_way = OneWayCollision{directions, 1};

    Probe actor;
    actor.set_position(start);
    actor.definition.velocity = velocity;

    PhysicsWorld world;
    platform.add(world);
    actor.add(world);
    step(world, 24);
    return actor.position();
}

void runtime_continuous_detection()
{
    auto run = [](bool initial_continuous, bool final_continuous, bool explicit_bullet,
                  bool other_continuous) {
        Probe target, projectile;
        target.collider.shape = AabbShape{{0, 0, 2, 40}};
        target.set_position({50, -20});
        projectile.definition.velocity = {6000, 0};
        projectile.definition.bullet = explicit_bullet;
        Collider colliders[2];
        colliders[0].shape = CircleShape{{}, 1};
        colliders[0].detection_mode = initial_continuous ? CollisionDetectionMode::Continuous
                                                       : CollisionDetectionMode::Discrete;
        colliders[1].enabled = false;
        colliders[1].shape = CircleShape{{}, 1};
        colliders[1].detection_mode = other_continuous ? CollisionDetectionMode::Continuous
                                                     : CollisionDetectionMode::Discrete;
        PhysicsWorld world;
        target.add(world);
        auto handle = world.register_object(projectile, projectile.definition, colliders);
        require(handle.is_valid(), "CCD regression projectile registers");
        // Exercise the deferred update path before the first simulation step.
        projectile.tick = [&] {
            colliders[0].detection_mode = final_continuous ? CollisionDetectionMode::Continuous
                                                         : CollisionDetectionMode::Discrete;
            require(world.update_collider(world.collider_id(handle, 0), colliders[0]),
                    "Runtime detection mode update accepted");
        };
        step(world);
        return world.body_state(handle)->position.x;
    };
    require(run(false, false, false, false) > 90, "Discrete reference crosses dynamic target");
    require(run(true, true, false, false) < 60, "Registration enables continuous detection");
    require(run(false, true, false, false) < 60, "Runtime update enables continuous detection");
    require(run(true, false, false, false) > 90, "Runtime update disables continuous detection");
    require(run(true, false, true, false) < 60, "Explicit bullet survives collider mode change");
    require(run(true, false, false, true) < 60,
            "Other continuous collider preserves CCD even when that collider is disabled");
}

int main()
{
    runtime_continuous_detection();
    {
        Tiles wall;
        wall.width = 3;
        PhysicsWorld world;
        world.set_tile_world(wall);
        wall.empty_column = 0;
        world.update_tiles({0, 0}, {0, 0});
        Probe actor;
        actor.set_position({70, 100});
        actor.definition.velocity = {150, 0};
        actor.add(world);
        step(world, 60);
        require(actor.position().x < 81, "Removing a neighbor exposes a blocking side face");
    }
    {
        Tiles floor;
        floor.width = 20;
        floor.cell.material.friction = 0;
        PhysicsWorldConfig config;
        config.gravity = {0, 1000};
        PhysicsWorld world(config);
        world.set_tile_world(floor);
        Probe walker;
        walker.set_position({20, 70});
        walker.collider.material.friction = 0;
        auto handle = walker.add(world);
        step(world, 120);
        walker.tick = [&] {
            world.set_velocity(handle, {150, world.body_state(handle)->velocity.y});
        };
        step(world, 360);
        require(walker.position().x > 910,
                "Walking crosses tile seams without catching a side face");
        walker.tick = [&] {
            world.set_velocity(handle, {-150, world.body_state(handle)->velocity.y});
        };
        step(world, 360);
        require(walker.position().x < 30, "Walking crosses tile seams in reverse");
        require(!world.request_pass_through(world.collider_id(handle, 0),
                                            CollisionTarget::from_tile({0, 0})),
                "Solid terrain rejects drop-through requests");
    }
    {
        Tiles floor;
        floor.width = 3;
        floor.cell.type = TileCollisionType::OneWay;
        floor.cell.one_way = OneWayCollision{PassThroughDirection::Up, 1};
        PhysicsWorldConfig config;
        config.gravity = {0, 1000};
        PhysicsWorld world(config);
        world.set_tile_world(floor);
        Probe actor;
        actor.set_position({90, 60});
        auto handle = actor.add(world);
        step(world, 300);
        require(world.contact_state(handle).grounded && !world.body_state(handle)->awake,
                "Actor rests asleep across a one-way tile seam");
        std::vector<CollisionContact> contacts;
        auto target = CollisionTarget::from_collider(world.collider_id(handle, 0));
        world.collect_contacts(target, contacts);
        require(contacts.size() == 2, "Seam has two supporting cells");
        for (const auto &contact : contacts)
            require(world.request_pass_through(target.collider, contact.pair.first == target
                                                                    ? contact.pair.second
                                                                    : contact.pair.first),
                    "Every supporting one-way cell accepts drop-through");
        step(world, 45);
        require(actor.position().y > 140, "Sleeping actor drops through all seam supports");
        world.teleport_object(handle, {90, 50});
        world.set_velocity(handle, {});
        step(world, 180);
        require(world.contact_state(handle).grounded, "Cleared drop request permits landing again");
    }
    for (int fps : {30, 60, 120, 144})
    {
        Probe o;
        o.definition.mass_policy = MassPolicy::ExplicitMass;
        o.definition.mass = 1;
        PhysicsWorld w;
        auto h = o.add(w);
        int ticks = 0;
        o.tick = [&] {
            ++ticks;
            w.apply_force(h, {60, 0});
        };
        for (int i = 0; i < fps; ++i)
            w.advance(1.0 / fps);
        require(ticks == 60 && near(w.body_state(h)->velocity.x, 60, 0.01f),
                "Force and callback count independent of render cadence");
    }
    Tiles tiles;
    Probe o;
    o.set_position({20, 0});
    PhysicsWorldConfig c;
    c.gravity = {0, 1000};
    PhysicsWorld w(c);
    auto h = o.add(w);
    w.set_tile_world(tiles);
    step(w, 120);
    require(w.contact_state(h).grounded, "Tile grounds actor");
    tiles.cell.type = TileCollisionType::Empty;
    w.update_tiles({0, 0}, {0, 0});
    step(w, 30);
    require(o.position().y > 150, "Dirty tile update removes collision");
    Tiles platform;
    platform.cell.type = TileCollisionType::OneWay;
    platform.cell.one_way = OneWayCollision{PassThroughDirection::Up, 1};
    Probe jumper;
    jumper.set_position({20, 140});
    jumper.definition.velocity = {0, -500};
    PhysicsWorld pw(c);
    auto j = jumper.add(pw);
    pw.set_tile_world(platform);
    step(pw, 20);
    require(jumper.position().y < 100, "Jump passes upward through one-way");
    step(pw, 100);
    require(pw.contact_state(j).grounded, "Fall lands on one-way");
    require(pw.request_pass_through(pw.collider_id(j, 0), CollisionTarget::from_tile({0, 0})),
            "Drop request accepted");
    step(pw, 45);
    require(jumper.position().y > 140, "Drop passes platform");

    require(run_one_way_case(PassThroughDirection::Up, {100, 140}, {0, -300}).y < 80,
            "Up one-way allows upward passage");
    require(run_one_way_case(PassThroughDirection::Up, {100, 60}, {0, 300}).y < 100,
            "Up one-way blocks downward passage");
    require(run_one_way_case(PassThroughDirection::Down, {100, 60}, {0, 300}).y > 120,
            "Down one-way allows downward passage");
    require(run_one_way_case(PassThroughDirection::Down, {100, 140}, {0, -300}).y > 100,
            "Down one-way blocks upward passage");
    require(run_one_way_case(PassThroughDirection::Left, {140, 100}, {-300, 0}).x < 80,
            "Left one-way allows leftward passage");
    require(run_one_way_case(PassThroughDirection::Left, {60, 100}, {300, 0}).x < 100,
            "Left one-way blocks rightward passage");
    require(run_one_way_case(PassThroughDirection::Right, {60, 100}, {300, 0}).x > 120,
            "Right one-way allows rightward passage");
    require(run_one_way_case(PassThroughDirection::Right, {140, 100}, {-300, 0}).x > 100,
            "Right one-way blocks leftward passage");

    constexpr auto up_or_left = PassThroughDirection::Up | PassThroughDirection::Left;
    require(run_one_way_case(up_or_left, {100, 140}, {0, -300}).y < 80 &&
                run_one_way_case(up_or_left, {140, 100}, {-300, 0}).x < 80,
            "Combined one-way directions allow every configured passage");
    require(run_one_way_case(up_or_left, {100, 60}, {0, 300}).y < 100 &&
                run_one_way_case(up_or_left, {60, 100}, {300, 0}).x < 100,
            "Combined one-way directions block unconfigured passage");

    // Sensor semantics are discrete; a cast supplies high-speed gameplay detection.
    Probe sensor, moving;
    sensor.collider.response = CollisionResponse::Overlap;
    sensor.set_position({100, 0});
    moving.definition.velocity = {12000, 0};
    moving.collider.filter = {2, 1, 0};
    PhysicsWorld sweeps;
    auto s = sensor.add(sweeps);
    moving.add(sweeps);
    auto hit = sweeps.sweep_aabb({{20, 0, 10, 10}, {200, 0}, {1, 1, 0}});
    require(hit && hit->target == CollisionTarget::from_collider(sweeps.collider_id(s, 0)),
            "Explicit cast catches thin sensor");
    std::cout << "physics regression tests passed\n";
}
