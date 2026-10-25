#include "physics_test_support.h"
#include <array>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>
namespace
{
PhysicsWorldConfig config(float scale = 1)
{
    PhysicsWorldConfig c;
    c.gravity = {};
    c.units_per_meter = 100 * scale;
    return c;
}
std::array<ColliderShape, 4> sources(float s = 1)
{
    ConvexPolygonShape p;
    p.vertex_count = 3;
    p.local_vertices[0] = {-8 * s, -8 * s};
    p.local_vertices[1] = {8 * s, -8 * s};
    p.local_vertices[2] = {0, 8 * s};
    return {BoxShape{{}, {8 * s, 8 * s}, .2f}, CircleShape{{}, 8 * s}, CapsuleShape{{-4 * s, 0}, {4 * s, 0}, 4 * s}, p};
}
void terrain()
{
    for (float s : {.5f, 1.f, 2.f})
    {
        PhysicsWorld w(config(s));
        Probe ground;
        ground.definition.type = BodyType::Static;
        ground.collider.shape = SegmentShape{{-100 * s, 0}, {100 * s, 0}};
        auto a = ground.add(w);
        require(a.is_valid(), "Segment registers");
        auto id = w.collider_id(a, 0);
        for (const auto &shape : sources(s))
        {
            auto hit = w.shape_cast({shape, {{0, -50 * s}, .3f}, {0, 100 * s}});
            require(hit && hit->target.collider == id, "All area shapes cast onto Segment");
            require(w.shape_cast({shape, {{0, 50 * s}, .3f}, {0, -100 * s}}), "Segment geometric cast is two sided");
            std::vector<CollisionOverlapQueryHit> hits;
            w.overlap_shape({shape, {}}, hits);
            require(hits.size() == 1, "All area shapes overlap Segment");
        }
        require(w.segment_cast({{0, -50 * s}, {0, 50 * s}}) && w.segment_cast({{0, 50 * s}, {0, -50 * s}}),
                "Segment rays are double sided");
        require(!w.shape_cast({SegmentShape{{0, 0}, {10 * s, 0}}, {}, {100 * s, 0}}),
                "Segment is not a shape cast source");
        Collider bad = ground.collider;
        bad.shape = SegmentShape{{}, {.001f * s, 0}};
        require(!w.update_collider(id, bad) && w.contains_collider(id), "Degenerate update leaves stable Segment");
        bad.shape = SegmentShape{{-80 * s, 0}, {80 * s, 0}};
        require(w.update_collider(id, bad) && w.collider_id(a, 0) == id, "Segment update preserves ID");
        CollisionQueryFilter f;
        f.ignored_object = a;
        require(!w.segment_cast({{0, -50 * s}, {0, 50 * s}, f}), "Segment owner can be ignored");
        Probe invalid;
        invalid.collider.shape = ground.collider.shape;
        require(!invalid.add(w).is_valid(), "Dynamic Segment is rejected");
        ChainDefinition d;
        d.local_vertices = {{-100 * s, 100 * s}, {0, 100 * s}, {100 * s, 100 * s}};
        d.tag = "terrain";
        auto chain = w.create_chain(a, d);
        require(chain.is_valid() && w.chain_state(chain)->segment_count == 2,
                "Open chain includes first and last actual edges");
        d.local_vertices.clear();
        d.tag = "changed";
        for (size_t i = 0; i < 2; ++i)
        {
            auto child = w.chain_collider(chain, i);
            auto info = w.chain_segment_info(child);
            require(info && info->chain == chain && info->owner == a && info->index == i,
                    "Chain child identity can be reversed");
            require(!w.update_collider(child, {}) && !w.set_collider_enabled(child, false),
                    "Chain children are immutable");
            float x = (i == 0 ? -50.f : 50.f) * s;
            auto ray = w.segment_cast({{x, 50 * s}, {x, 150 * s}});
            require(ray && ray->target.collider == child, "Open chain actual endpoint edge has front ray hit");
            require(!w.segment_cast({{x, 150 * s}, {x, 110 * s}}), "Back ray misses chain");
            for (const auto &shape : sources(s))
            {
                auto h = w.shape_cast({shape, {{x, 150 * s}, 0}, {0, -70 * s}});
                require(h && h->target.collider == child, "Back shape cast uses geometric chain semantics");
                std::vector<CollisionOverlapQueryHit> hits;
                w.overlap_shape({shape, {{x, 100 * s}, 0}}, hits);
                require(hits.size() == 1 && hits[0].target.collider == child, "Area shapes overlap chain");
            }
        }
        require(w.collider_id(a, 1) == InvalidColliderId && w.registered_collider_count() == 3,
                "Declared collider indexing excludes chain children");
        ShapeCastQuery q{CircleShape{{}, 8 * s}, {{0, 50 * s}, 0}, {0, 100 * s}};
        std::vector<CollisionQueryHit> all;
        w.shape_cast_all(q, all);
        require(all.size() == 2 && all[0].target.collider < all[1].target.collider &&
                    w.shape_cast(q)->target == all.front().target,
                "Chain seam hits sort and deduplicate by target");
        q.filter.ignored_object = a;
        w.shape_cast_all(q, all);
        require(all.empty(), "Ignore owner excludes all chain edges");
        w.set_debug_capture(PhysicsDebugCapture::Shapes);
        step(w);
        require(w.debug_snapshot().shapes.size() == 3 && w.last_step_stats().chains == 1,
                "Chain contributes to debug and statistics");
        w.set_body_enabled(a, false);
        require(!w.segment_cast({{-50 * s, 50 * s}, {-50 * s, 150 * s}}), "Disable owner removes chain query proxies");
        w.set_body_enabled(a, true);
        auto child = w.chain_collider(chain, 0);
        require(w.destroy_chain(chain) && !w.chain_state(chain) && !w.contains_collider(child) &&
                    !w.chain_segment_info(child),
                "Destroy retires chain and identities");
        ChainDefinition closed;
        closed.loop = true;
        closed.local_vertices = {{200 * s, 0}, {300 * s, 0}, {300 * s, 100 * s}, {200 * s, 100 * s}};
        auto loop = w.create_chain(a, closed);
        require(loop.is_valid() && w.chain_state(loop)->segment_count == 4, "Closed chain owns closing edge");
        require(w.segment_cast({{150 * s, 50 * s}, {250 * s, 50 * s}}), "Closed last edge participates");
        w.unregister_object(a);
        require(!w.chain_state(loop) && w.registered_collider_count() == 0, "Removing owner retires all children");
    }
    PhysicsWorld w(config());
    Probe owner;
    owner.definition.type = BodyType::Kinematic;
    auto a = owner.add(w);
    ChainDefinition d;
    d.local_vertices = {{0, 0}, {100, 0}};
    d.ghost_before = Vector2{-80, -20};
    d.ghost_after = Vector2{180, 20};
    auto h = w.create_chain(a, d);
    require(h.is_valid(), "Explicit ghost geometry accepted");
    w.set_transform(a, {{200, 200}, std::numbers::pi_v<float> / 2});
    require(w.segment_cast({{250, 250}, {150, 250}}), "Kinematic rotated chain follows body");
    auto reject = [&](std::vector<Vector2> v, bool loop = false) {
        ChainDefinition c;
        c.local_vertices = std::move(v);
        c.loop = loop;
        require(!w.create_chain(a, c).is_valid(), "Invalid chain rejected before native API");
    };
    reject({{0, 0}});
    reject({{0, 0}, {.01f, 0}});
    reject({{0, 0}, {100, 0}, {0, 0}});
    reject({{0, 0}, {100, 100}, {0, 100}, {100, 0}});
    reject({{0, 0}, {100, 0}, {100, 100}}, true);
    reject({{0, 0}, {std::numeric_limits<float>::quiet_NaN(), 0}});
    d.ghost_before = Vector2{0, 0};
    require(!w.create_chain(a, d).is_valid(), "Degenerate ghost edge rejected");
    w.reset();
    require(!w.chain_state(h) && !w.chain_collider(h, 0), "Reset invalidates chain");
}
void contacts()
{
    for (bool chain : {false, true})
        for (bool sensor : {false, true})
            for (bool reverse : {false, true})
            {
                auto c = config();
                PhysicsWorld w(c);
                Probe ground, moving;
                ground.definition.type = BodyType::Static;
                ground.collider.shape = SegmentShape{{-100, 0}, {100, 0}};
                auto a = ground.add(w);
                if (chain)
                {
                    w.set_collider_enabled(w.collider_id(a, 0), false);
                    ChainDefinition d;
                    d.local_vertices =
                        reverse ? std::vector<Vector2>{{100, 0}, {-100, 0}} : std::vector<Vector2>{{-100, 0}, {100, 0}};
                    require(w.create_chain(a, d).is_valid(), "Chain terrain registers");
                }
                moving.collider.shape = CircleShape{{}, 8};
                moving.collider.response = sensor ? CollisionResponse::Overlap : CollisionResponse::Block;
                moving.set_position({0, reverse ? 30.f : -30.f});
                moving.definition.velocity = {0, reverse ? -30.f : 30.f};
                auto b = moving.add(w);
                Events events;
                w.add_listener(events);
                step(w, 100);
                require(events.count(CollisionEventPhase::Begin) > 0,
                        "Segment and Chain produce physical or sensor events");
                if (!sensor)
                {
                    require(std::abs(w.body_state(b)->position.y) > 5,
                            "Solid terrain prevents crossing from its front");
                    require(reverse ? w.contact_state(b).ceiling : w.contact_state(b).grounded,
                            "Edge contacts contribute grounded and ceiling state");
                    require(reverse ? w.contact_state(a).grounded : w.contact_state(a).ceiling,
                            "Terrain owner aggregates its Chain child contacts");
                }
                if (sensor)
                    require(events.count(CollisionEventPhase::End) > 0, "Sensors cross zero thickness edges and leave");
            }
    // Back-side physical movement passes through a chain; geometric casts above still hit it.
    PhysicsWorld w(config());
    Probe owner, b;
    owner.definition.type = BodyType::Static;
    auto a = owner.add(w);
    w.set_collider_enabled(w.collider_id(a, 0), false);
    ChainDefinition d;
    d.local_vertices = {{-100, 0}, {0, 0}, {100, 0}};
    auto chain = w.create_chain(a, d);
    b.collider.shape = CircleShape{{}, 8};
    b.set_position({0, 30});
    b.definition.velocity = {0, -30};
    auto bh = b.add(w);
    step(w, 100);
    require(w.body_state(bh)->position.y < -15, "Back face chain has no physical response");
    // Continuous seam sliding must not snag on internal vertices.
    w.set_transform(bh, {{-80, -8}, 0}, TeleportVelocityMode::Clear);
    w.set_velocity(bh, {25, 0});
    step(w, 240);
    require(w.body_state(bh)->position.x > 10, "Sliding along chain passes seam");
    require(w.destroy_chain(chain), "Live terrain can be destroyed");
    step(w);
}
void deferred()
{
    PhysicsWorld w(config());
    Probe owner;
    owner.definition.type = BodyType::Static;
    auto a = owner.add(w);
    ChainHandle h;
    bool once = false;
    owner.tick = [&] {
        if (once)
            return;
        once = true;
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        h = w.create_chain(a, d);
        d.local_vertices[0] = {500, 500};
        require(h.is_valid() && !w.chain_state(h), "Pending chain waits for safe point");
    };
    step(w);
    require(w.chain_state(h) && w.segment_cast({{50, 50}, {50, 150}}), "Deferred chain owns copied vertices");
    once = false;
    owner.tick = [&] {
        if (once)
            return;
        once = true;
        require(w.destroy_chain(h) && !w.chain_state(h), "Deferred destroy invalidates handle immediately");
    };
    step(w);
    require(w.last_step_stats().chains == 0, "Deferred destroy clears children");
    once = false;
    owner.tick = [&] {
        if (once)
            return;
        once = true;
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        h = w.create_chain(a, d);
        w.unregister_object(a);
    };
    step(w);
    require(!w.chain_state(h) && w.registered_collider_count() == 0,
            "Owner removal before native chain creation is safe");
    a = owner.add(w);
    owner.tick = [&] {
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        h = w.create_chain(a, d);
        throw std::runtime_error("abort");
    };
    try
    {
        step(w);
        require(false, "Callback throws");
    }
    catch (const std::runtime_error &)
    {
    }
    owner.tick = {};
    step(w);
    require(!w.chain_state(h) && w.last_step_stats().chains == 0 && w.registered_collider_count() == 1,
            "Aborted callback rolls back pending chain");
}
void terrain_filters_and_pending_owner()
{
    PhysicsWorld world(config());
    Probe source, pending;
    auto driver = source.add(world);
    PhysicsObjectHandle owner;
    ChainHandle chain;
    bool once = false;
    source.tick = [&] {
        if (once)
            return;
        once = true;
        owner = world.register_object(pending, {.type = BodyType::Static}, {});
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        d.filter = {.category = 0, .mask = 0, .group = 7};
        chain = world.create_chain(owner, d);
    };
    step(world);
    source.tick = {};
    require(world.chain_state(chain), "Pending owner creates before its dependent Chain");
    CollisionQueryFilter filter;
    filter.collision = {.category = 0, .mask = 0, .group = 7};
    filter.include_sensors = false;
    auto hit = world.segment_cast({{50, 50}, {50, 150}, filter});
    require(hit && world.chain_segment_info(hit->target.collider),
            "Positive group queries include otherwise masked Chain children");
    require(!world.segment_cast({{50, 150}, {50, 50}, filter}), "Positive group fallback preserves single-sided rays");
    require(world.shape_cast({CircleShape{{}, 8}, {{50, 150}, 0}, {0, -100}, filter}),
            "Positive group fallback preserves geometric back sweeps");
    filter.collision.group = -7;
    require(!world.segment_cast({{50, 50}, {50, 150}, filter}), "Negative or unmatched groups exclude terrain");
    require(world.unregister_object(owner) && !world.chain_state(chain),
            "Dependent Chain is removed with its pending-created owner");
    once = false;
    source.tick = [&] {
        if (once)
            return;
        once = true;
        owner = world.register_object(pending, {.type = BodyType::Static}, {});
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        chain = world.create_chain(owner, d);
        world.unregister_object(owner);
    };
    step(world);
    source.tick = {};
    require(!world.chain_state(chain) && world.registered_object_count() == 1,
            "Removing pending owner cleans all dependent Chain metadata");
    Probe terrain, sensor;
    terrain.definition.type = BodyType::Static;
    auto a = world.register_object(terrain, terrain.definition, {});
    ChainDefinition d;
    d.local_vertices = {{0, 100}, {100, 100}};
    chain = world.create_chain(a, d);
    sensor.set_position({50, 90});
    sensor.collider.response = CollisionResponse::Overlap;
    sensor.add(world);
    Events events;
    world.add_listener(events);
    step(world);
    require(events.count(CollisionEventPhase::Begin) == 1, "Chain reports a single stable overlap pair");
    auto child = world.chain_collider(chain, 0);
    world.destroy_chain(chain);
    step(world, 2);
    require(events.count(CollisionEventPhase::End) == 1 && !world.contains_collider(child),
            "Destroyed Chain emits one logical End with retired child identity");
    terrain.collider.shape = SegmentShape{{0, 0}, {100, 0}};
    terrain.collider.response = CollisionResponse::Overlap;
    auto edge = world.register_object(pending, {.type = BodyType::Static}, {&terrain.collider, 1});
    filter = {};
    filter.include_sensors = false;
    require(!world.segment_cast({{50, -50}, {50, 50}, filter}), "Segment sensors obey include_sensors");
    filter.include_sensors = true;
    require(world.segment_cast({{50, -50}, {50, 50}, filter}), "Segment sensors can be queried explicitly");
}
void targets()
{
    for (float s : {.5f, 1.f, 2.f})
        for (double dt : {1. / 30, 1. / 60, 1. / 120})
        {
            PhysicsWorld w(config(s));
            Probe platform;
            platform.definition.type = BodyType::Kinematic;
            platform.definition.fixed_rotation = false;
            auto a = platform.add(w);
            require(w.set_target_transform(a, {{s, 0}, .01f}, dt), "Kinematic target accepted");
            auto before = w.body_state(a);
            require(near(before->position.x, 0) && near(before->velocity.x, float(s / dt), .1f),
                    "Target changes velocity without teleport");
            w.step(dt);
            w.finalize_frame(.5);
            auto state = w.body_state(a);
            require(near(state->position.x, s, .02f) && near(state->angle, .01f, .001f),
                    "Target tracks small pose in fixed step");
            require(w.render_pose(a)->position.x < state->position.x, "Target preserves render interpolation");
            require(w.set_target_transform(a, {state->position, state->angle}, dt) &&
                        w.body_state(a)->velocity == Vector2{} && w.body_state(a)->angular_velocity == 0,
                    "Same target clears old velocity");
            w.set_velocity(a, {10 * s, 0});
            w.step(dt);
            const auto stopped_submitting = w.body_state(a)->position.x;
            w.step(dt);
            require(w.body_state(a)->position.x > stopped_submitting, "Omitting targets retains current velocity");
            w.set_transform(a, {state->position, state->angle}, TeleportVelocityMode::Clear);
            w.set_velocity(a, {20, 0});
            w.set_target_transform(a, {{state->position.x + .00001f * s, 0}, state->angle}, dt);
            require(w.body_state(a)->velocity == Vector2{}, "Sleepy target clears previous command");
            w.set_transform(a, {{0, 0}, 3.0f}, TeleportVelocityMode::Clear);
            w.set_target_transform(a, {{0, 0}, -3.0f}, dt);
            require(w.body_state(a)->angular_velocity > 0 && w.body_state(a)->angular_velocity < 40,
                    "Rotation crosses pi by shortest path");
            auto previous = w.body_state(a);
            require(!w.set_target_transform(a, {}, 0) && !w.set_target_transform(a, {}, -1) &&
                        !w.set_target_transform(a, {}, std::numeric_limits<double>::infinity()) &&
                        !w.set_target_transform(a, {{}, std::numeric_limits<float>::quiet_NaN()}, dt) &&
                        w.body_state(a)->angular_velocity == previous->angular_velocity,
                    "Invalid target preserves state");
        }
    PhysicsWorld w(config());
    Probe p;
    p.definition.type = BodyType::Kinematic;
    auto a = p.add(w);
    w.set_target_transform(a, {{1, 0}, 1}, 1. / 60);
    require(w.body_state(a)->angular_velocity == 0, "Fixed rotation ignores target angle");
    Probe dynamic;
    auto b = dynamic.add(w);
    require(!w.set_target_transform(b, {}, 1. / 60), "Dynamic bodies reject target control");
    bool once = false;
    p.tick = [&] {
        if (once)
            return;
        once = true;
        PhysicsPose target{{1, 0}, 0};
        w.set_target_transform(a, target, 1. / 60);
        target.position = {1000, 1000};
        w.set_velocity(a, {15, 0});
    };
    step(w);
    require(near(w.body_state(a)->velocity.x, 15), "Later velocity overrides deferred target");
    once = false;
    p.tick = [&] {
        if (once)
            return;
        once = true;
        w.set_velocity(a, {15, 0});
        w.set_transform(a, {{5, 0}, 0});
        w.set_target_transform(a, {{6, 0}, 0}, 1. / 60);
    };
    step(w);
    require(near(w.body_state(a)->position.x, 6, .1f), "Target executes after teleport using current native pose");
    once = false;
    p.tick = [&] {
        if (once)
            return;
        once = true;
        w.set_body_enabled(a, false);
        w.set_target_transform(a, {{100, 0}, 0}, 1. / 60);
    };
    step(w);
    require(!w.body_state(a)->enabled, "Disabled target skipped safely");
}
struct Pair
{
    PhysicsWorld w;
    Probe anchor, bob;
    PhysicsObjectHandle a, b;
    Pair(float scale = 1) : w(config(scale))
    {
        anchor.definition.type = BodyType::Static;
        bob.collider.shape = BoxShape{{}, {10, 10}, 0};
        bob.definition.fixed_rotation = false;
        bob.definition.mass_properties = ExplicitMass{1};
        a = anchor.add(w);
        b = bob.add(w);
    }
};
template <class State, class Definition, class Settings, class Create, class Update>
void lifecycle(Definition d, Settings settings, Create create, Update update)
{
    Pair p;
    d.first = p.a;
    d.second = p.b;
    JointHandle h;
    bool once = false;
    p.bob.tick = [&] {
        if (once)
            return;
        once = true;
        h = (p.w.*create)(d);
        require(h.is_valid() && (p.w.*update)(h, settings), "New joint accepts immediate deferred update");
    };
    step(p.w);
    require(p.w.joint_state(h) && std::holds_alternative<State>(p.w.joint_state(h)->details),
            "New joint reports typed state");
    require(p.w.joint_state(h)->first == p.a && p.w.joint_state(h)->second == p.b,
            "Joint exposes both object identities");
    p.w.set_awake(p.b, false);
    require((p.w.*update)(h, settings) && p.w.body_state(p.b)->awake, "Update wakes connected dynamic body");
    require(!p.w.update_distance_joint(h, {}), "Wrong joint update rejected");
    once = false;
    p.bob.tick = [&] {
        if (once)
            return;
        once = true;
        require((p.w.*update)(h, settings) && p.w.destroy_joint(h), "Update destroy FIFO accepted");
    };
    step(p.w);
    require(!p.w.joint_state(h), "Destroyed joint has no state");
    once = false;
    p.bob.tick = [&] {
        if (once)
            return;
        once = true;
        h = (p.w.*create)(d);
        (p.w.*update)(h, settings);
        p.w.unregister_object(p.b);
    };
    step(p.w);
    require(!p.w.joint_state(h) && !(p.w.*update)(h, settings), "Owner removal invalidates queued joint");
    Pair q;
    d.first = q.a;
    d.second = q.b;
    q.bob.tick = [&] {
        h = (q.w.*create)(d);
        (q.w.*update)(h, settings);
        throw std::runtime_error("abort joint");
    };
    try
    {
        step(q.w);
        require(false, "Callback throws");
    }
    catch (const std::runtime_error &)
    {
    }
    q.bob.tick = {};
    step(q.w);
    require(!q.w.joint_state(h) && q.w.last_step_stats().joints == 0, "Aborted callback cleans pending new joint");
}
void joints()
{
    lifecycle<WeldJointState>(WeldJointDefinition{}, WeldJointSettings{}, &PhysicsWorld::create_weld_joint,
                              &PhysicsWorld::update_weld_joint);
    lifecycle<PrismaticJointState>(PrismaticJointDefinition{}, PrismaticJointSettings{},
                                   &PhysicsWorld::create_prismatic_joint, &PhysicsWorld::update_prismatic_joint);
    lifecycle<WheelJointState>(WheelJointDefinition{}, WheelJointSettings{}, &PhysicsWorld::create_wheel_joint,
                               &PhysicsWorld::update_wheel_joint);
    lifecycle<MouseJointState>(MouseJointDefinition{}, MouseJointSettings{}, &PhysicsWorld::create_mouse_joint,
                               &PhysicsWorld::update_mouse_joint);
    lifecycle<MotorJointState>(MotorJointDefinition{}, MotorJointSettings{}, &PhysicsWorld::create_motor_joint,
                               &PhysicsWorld::update_motor_joint);
    for (float scale : {.5f, 1.f, 2.f})
    {
        Pair p(scale);
        WeldJointDefinition wd{p.a, p.b};
        auto h = p.w.create_weld_joint(wd);
        p.w.set_velocity(p.b, {40, 30});
        p.w.set_angular_velocity(p.b, 3);
        step(p.w, 120);
        auto ws = std::get<WeldJointState>(p.w.joint_state(h)->details);
        require(ws.linear_error.length() < 1 && std::abs(ws.angular_error) < .02f, "Weld maintains position and angle");
        auto settings = wd.settings;
        settings.linear_frequency_hz = 2;
        settings.angular_frequency_hz = 3;
        require(p.w.update_weld_joint(h, settings), "Weld compliance updates");
        p.w.destroy_joint(h);
        PrismaticJointDefinition pd{p.a, p.b};
        pd.local_axis = {scale * 3, 0};
        pd.settings.enable_motor = true;
        pd.settings.motor_speed = 30 * scale;
        pd.settings.max_motor_force = 1000 * scale;
        pd.settings.enable_limit = true;
        pd.settings.lower_translation = -20 * scale;
        pd.settings.upper_translation = 20 * scale;
        h = p.w.create_prismatic_joint(pd);
        step(p.w, 120);
        auto ps = std::get<PrismaticJointState>(p.w.joint_state(h)->details);
        require(ps.translation > 10 * scale && ps.translation < 22 * scale && std::abs(ps.motor_force) <= 1001 * scale,
                "Prismatic motor and limit constrain displacement and output");
        pd.settings.motor_speed = -30 * scale;
        p.w.update_prismatic_joint(h, pd.settings);
        step(p.w, 120);
        require(std::get<PrismaticJointState>(p.w.joint_state(h)->details).translation < -10 * scale,
                "Prismatic motor reverses at runtime");
        pd.settings.enable_motor = false;
        pd.settings.enable_spring = true;
        pd.settings.target_translation = 5 * scale;
        pd.settings.frequency_hz = 5;
        p.w.update_prismatic_joint(h, pd.settings);
        step(p.w, 180);
        require(near(std::get<PrismaticJointState>(p.w.joint_state(h)->details).translation, 5 * scale, 1 * scale),
                "Prismatic spring reaches runtime target");
        p.w.destroy_joint(h);
        p.w.set_transform(p.b, {}, TeleportVelocityMode::Clear);
        WheelJointDefinition wheel{p.a, p.b};
        wheel.local_axis = {0, 2};
        wheel.settings.enable_motor = true;
        wheel.settings.motor_speed = 2;
        wheel.settings.max_motor_torque = 10000 * scale * scale;
        wheel.settings.enable_limit = true;
        wheel.settings.lower_translation = -20 * scale;
        wheel.settings.upper_translation = 20 * scale;
        h = p.w.create_wheel_joint(wheel);
        p.w.apply_impulse(p.b, {30, 20});
        step(p.w, 90);
        auto whs = std::get<WheelJointState>(p.w.joint_state(h)->details);
        require(std::abs(p.w.body_state(p.b)->position.x) < 1 && std::abs(whs.translation) < 22 * scale &&
                    whs.angular_velocity > .5f && std::abs(whs.motor_torque) <= 10001 * scale * scale,
                "Wheel constrains axis and drives rotation");
        wheel.settings.motor_speed = -2;
        p.w.update_wheel_joint(h, wheel.settings);
        step(p.w, 90);
        require(std::get<WheelJointState>(p.w.joint_state(h)->details).angular_velocity < -.5f, "Wheel motor reverses");
        p.w.destroy_joint(h);
        p.w.set_transform(p.b, {}, TeleportVelocityMode::Clear);
        MouseJointDefinition mouse{p.a, p.b};
        h = p.w.create_mouse_joint(mouse);
        mouse.settings.target = {40 * scale, 20 * scale};
        mouse.settings.max_force = 3000 * scale;
        p.w.update_mouse_joint(h, mouse.settings);
        step(p.w, 180);
        auto ms = std::get<MouseJointState>(p.w.joint_state(h)->details);
        require(ms.error.length() < 1 * scale && p.w.joint_state(h)->reaction_force.length() <= 3001 * scale,
                "Mouse follows world target with force limit");
        p.w.destroy_joint(h);
        p.w.set_transform(p.b, {}, TeleportVelocityMode::Clear);
        MotorJointDefinition motor{p.a, p.b};
        motor.settings.linear_offset = {30 * scale, 20 * scale};
        motor.settings.angular_offset = .5f;
        motor.settings.max_force = 100000 * scale;
        motor.settings.max_torque = 100000 * scale * scale;
        motor.settings.correction_factor = .05f;
        h = p.w.create_motor_joint(motor);
        step(p.w, 180);
        auto mot = std::get<MotorJointState>(p.w.joint_state(h)->details);
        require(mot.linear_error.length() < 1 * scale && std::abs(mot.angular_error) < .03f,
                "Motor corrects relative pose");
        motor.settings.linear_offset = {-30 * scale, 0};
        motor.settings.angular_offset = -.5f;
        p.w.update_motor_joint(h, motor.settings);
        step(p.w, 180);
        require(std::get<MotorJointState>(p.w.joint_state(h)->details).linear_error.length() < 1 * scale,
                "Motor follows new relative target");
        p.w.set_transform(p.a, {{100 * scale, 100 * scale}, std::numbers::pi_v<float> / 2},
                          TeleportVelocityMode::Clear);
        step(p.w, 180);
        require(std::get<MotorJointState>(p.w.joint_state(h)->details).linear_error.length() < 2 * scale,
                "Motor target is resolved in rotated body A local coordinates");
        auto invalid = motor.settings;
        invalid.correction_factor = 1.1f;
        require(!p.w.update_motor_joint(h, invalid), "Motor correction out of range rejected");
        pd.local_axis = {};
        require(!p.w.create_prismatic_joint(pd).is_valid(), "Zero local axis rejected");
        pd.local_axis = {std::numeric_limits<float>::infinity(), 0};
        require(!p.w.create_prismatic_joint(pd).is_valid(), "Nonfinite axis rejected");
    }
    for (float scale : {.5f, 1.f, 2.f})
    {
        Pair linear(scale);
        PrismaticJointDefinition d{linear.a, linear.b};
        d.settings.enable_motor = true;
        d.settings.motor_speed = 100;
        d.settings.max_motor_force = 10;
        auto h = linear.w.create_prismatic_joint(d);
        step(linear.w);
        require(near(linear.w.body_state(linear.b)->velocity.x, 10.f / 60, .005f) &&
                    near(std::get<PrismaticJointState>(linear.w.joint_state(h)->details).motor_force, 10, .05f),
                "Prismatic force and acceleration respect EU units across scales");
        Pair rotary(scale);
        WheelJointDefinition wheel{rotary.a, rotary.b};
        wheel.settings.enable_motor = true;
        wheel.settings.motor_speed = 10;
        wheel.settings.max_motor_torque = 100;
        h = rotary.w.create_wheel_joint(wheel);
        step(rotary.w);
        require(near(rotary.w.body_state(rotary.b)->angular_velocity, .025f, .003f) &&
                    near(std::get<WheelJointState>(rotary.w.joint_state(h)->details).motor_torque, 100, .1f),
                "Wheel torque and inertia respect squared EU units");
        Pair motor(scale);
        MotorJointDefinition pose{motor.a, motor.b};
        pose.settings.linear_offset = {100, 0};
        pose.settings.angular_offset = 1;
        pose.settings.max_force = 10;
        pose.settings.max_torque = 100;
        h = motor.w.create_motor_joint(pose);
        step(motor.w);
        require(near(motor.w.joint_state(h)->reaction_force.length(), 10, .1f) &&
                    near(std::abs(motor.w.joint_state(h)->reaction_torque), 100, .1f),
                "Motor respects configured force and torque ceilings");
        Pair drag(scale);
        MouseJointDefinition mouse{drag.a, drag.b};
        h = drag.w.create_mouse_joint(mouse);
        mouse.settings.target = {100, 0};
        mouse.settings.max_force = 10;
        drag.w.update_mouse_joint(h, mouse.settings);
        step(drag.w);
        require(near(drag.w.joint_state(h)->reaction_force.length(), 10, .1f), "Mouse force ceiling respects EU units");
    }
    Pair p;
    FilterJointDefinition fd{p.a, p.b};
    auto f = p.w.create_filter_joint(fd);
    auto state = p.w.joint_state(f);
    require(f.is_valid() && std::holds_alternative<FilterJointState>(state->details) &&
                state->reaction_force == Vector2{} && state->reaction_torque == 0,
            "Filter has no mechanical reaction");
    Events events;
    p.w.add_listener(events);
    step(p.w, 2);
    require(events.count(CollisionEventPhase::Begin) == 0, "Filter disables pair collision");
    p.w.destroy_joint(f);
    step(p.w, 3);
    require(events.count(CollisionEventPhase::Begin) > 0, "Destroy Filter restores collision");
    f = p.w.create_filter_joint(fd);
    p.w.set_transform(p.b, {}, TeleportVelocityMode::Clear);
    step(p.w, 2);
    require(p.w.last_step_stats().contacts == 0, "Creating a Filter also removes an already established collision");
    const auto previous_begins = events.count(CollisionEventPhase::Begin);
    p.bob.tick = [&] {
        p.w.destroy_joint(f);
        throw std::runtime_error("abort after filter destruction");
    };
    try
    {
        step(p.w);
        require(false, "Callback throws");
    }
    catch (const std::runtime_error &)
    {
    }
    p.bob.tick = {};
    step(p.w, 3);
    require(!p.w.joint_state(f) && events.count(CollisionEventPhase::Begin) > previous_begins,
            "Aborted deferred Filter destruction restores ordinary pair detection");
    f=p.w.create_filter_joint(fd);p.w.set_transform(p.b,{},TeleportVelocityMode::Clear);
    auto sensor_definition=p.bob.collider;sensor_definition.response=CollisionResponse::Overlap;p.w.update_collider(p.w.collider_id(p.b,0),sensor_definition);step(p.w,2);
    require(p.w.last_step_stats().contacts>0,"Native Filter joints suppress solid response while Sensor overlaps retain their own filters");
    CollisionQueryFilter query_filter;query_filter.ignored_object=p.a;
    require(p.w.shape_cast({CircleShape{{},5},{{-40,0},0},{80,0},query_filter}),"Filter joints do not suppress stateless geometric queries");
    Probe another;
    another.definition.type = BodyType::Static;
    auto c = another.add(p.w);
    require(p.w.create_filter_joint({p.a, c}).is_valid(), "Filter accepts static static pair");
    require(!p.w.create_weld_joint({p.a, c}).is_valid() && !p.w.create_mouse_joint({p.b, p.a}).is_valid() &&
                !p.w.create_filter_joint({p.a, p.a}).is_valid(),
            "Mechanical and mouse type rules enforced");
}
} // namespace
int main(int argc, char **argv)
{
    std::string_view mode = argc > 1 ? argv[1] : "all";
    if (mode == "terrain" || mode == "all")
    {
        terrain();
        contacts();
        deferred();
        terrain_filters_and_pending_owner();
    }
    if (mode == "target" || mode == "all")
        targets();
    if (mode == "joints" || mode == "all")
        joints();
}
