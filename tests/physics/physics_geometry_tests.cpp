#include "engine/physics/collision/world_shape.h"
#include "physics_test_support.h"
#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace
{
ConvexPolygonShape polygon(float scale = 1)
{
    ConvexPolygonShape p;
    p.vertex_count = 4;
    p.local_vertices[0] = {-10 * scale, -10 * scale};
    p.local_vertices[1] = {10 * scale, -10 * scale};
    p.local_vertices[2] = {10 * scale, 10 * scale};
    p.local_vertices[3] = {-10 * scale, 10 * scale};
    return p;
}
std::array<ColliderShape, 4> shapes(float s = 1)
{
    return {BoxShape{{0, 0}, {10 * s, 10 * s}, 0.2f}, CircleShape{{0, 0}, 10 * s},
            CapsuleShape{{0, -5 * s}, {0, 5 * s}, 5 * s}, polygon(s)};
}
void matrix()
{
    for (float scale : {0.5f, 1.f, 2.f})
        for (const auto &target : shapes(scale))
            for (const auto &query : shapes(scale))
            {
                Probe object;
                object.collider.shape = target;
                object.definition.type = BodyType::Static;
                object.set_position({100 * scale, 0});
                PhysicsWorld world({.units_per_meter = 100 * scale});
                auto h = object.add(world);
                require(h.is_valid(), "All target geometries register");
                ShapeCastQuery q{query, {{0, 0}, 0.35f}, {200 * scale, 0}};
                std::vector<CollisionQueryHit> hits;
                world.shape_cast_all(q, hits);
                auto nearest = world.shape_cast(q);
                require(hits.size() == 1 && nearest && nearest->target == hits[0].target &&
                            near(nearest->fraction, hits[0].fraction) && !nearest->initial_overlap &&
                            nearest->fraction > 0.3f && nearest->fraction < 0.5f,
                        "4x4 casts hit the actual geometry");
                require(near(nearest->distance / scale, 200 * nearest->fraction), "Cast distance respects units");
                std::vector<CollisionOverlapQueryHit> overlaps;
                world.overlap_shape({query, {{100 * scale, 0}, 0.35f}}, overlaps);
                require(overlaps.size() == 1, "4x4 rotated overlaps hit");
                q.pose.position = {100 * scale, 0};
                q.displacement = {};
                nearest = world.shape_cast(q);
                require(nearest && nearest->initial_overlap && nearest->fraction == 0 && nearest->distance == 0 &&
                            nearest->point == q.pose.position && nearest->normal == Vector2{},
                        "Zero casts report initial overlap");
                auto ray = world.raycast({{100 * scale, 0}, {1, 0}, 100 * scale});
                require(ray && ray->initial_overlap && ray->normal == Vector2{}, "Rays report initial overlap");
                world.set_debug_capture(PhysicsDebugCapture::Shapes);
                world.set_transform(h, {{100 * scale, 0}, 0.5f});
                step(world);
                require(world.debug_snapshot().shapes.size() == 1, "Each shape is captured with a rotated pose");
            }
}
void invalid_and_updates()
{
    Probe object;
    PhysicsWorld world;
    const auto h = object.add(world);
    const auto id = world.collider_id(h, 0);
    auto invalid = polygon();
    invalid.local_vertices[2] = {0, 0};
    std::vector<ColliderShape> bad = {BoxShape{{}, {-1, 1}}, CircleShape{{}, 0}, CapsuleShape{{}, {1, 1}, -1},
                                      ConvexPolygonShape{}, invalid};
    invalid = polygon();
    invalid.local_vertices[2] = {0, -5};
    bad.push_back(invalid);
    invalid = polygon();
    invalid.vertex_count = 9;
    bad.push_back(invalid);
    bad.push_back(BoxShape{{}, {1e-20f, 1e-20f}});
    bad.push_back(CircleShape{{}, 1e30f});
    invalid = polygon();
    invalid.local_vertices[2] = invalid.local_vertices[1];
    bad.push_back(invalid);
    invalid = polygon();
    std::swap(invalid.local_vertices[1], invalid.local_vertices[2]);
    bad.push_back(invalid);
    invalid = polygon();
    invalid.local_vertices[1].x = std::numeric_limits<float>::quiet_NaN();
    bad.push_back(invalid);
    for (const auto &shape : bad)
    {
        Collider c = object.collider;
        c.shape = shape;
        require(!world.update_collider(id, c) && world.collider_id(h, 0) == id,
                "Invalid replacement preserves collider ID");
        std::vector<CollisionOverlapQueryHit> hits(1);
        world.overlap_shape({shape}, hits);
        require(hits.empty(), "Invalid overlap clears output");
        std::vector<CollisionQueryHit> casts(1);
        world.shape_cast_all({shape, {}, Vector2{100, 0}}, casts);
        require(casts.empty(), "Invalid casts clear output");
        Probe rejected;
        rejected.collider = c;
        require(!rejected.add(world).is_valid(), "Invalid shape registration is rejected");
    }
    require(world.raycast({{-20, 10}, {1, 0}, 60}).has_value(), "Rejected edits preserve original geometry");
    auto reverse = polygon();
    std::reverse(reverse.local_vertices.begin(), reverse.local_vertices.begin() + 4);
    object.collider.shape = reverse;
    require(world.update_collider(id, object.collider), "Reverse winding accepted");
    auto octagon = ConvexPolygonShape{};
    octagon.vertex_count = 8;
    for (int i = 0; i < 8; ++i)
    {
        float a = i * std::numbers::pi_v<float> / 4;
        octagon.local_vertices[i] = {20 * std::cos(a), 20 * std::sin(a)};
    }
    object.collider.shape = octagon;
    require(world.update_collider(id, object.collider), "Eight vertices supported");
    world.set_debug_capture(PhysicsDebugCapture::Shapes);
    step(world);
    require(std::get<WorldPolygon>(world.debug_snapshot().shapes[0].current).vertex_count == 8,
            "Debug capture preserves all polygon vertices");
    object.collider.shape = CapsuleShape{{5, 5}, {5, 5}, 10};
    require(world.update_collider(id, object.collider), "Coincident capsule endpoints are a circle");
    auto hit = world.raycast({{-20, 5}, {1, 0}, 100});
    require(hit && near(hit->distance, 15, .2f), "Degenerate capsule uses circle geometry");
}
void filtering_and_order()
{
    Probe first, second, sensor;
    first.definition.type = second.definition.type = sensor.definition.type = BodyType::Static;
    first.collider.shape = CircleShape{{}, 10};
    second.collider.shape = CircleShape{{}, 10};
    first.set_position({50, 0});
    second.set_position({50, 0});
    sensor.set_position({100, -10});
    sensor.collider.response = CollisionResponse::Overlap;
    PhysicsWorld world;
    std::array<Collider, 2> cs = {first.collider, first.collider};
    auto a = world.register_object(first, first.definition, cs);
    auto b = second.add(world);
    auto c = sensor.add(world);
    std::vector<CollisionQueryHit> hits;
    ShapeCastQuery q{CircleShape{{}, 1}, {}, {150, 0}};
    world.shape_cast_all(q, hits);
    require(hits.size() == 4 && hits[0].target < hits[1].target && hits[1].target < hits[2].target,
            "Equal-distance hits sort by target and retain separate colliders");
    q.filter.ignored_object = a;
    world.shape_cast_all(q, hits);
    require(hits.size() == 2, "Ignoring an object excludes all its colliders");
    q.filter.include_sensors = false;
    world.shape_cast_all(q, hits);
    require(hits.size() == 1 && hits[0].target == CollisionTarget::from_collider(world.collider_id(b, 0)),
            "Sensor exclusion retains blocking geometry");
    q.filter.collision.mask = 0;
    world.shape_cast_all(q, hits);
    require(hits.empty(), "Masks filter queries");
    q.filter.collision = {1, 0, 4};
    second.collider.filter = {1, 0, 4};
    world.update_collider(world.collider_id(b, 0), second.collider);
    require(world.shape_cast(q).has_value(), "Positive group overrides masks");
    q.filter.collision.group = -4;
    second.collider.filter.group = -4;
    world.update_collider(world.collider_id(b, 0), second.collider);
    require(!world.shape_cast(q), "Negative group excludes queries");
    world.set_body_enabled(c, false);
    q.filter = {};
    q.filter.ignored_object = a;
    require(!world.shape_cast(q), "Disabled shapes are excluded");
}
void contacts_mass_and_ccd()
{
    std::array<float, 4> masses{}, inertias{};
    for (float scale : {0.5f, 1.f, 2.f})
        for (const auto &shape : shapes(scale))
        {
            Probe floor, falling;
            floor.definition.type = BodyType::Static;
            floor.collider.shape = BoxShape{{0, 100 * scale}, {2000 * scale, 5 * scale}};
            falling.collider.shape = shape;
            falling.definition.fixed_rotation = false;
            falling.definition.bullet = true;
            falling.definition.velocity = {0, 12000 * scale};
            PhysicsWorld world({.units_per_meter = 100 * scale, .gravity = {0, 980 * scale}});
            floor.add(world);
            auto h = falling.add(world);
            auto state = world.body_state(h);
            require(state && state->mass > 0 && state->rotational_inertia > 0, "All shapes have mass and inertia");
            const float mass = state->mass, inertia = state->rotational_inertia;
            if (scale == .5f)
            {
                masses[shape.index()] = mass;
                inertias[shape.index()] = inertia / (scale * scale);
            }
            require(near(mass, masses[shape.index()], .001f) &&
                        near(inertia / (scale * scale), inertias[shape.index()], .001f),
                    "All four shape masses and normalized inertias agree across world scales");
            require(world.apply_impulse(h, {0, scale}), "New shapes accept impulses");
            require(world.body_state(h)->velocity.y > 12000 * scale, "Impulse changes velocity");
            step(world, 90);
            state = world.body_state(h);
            if (!(state->position.y < 105 * scale && world.contact_state(h).grounded))
                std::cerr << "CCD shape=" << shape.index() << " scale=" << scale << " position=" << state->position.x
                          << "," << state->position.y << " grounded=" << world.contact_state(h).grounded << "\n";
            require(state->position.y < 105 * scale && world.contact_state(h).grounded,
                    "CCD shape lands on a thin floor");
            require(near(state->mass, mass) && near(state->rotational_inertia, inertia), "Simulation preserves mass");
        }
    for (const auto &a : shapes())
        for (const auto &b : shapes())
        {
            Probe sensor, actor;
            Events events;
            sensor.definition.type = BodyType::Static;
            sensor.collider.shape = a;
            sensor.collider.response = CollisionResponse::Overlap;
            actor.collider.shape = b;
            PhysicsWorld world;
            world.add_listener(events);
            sensor.add(world);
            auto h = actor.add(world);
            step(world, 2);
            require(events.count(CollisionEventPhase::Begin) == 1 && events.count(CollisionEventPhase::Stay) >= 1,
                    "All sensor pairs generate Begin and Stay");
            world.teleport_object(h, {100, 0});
            step(world);
            require(events.count(CollisionEventPhase::End) == 1, "All sensor pairs generate End");
        }
}
class Tile final : public ITileCollisionWorld
{
  public:
    Vector2 world_origin() const noexcept override
    {
        return {100, 0};
    }
    Vector2 tile_size() const noexcept override
    {
        return {20, 20};
    }
    int columns() const noexcept override
    {
        return 1;
    }
    int rows() const noexcept override
    {
        return 1;
    }
    TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override
    {
        return TileOutOfBoundsPolicy::Empty;
    }
    TileCollisionCell cell_at(TileCoordinate) const noexcept override
    {
        return {.type = TileCollisionType::Block};
    }
};
void tiles_and_offsets()
{
    Tile tile;
    PhysicsWorld world;
    require(world.set_tile_world(tile), "Tile binds");
    for (const auto &shape : shapes())
    {
        auto hit = world.shape_cast({shape, {{0, 10}, 0.2f}, {200, 0}});
        require(hit && hit->target.kind == CollisionTargetKind::Tile, "Every cast shape can hit a Tile");
        std::vector<CollisionOverlapQueryHit> hits;
        world.overlap_shape({shape, {{110, 10}, 0.2f}}, hits);
        require(hits.size() == 1 && hits[0].target.kind == CollisionTargetKind::Tile,
                "Every overlap shape can select a Tile");
    }
    require(world.clear_tile_world(tile), "Tile detaches");
    Probe target;
    target.definition.type = BodyType::Static;
    target.collider.shape = BoxShape{{20, 0}, {30, 2}, 1.57079632679f};
    auto h = target.add(world);
    std::vector<CollisionOverlapQueryHit> hits;
    world.overlap_shape({CircleShape{{}, 1}, {{20, 20}, 0}}, hits);
    require(hits.size() == 1, "Local box rotation is used by overlap queries");
    world.overlap_shape({CircleShape{{}, 1}, {{40, 0}, 0}}, hits);
    require(hits.empty(), "Local box rotation does not query an unrotated rectangle");
    require(world.unregister_object(h), "Box unregisters");
    auto p = polygon();
    for (auto &v : p.local_vertices)
        v += Vector2{25, 0};
    target.collider.shape = p;
    h = target.add(world);
    require(world.raycast({{0, 0}, {1, 0}, 100})->distance > 14, "Polygon local offset is honored");
    world.set_transform(h, {{0, 0}, 1.57079632679f});
    auto hit = world.raycast({{0, 0}, {0, 1}, 100});
    require(hit && hit->distance > 14 && hit->distance < 16, "Body pose rotates polygon offsets");
}
void joints()
{
    for (float scale : {0.5f, 1.f, 2.f})
    {
        Probe anchor, bob;
        anchor.definition.type = BodyType::Static;
        bob.set_position({100 * scale, 0});
        bob.definition.fixed_rotation = false;
        PhysicsWorld world({.units_per_meter = 100 * scale});
        auto a = anchor.add(world), b = bob.add(world);
        DistanceJointDefinition d{a, b, {}, {}, {.length = 100 * scale}};
        auto h = world.create_distance_joint(d);
        require(h.is_valid(), "Distance joint created");
        auto s = d.settings;
        s.length = 80 * scale;
        s.enable_spring = true;
        s.frequency_hz = 4;
        s.damping_ratio = .8f;
        s.enable_limit = true;
        s.min_length = 60 * scale;
        s.max_length = 110 * scale;
        s.enable_motor = true;
        s.motor_speed = -10 * scale;
        s.max_motor_force = 1000 * scale;
        s.collide_connected = true;
        require(world.update_distance_joint(h, s), "Distance settings update in place");
        step(world, 3);
        auto ds = std::get<DistanceJointState>(world.joint_state(h)->details);
        require(near(ds.settings.length / scale, 80) && ds.settings.enable_motor && ds.settings.collide_connected &&
                    std::isfinite(ds.speed) && std::isfinite(ds.motor_force),
                "Distance state exposes settings and measurements");
        require(!world.update_revolute_joint(h, {}), "Wrong joint type is rejected");
        auto invalid = s;
        invalid.min_length = 120 * scale;
        require(!world.update_distance_joint(h, invalid), "Invalid limits leave joint usable");
        require(world.destroy_joint(h) && !world.joint_state(h), "Destroy invalidates state");
        world.teleport_object(b, {0, 0}, TeleportVelocityMode::Clear);
        RevoluteJointDefinition rd{a, b};
        rd.settings.enable_motor = true;
        rd.settings.motor_speed = 1;
        rd.settings.max_motor_torque = 100000 * scale * scale;
        auto r = world.create_revolute_joint(rd);
        require(r.is_valid(), "Revolute joint created");
        step(world, 20);
        require(world.body_state(b)->angle > 0.1f, "Motor turns forward");
        auto rs = rd.settings;
        rs.motor_speed = -2;
        require(world.update_revolute_joint(r, rs), "Reverse motor accepted");
        const float previous = world.body_state(b)->angle;
        step(world, 20);
        require(world.body_state(b)->angle < previous, "Runtime reversal changes physical behavior");
        rs.enable_motor = false;
        rs.enable_spring = true;
        rs.target_angle = .2f;
        rs.frequency_hz = 5;
        rs.damping_ratio = .8f;
        rs.enable_limit = true;
        rs.lower_angle = -.5f;
        rs.upper_angle = .5f;
        world.set_awake(b, false);
        require(world.update_revolute_joint(r, rs) && world.body_state(b)->awake, "Updating a joint wakes its body");
        step(world, 90);
        auto state = std::get<RevoluteJointState>(world.joint_state(r)->details);
        require(near(state.angle, .2f, .05f) && std::isfinite(state.angular_velocity) &&
                    std::isfinite(state.motor_torque),
                "Revolute spring reaches the new target angle");
        rs.upper_angle = 4;
        require(!world.update_revolute_joint(r, rs), "Out of native angle range is rejected");
        world.unregister_object(b);
        require(!world.joint_state(r) && !world.update_revolute_joint(r, {}), "Removing a body invalidates its joints");
    }
}
void deferred_joints()
{
    Probe anchor, bob;
    anchor.definition.type = BodyType::Static;
    bob.set_position({100, 0});
    PhysicsWorld world;
    auto a = anchor.add(world), b = bob.add(world);
    JointHandle h;
    bool once = false;
    bob.tick = [&] {
        if (once)
            return;
        once = true;
        h = world.create_distance_joint({a, b, {}, {}, {.length = 100}});
        DistanceJointSettings s{.length = 70, .enable_spring = true};
        require(h.is_valid() && world.update_distance_joint(h, s), "Pending creation can be updated");
        s.length = 30;
    };
    step(world);
    require(near(std::get<DistanceJointState>(world.joint_state(h)->details).settings.length, 70),
            "Deferred settings are copied by value and applied FIFO");
    once = false;
    bob.tick = [&] {
        if (once)
            return;
        once = true;
        require(world.update_distance_joint(h, {.length = 60}), "Deferred update accepted");
        require(world.destroy_joint(h), "Deferred destruction accepted");
    };
    step(world);
    require(!world.joint_state(h), "Update followed by destruction leaves no joint");
    once = false;
    bob.tick = [&] {
        if (once)
            return;
        once = true;
        h = world.create_distance_joint({a, b, {}, {}, {.length = 100}});
        world.update_distance_joint(h, {.length = 80});
        world.destroy_joint(h);
    };
    step(world);
    require(!world.joint_state(h), "Pending create-update-destroy leaves no native joint");
    once = false;
    bob.tick = [&] {
        if (once)
            return;
        once = true;
        h = world.create_distance_joint({a, b, {}, {}, {.length = 100}});
        world.update_distance_joint(h, {.length = 50});
        throw std::runtime_error("abort callback");
    };
    bool caught = false;
    try
    {
        step(world);
    }
    catch (const std::runtime_error &)
    {
        caught = true;
    }
    require(caught, "Callback failure propagates");
    bob.tick = {};
    step(world);
    require(!world.joint_state(h) && world.last_step_stats().joints == 0,
            "Aborted pending joint is removed after callback failure");
    require(world.registered_object_count() == 2, "Callback failure preserves existing owners");
}
} // namespace
int main()
{
    matrix();
    invalid_and_updates();
    filtering_and_order();
    contacts_mass_and_ccd();
    tiles_and_offsets();
    joints();
    deferred_joints();
}
