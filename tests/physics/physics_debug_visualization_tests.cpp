#include "engine/physics/physics_debug_draw.h"
#include "physics_test_support.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <numbers>
#include <stdexcept>
#include <type_traits>
namespace
{
using C = elysia::tools::DebugDrawCategory;
using elysia::tools::DebugDraw;
using elysia::tools::DebugDrawLine;
using elysia::tools::DebugDrawPoint;
constexpr float pi = std::numbers::pi_v<float>;
Vector2 axis(float angle)
{
    return {std::cos(angle), std::sin(angle)};
}
Vector2 transformed(Vector2 local, PhysicsPose pose)
{
    auto r = axis(pose.angle);
    return pose.position + Vector2{r.x * local.x - r.y * local.y, r.y * local.x + r.x * local.y};
}
PhysicsPose pose_at(PhysicsPose a, PhysicsPose b, float alpha)
{
    return {a.position + (b.position - a.position) * alpha,
            a.angle + std::remainder(b.angle - a.angle, 2 * pi) * alpha};
}
bool close(Vector2 a, Vector2 b, float tolerance = .02f)
{
    return (a - b).length() <= tolerance;
}
void draw(const PhysicsDebugSnapshot &snapshot, C category)
{
    auto *debug = DebugDraw::instance();
    debug->clear();
    debug->set_enabled(true);
    debug->set_enabled_categories(category);
    submit_physics_debug_snapshot(snapshot, *debug);
}
bool marked(Vector2 point, C category)
{
    for (const auto &c : DebugDraw::instance()->commands())
        if (c.category == category)
            if (auto *p = std::get_if<DebugDrawPoint>(&c.primitive); p && close(p->position, point))
                return true;
    return false;
}
bool line(Vector2 a, Vector2 b, C category)
{
    for (const auto &c : DebugDraw::instance()->commands())
        if (c.category == category)
            if (auto *p = std::get_if<DebugDrawLine>(&c.primitive);
                p && ((close(p->start, a) && close(p->end, b)) || (close(p->start, b) && close(p->end, a))))
                return true;
    return false;
}
bool endpoint(Vector2 point, C category)
{
    for (const auto &c : DebugDraw::instance()->commands())
        if (c.category == category)
            if (auto *p = std::get_if<DebugDrawLine>(&c.primitive);
                p && (close(p->start, point) || close(p->end, point)))
                return true;
    return false;
}
void finite_commands()
{
    for (const auto &c : DebugDraw::instance()->commands())
        std::visit(
            [](const auto &p) {
                using T = std::decay_t<decltype(p)>;
                auto valid = [](Vector2 v) { return std::isfinite(v.x) && std::isfinite(v.y); };
                if constexpr (std::is_same_v<T, DebugDrawLine>)
                    require(valid(p.start) && valid(p.end), "Every generated line is finite");
                else if constexpr (std::is_same_v<T, DebugDrawPoint>)
                    require(valid(p.position) && std::isfinite(p.size), "Every marker is finite");
            },
            c.primitive);
}
void categories()
{
    auto *debug = DebugDraw::instance();
    debug->set_enabled(true);
    debug->set_enabled_categories(C::Default);
    require(!debug->is_enabled(C::PhysicsJointDetails) && !debug->is_enabled(C::PhysicsChainDetails) &&
                !debug->is_enabled(C::PhysicsTarget),
            "New detail categories default off");
    auto capture = physics_debug_capture_for(*debug);
    require(!captures_physics_debug(capture, PhysicsDebugCapture::Chains) &&
                !captures_physics_debug(capture, PhysicsDebugCapture::Targets),
            "Default capture excludes expensive details");
    for (auto pair : {std::pair{C::PhysicsJointDetails, PhysicsDebugCapture::Joints},
                      std::pair{C::PhysicsChainDetails, PhysicsDebugCapture::Chains},
                      std::pair{C::PhysicsTarget, PhysicsDebugCapture::Targets}})
    {
        debug->set_enabled_categories(pair.first);
        require(physics_debug_capture_for(*debug) == pair.second, "Each detail category independently maps to capture");
    }
    debug->set_enabled_categories(C::All);
    require(physics_debug_capture_for(*debug) == PhysicsDebugCapture::All, "All includes every new capture type");
    debug->set_enabled(false);
    require(physics_debug_capture_for(*debug) == PhysicsDebugCapture::None && debug->commands().empty(),
            "Disabled drawing captures nothing and clears commands");
}
JointHandle make_joint(PhysicsWorld &w, PhysicsObjectHandle a, PhysicsObjectHandle b, int type)
{
    const Vector2 first{5, 4}, second{-3, 2};
    switch (type)
    {
    case 0: {
        DistanceJointDefinition d{a, b, first, second};
        d.settings.length = 70;
        d.settings.enable_spring = true;
        d.settings.enable_limit = true;
        d.settings.min_length = 30;
        d.settings.max_length = 110;
        return w.create_distance_joint(d);
    }
    case 1: {
        RevoluteJointDefinition d{a, b, first, second, .2f};
        d.settings.enable_spring = true;
        d.settings.target_angle = .3f;
        d.settings.enable_limit = true;
        d.settings.lower_angle = -.5f;
        d.settings.upper_angle = .5f;
        return w.create_revolute_joint(d);
    }
    case 2:
        return w.create_weld_joint({a, b, first, second, .2f});
    case 3: {
        PrismaticJointDefinition d{a, b, first, second, {2, 1}, .2f};
        d.settings.enable_spring = true;
        d.settings.target_translation = 30;
        d.settings.enable_limit = true;
        d.settings.lower_translation = -20;
        d.settings.upper_translation = 80;
        return w.create_prismatic_joint(d);
    }
    case 4: {
        WheelJointDefinition d{a, b, first, second, {1, 2}};
        d.settings.enable_limit = true;
        d.settings.lower_translation = -20;
        d.settings.upper_translation = 80;
        d.settings.enable_motor = true;
        d.settings.motor_speed = 1;
        return w.create_wheel_joint(d);
    }
    case 5: {
        auto h = w.create_mouse_joint({a, b, {{60, 40}}});
        w.update_mouse_joint(h, {{90, 25}});
        return h;
    }
    case 6:
        return w.create_motor_joint({a, b, {{30, 15}, .5f}});
    default:
        return w.create_filter_joint({a, b});
    }
}
void joint_geometry()
{
    for (float scale : {.5f, 1.f, 2.f})
        for (int type = 0; type < 8; ++type)
        {
            PhysicsDebugSnapshot snapshot;
            {
                Probe a, b;
                a.definition.type = BodyType::Static;
                a.definition.angle = .4f;
                b.definition.fixed_rotation = false;
                b.definition.angle = -.1f;
                b.set_position({100, 50});
                PhysicsWorld w({.units_per_meter = 100 * scale});
                auto ah = a.add(w), bh = b.add(w);
                auto h = make_joint(w, ah, bh, type);
                require(h.is_valid(), "Each joint type can be captured");
                w.set_debug_capture(PhysicsDebugCapture::Joints);
                snapshot = w.debug_snapshot();
                require(snapshot.joints.size() == 1 && snapshot.joints[0].state.first == ah &&
                            snapshot.joints[0].state.second == bh &&
                            snapshot.joints[0].state.details.index() == std::size_t(type),
                        "Typed joint snapshots share public state and endpoint identities");
            }
            auto &j = snapshot.joints[0];
            j.first_previous = {{-10, 20}, -.4f};
            j.second_previous = {{80, -10}, -.8f};
            for (float alpha : {0.f, .5f, 1.f})
            {
                snapshot.interpolation_alpha = alpha;
                auto first = pose_at(j.first_previous, j.first_current, alpha),
                     second = pose_at(j.second_previous, j.second_current, alpha);
                Vector2 a = type == 7   ? first.position
                            : type == 5 ? Vector2{90, 25}
                                        : transformed(j.local_anchor_first, first),
                        b = type == 7 ? second.position : transformed(j.local_anchor_second, second);
                draw(snapshot, C::PhysicsJoint);
                require(marked(a, C::PhysicsJoint) && marked(b, C::PhysicsJoint),
                        "Basic anchors follow render interpolation; Mouse world target is independent");
                if (type == 7)
                    require(!line(a, b, C::PhysicsJoint) && !DebugDraw::instance()->commands().empty(),
                            "Filter is a dotted relation, never a solid rod");
                draw(snapshot, C::PhysicsJointDetails);
                const float unit = 100 * scale, glyph = .25f * unit, radius = .2f * unit;
                switch (type)
                {
                case 0: {
                    auto along = (b - a).normalized();
                    require(marked(a + along * 70, C::PhysicsJointDetails) &&
                                marked(a + along * 30, C::PhysicsJointDetails) == false,
                            "Distance rest length uses actual EU values");
                    require(endpoint(a + along * 30, C::PhysicsJointDetails) &&
                                endpoint(a + along * 110, C::PhysicsJointDetails),
                            "Distance limits are at real length bounds");
                    break;
                }
                case 1: {
                    float reference = first.angle + .2f;
                    require(marked(a + axis(reference - .5f) * radius, C::PhysicsJointDetails) &&
                                marked(a + axis(reference + .5f) * radius, C::PhysicsJointDetails),
                            "Revolute arc respects reference angle and limits");
                    require(line(a, a + axis(reference + .3f) * glyph, C::PhysicsJointDetails),
                            "Revolute spring target follows body A");
                    break;
                }
                case 2:
                    require(line(a, a + axis(first.angle + .2f) * glyph, C::PhysicsJointDetails) &&
                                line(b, b + axis(second.angle) * glyph, C::PhysicsJointDetails),
                            "Weld shows both reference frames");
                    break;
                case 3:
                case 4: {
                    auto local = j.local_axis_first;
                    require(near(local.length(), 1), "Native normalized local axis is retained");
                    auto along = transformed(local, {Vector2{}, first.angle});
                    require(endpoint(a + along * (-20), C::PhysicsJointDetails) &&
                                endpoint(a + along * 80, C::PhysicsJointDetails),
                            "Translation limits rotate with the first body");
                    if (type == 3)
                        require(marked(a + along * 30, C::PhysicsJointDetails),
                                "Prismatic target translation is visible");
                    else
                        require(endpoint(b + axis(second.angle + pi * .75f) * radius, C::PhysicsJointDetails),
                                "Wheel motor direction is visible");
                    break;
                }
                case 5:
                    require(marked({90, 25}, C::PhysicsJointDetails) && line(b, {90, 25}, C::PhysicsJointDetails),
                            "Mouse world target and error remain independent of anchor interpolation");
                    break;
                case 6: {
                    auto target = transformed({30, 15}, first);
                    require(marked(target, C::PhysicsJointDetails) &&
                                line(target, target + axis(first.angle + .5f) * glyph, C::PhysicsJointDetails) &&
                                line(second.position, target, C::PhysicsJointDetails),
                            "Motor shows local relative target pose and error");
                    break;
                }
                default:
                    require(DebugDraw::instance()->commands().empty(), "Filter adds no mechanical details");
                    break;
                }
                finite_commands();
            }
            if (type == 0)
            {
                auto &d = std::get<DistanceJointState>(j.state.details);
                d.settings.enable_spring = false;
                snapshot.interpolation_alpha = 1;
                auto a = transformed(j.local_anchor_first, j.first_current),
                     b = transformed(j.local_anchor_second, j.second_current);
                auto along = (b - a).normalized();
                draw(snapshot, C::PhysicsJointDetails);
                require(!endpoint(a + along * 30, C::PhysicsJointDetails), "Inactive distance limits are not rendered");
            }
            if (type == 4)
            {
                auto &d = std::get<WheelJointState>(j.state.details);
                d.settings.motor_speed = -1;
                snapshot.interpolation_alpha = 1;
                draw(snapshot, C::PhysicsJointDetails);
                auto b = transformed(j.local_anchor_second, j.second_current);
                require(endpoint(b + axis(j.second_current.angle - pi * .75f) * (.2f * snapshot.units_per_meter),
                                 C::PhysicsJointDetails),
                        "Wheel reversal reverses its direction glyph");
            }
        }
    PhysicsDebugSnapshot snapshot;
    PhysicsDebugJoint j;
    j.state.details = PrismaticJointState{};
    j.local_anchor_first = {10, 0};
    j.first_previous = {{}, 3.1f};
    j.first_current = {{}, -3.1f};
    snapshot.joints.push_back(j);
    snapshot.interpolation_alpha = .5f;
    draw(snapshot, C::PhysicsJoint);
    require(marked({-10, 0}, C::PhysicsJoint), "Debug interpolation crosses pi by the shortest path");
    snapshot.joints[0].state.details = DistanceJointState{{.length = 10, .enable_spring = true}};
    snapshot.joints[0].local_anchor_first = {};
    snapshot.joints[0].first_previous = snapshot.joints[0].first_current = {};
    snapshot.joints[0].second_previous = snapshot.joints[0].second_current = {};
    draw(snapshot, C::All);
    finite_commands();
    require(!DebugDraw::instance()->commands().empty(), "Coincident anchors produce finite useful markers");
}
void chains()
{
    for (float scale : {.5f, 1.f, 2.f})
    {
        Probe owner;
        owner.definition.type = BodyType::Kinematic;
        owner.definition.fixed_rotation = false;
        owner.definition.angle = .4f;
        owner.set_position({100, 50});
        PhysicsWorld w({.units_per_meter = 100 * scale});
        auto a = owner.add(w);
        ChainDefinition d;
        d.local_vertices = {{0, 0}, {100, 0}, {150, 50}};
        d.ghost_before = Vector2{-50, -10};
        d.ghost_after = Vector2{180, 80};
        auto h = w.create_chain(a, d);
        w.set_debug_capture(PhysicsDebugCapture::Chains);
        auto snapshot = w.debug_snapshot();
        require(snapshot.shapes.empty() && snapshot.chains.size() == 1, "Chains are captured independently");
        auto &c = snapshot.chains[0];
        require(c.handle == h && c.owner == a && c.colliders[1] == w.chain_collider(h, 1) &&
                    close(*c.ghost_before, {-50, -10}) && close(*c.ghost_after, {180, 80}),
                "Chain snapshot owns identity and actual ghost data");
        c.previous = {{0, 0}, -.3f};
        for (float alpha : {0.f, .5f, 1.f})
        {
            snapshot.interpolation_alpha = alpha;
            auto pose = pose_at(c.previous, c.current, alpha);
            draw(snapshot, C::PhysicsChainDetails);
            require(marked(transformed(c.local_vertices[1], pose), C::PhysicsChainDetails) &&
                        marked(transformed(*c.ghost_before, pose), C::PhysicsChainDetails) &&
                        marked(transformed(*c.ghost_after, pose), C::PhysicsChainDetails),
                    "Chain vertices and ghosts use interpolated owner pose");
            finite_commands();
        }
        d.loop = true;
        d.ghost_before.reset();
        d.ghost_after.reset();
        d.local_vertices = {{300, 0}, {400, 0}, {400, 100}, {300, 100}};
        auto loop = w.create_chain(a, d);
        w.finalize_frame(0);
        auto closed = w.debug_snapshot().chains.back();
        require(closed.handle == loop && closed.loop && !closed.ghost_before && !closed.ghost_after,
                "Closed chain has no endpoint ghosts");
        w.set_debug_capture(PhysicsDebugCapture::Shapes);
        draw(w.debug_snapshot(), C::PhysicsCollider);
        bool arrow_head = false;
        for (const auto &cmd : DebugDraw::instance()->commands())
            if (auto *p = std::get_if<DebugDrawLine>(&cmd.primitive);
                p && p->start != p->end && (p->start - p->end).length() < .05f * (100 * scale))
                arrow_head = true;
        require(arrow_head, "Chain front direction includes arrowheads");
        w.destroy_chain(h);
        w.set_debug_capture(PhysicsDebugCapture::Chains);
        require(w.debug_snapshot().chains.size() == 1, "Removed Chain has no debug record");
        w.set_body_enabled(a, false);
        w.finalize_frame(0);
        require(w.debug_snapshot().chains.empty(), "Disabled owner hides every Chain");
    }
}
void targets()
{
    for (float scale : {.5f, 1.f, 2.f})
        for (bool fixed : {false, true})
        {
            Probe owner;
            owner.definition.type = BodyType::Kinematic;
            owner.definition.fixed_rotation = fixed;
            PhysicsWorld w({.units_per_meter = 100 * scale});
            auto h = owner.add(w);
            w.set_target_transform(h, {{scale, 0}, .1f}, 1. / 60);
            require(w.debug_snapshot().targets.empty(), "Capture off does not construct target contours");
            w.set_debug_capture(PhysicsDebugCapture::Targets);
            auto first = w.debug_snapshot().targets[0];
            require(first.pending && first.shapes.size() == 1 && close(first.requested_pose.position, {scale, 0}) &&
                        near(first.effective_pose.angle, fixed ? 0 : .1f) &&
                        near(float(first.fixed_delta_seconds), 1.f / 60, .00001f),
                    "Pending target retains requested/effective pose and step");
            draw(w.debug_snapshot(), C::PhysicsTarget);
            require(marked(first.effective_pose.position, C::PhysicsTarget), "Effective target marker is drawn");
            finite_commands();
            step(w);
            require(w.debug_snapshot().targets.size() == 1 && !w.debug_snapshot().targets[0].pending,
                    "Target becomes a completed-step diagnostic");
            w.finalize_frame(.5);
            require(w.debug_snapshot().targets.size() == 1, "Render-only frames retain completed target");
            step(w);
            require(w.debug_snapshot().targets.empty(), "A new fixed step without submission expires the target");
            for (int override = 0; override < 7; ++override)
            {
                w.set_target_transform(h, {{10, 10}, .2f}, 1. / 60);
                switch (override)
                {
                case 0:
                    w.set_velocity(h, {});
                    break;
                case 1:
                    w.set_velocity_x(h, 0);
                    break;
                case 2:
                    w.set_velocity_y(h, 0);
                    break;
                case 3:
                    w.set_angular_velocity(h, 0);
                    break;
                case 4:
                    w.set_transform(h, {});
                    break;
                case 5:
                    w.teleport_object(h, {});
                    break;
                default:
                    w.set_body_enabled(h, false);
                    break;
                }
                w.finalize_frame(0);
                require(w.debug_snapshot().targets.empty(),
                        "Explicit motion overrides or disablement clear target records");
                w.set_body_enabled(h, true);
            }
            w.set_target_transform(h, {{1, 0}, 0}, 1. / 60);
            w.set_target_transform(h, {{2, 0}, 0}, 1. / 60);
            w.finalize_frame(0);
            require(near(w.debug_snapshot().targets[0].requested_pose.position.x, 2), "Last executed target wins FIFO");
            auto before = w.body_state(h);
            require(!w.set_target_transform(h, {}, 0), "Invalid target rejected");
            w.finalize_frame(0);
            require(near(w.debug_snapshot().targets[0].requested_pose.position.x, 2) &&
                        w.body_state(h)->velocity == before->velocity,
                    "Invalid target leaves state and diagnostics unchanged");
            w.set_velocity(h, {10, 0});
            auto state = w.body_state(h);
            w.set_target_transform(h, {state->position, state->angle}, 1. / 60);
            w.finalize_frame(0);
            require(w.body_state(h)->velocity == Vector2{} && w.debug_snapshot().targets.size() == 1,
                    "Same-pose stop is still a valid target diagnostic");
            w.reset();
            w.finalize_frame(0);
            require(w.debug_snapshot().targets.empty(), "Reset clears target diagnostics");
        }
    // A listener submission happens after simulation and belongs to the following step.
    Probe platform, bob;
    platform.definition.type = BodyType::Kinematic;
    PhysicsWorld world;
    auto a = platform.add(world);
    bob.add(world);
    Events events;
    bool once = false;
    events.callback = [&](const CollisionEvent &) {
        if (!once)
        {
            once = true;
            world.set_target_transform(a, {{1, 0}, 0}, 1. / 60);
        }
    };
    world.add_listener(events);
    world.set_debug_capture(PhysicsDebugCapture::Targets);
    step(world);
    require(once && world.debug_snapshot().targets[0].pending, "Post-step callback target remains pending");
    step(world);
    require(!world.debug_snapshot().targets[0].pending, "Post-step command is consumed by the next simulation");
    // Collider-less bodies still have a target frame; source records need no geometry allocation.
    Probe empty;
    PhysicsWorld w;
    auto h = w.register_object(empty, {.type = BodyType::Kinematic}, {});
    w.set_target_transform(h, {{10, 0}, 1}, 1. / 60);
    w.set_debug_capture(PhysicsDebugCapture::Targets);
    draw(w.debug_snapshot(), C::PhysicsTarget);
    require(w.debug_snapshot().targets[0].shapes.empty() && marked({10, 0}, C::PhysicsTarget),
            "Collider-less target pose stays observable");
}
void target_contours()
{
    for (float scale : {.5f, 1.f, 2.f})
    {
        Probe owner;
        std::array<Collider, 5> colliders;
        colliders[0].shape = BoxShape{{12, 5}, {8, 4}, .25f};
        colliders[1].shape = CircleShape{{-20, 5}, 5};
        colliders[2].shape = CapsuleShape{{0, 15}, {10, 15}, 4};
        colliders[3].shape = ConvexPolygonShape{{Vector2{20, 0}, Vector2{40, 0}, Vector2{30, 20}}, 3};
        colliders[4].shape = SegmentShape{{0, 30}, {40, 30}};
        PhysicsWorld world({.units_per_meter = 100 * scale});
        auto h = world.register_object(owner, {.type = BodyType::Kinematic, .fixed_rotation = false}, colliders);
        ChainDefinition chain;
        chain.local_vertices = {{0, 50}, {40, 50}};
        world.create_chain(h, chain);
        PhysicsPose target{{150, 220}, .7f};
        world.set_target_transform(h, target, .2);
        world.set_debug_capture(PhysicsDebugCapture::Targets);
        const auto &data = world.debug_snapshot().targets[0];
        require(data.shapes.size() == 6 && world.debug_snapshot().shapes.empty(),
                "Target-only capture owns all area/Segment/Chain contours without base shape capture");
        const auto &box = std::get<WorldPolygon>(data.shapes[0]);
        auto expected = std::get<WorldPolygon>(transform_collider_shape(colliders[0].shape, target));
        for (unsigned n = 0; n < 4; ++n)
            require(std::ranges::any_of(box.vertices.begin(), box.vertices.begin() + box.vertex_count,
                                        [&](Vector2 p) { return close(p, expected.vertices[n]); }),
                    "Target box includes local rotation and offset");
        auto circle = std::get<WorldCircle>(data.shapes[1]);
        require(close(circle.center, transformed({-20, 5}, target)) && near(circle.radius, 5),
                "Target circles use body-local centers and EU radii");
        auto capsule = std::get<WorldCapsule>(data.shapes[2]);
        require(close(capsule.first, transformed({0, 15}, target)) &&
                    close(capsule.second, transformed({10, 15}, target)),
                "Target capsules transform both endpoints");
        auto polygon = std::get<WorldPolygon>(data.shapes[3]);
        require(polygon.vertex_count == 3, "Target polygons retain actual vertex count");
        for (unsigned n = 0; n < 3; ++n)
        {
            auto p = transformed(std::get<ConvexPolygonShape>(colliders[3].shape).local_vertices[n], target);
            require(std::ranges::any_of(polygon.vertices.begin(), polygon.vertices.begin() + polygon.vertex_count,
                                        [&](Vector2 v) { return close(p, v); }),
                    "Target polygon vertices follow target pose");
        }
        require(!std::get<WorldSegment>(data.shapes[4]).one_sided && std::get<WorldSegment>(data.shapes[5]).one_sided,
                "Target edges retain their native side semantics");
        draw(world.debug_snapshot(), C::PhysicsTarget);
        finite_commands();
    }
}
void lifecycle()
{
    Probe owner, bob, spawn;
    owner.definition.type = BodyType::Static;
    PhysicsWorld w;
    auto a = owner.add(w), b = bob.add(w);
    auto joint = w.create_weld_joint({a, b});
    w.set_debug_capture(PhysicsDebugCapture::All);
    require(w.debug_snapshot().joints.size() == 1, "Active joint visible");
    w.set_awake(b, false);
    w.finalize_frame(0);
    require(w.debug_snapshot().shapes.size() == 2 && !w.debug_snapshot().shapes.back().awake,
            "Sleeping bodies remain visible");
    w.set_body_enabled(a, false);
    w.finalize_frame(0);
    require(w.debug_snapshot().shapes.size() == 1 && w.debug_snapshot().velocities.size() == 1 &&
                w.debug_snapshot().joints.empty(),
            "Disabled body is excluded consistently");
    w.set_body_enabled(a, true);
    bob.set_active(false);
    w.finalize_frame(0);
    require(w.debug_snapshot().shapes.size() == 1 && w.debug_snapshot().joints.empty(),
            "Inactive bodies are excluded even before enablement sync");
    bob.set_active(true);
    Probe disabled;
    auto disabled_handle = w.register_object(disabled, {.enabled = false}, {&disabled.collider, 1});
    auto hidden_joint = w.create_weld_joint({a, disabled_handle});
    w.finalize_frame(0);
    require(hidden_joint.is_valid() && w.debug_snapshot().joints.size() == 1 &&
                w.debug_snapshot().velocities.size() == 2,
            "A disabled dynamic second endpoint is consistently excluded");
    w.unregister_object(disabled_handle);
    bool once = false;
    PhysicsObjectHandle created;
    owner.tick = [&] {
        if (once)
            return;
        once = true;
        created = w.register_object(spawn, {.type = BodyType::Kinematic}, {&spawn.collider, 1});
        w.create_weld_joint({created, b});
        ChainDefinition d;
        d.local_vertices = {{0, 100}, {100, 100}};
        w.create_chain(created, d);
        w.set_target_transform(created, {{1, 0}, 0}, 1. / 60);
        w.set_debug_capture(PhysicsDebugCapture::None);
        w.set_debug_capture(PhysicsDebugCapture::All);
        require(w.debug_snapshot().velocities.size() == 2 && w.debug_snapshot().chains.empty() &&
                    w.debug_snapshot().targets.empty() && w.debug_snapshot().joints.size() == 1,
                "Capture during a callback never reads pending native IDs");
    };
    step(w);
    owner.tick = {};
    require(w.debug_snapshot().chains.size() == 1 && w.debug_snapshot().targets.size() == 1 &&
                w.debug_snapshot().joints.size() == 2,
            "Committed records appear together");
    w.unregister_object(created);
    w.finalize_frame(0);
    require(w.debug_snapshot().targets.empty() && w.debug_snapshot().chains.empty() &&
                w.debug_snapshot().joints.size() == 1,
            "Owner removal clears dependent debug records");
    w.unregister_object(b);
    w.finalize_frame(0);
    require(w.debug_snapshot().joints.empty(), "Removed endpoint hides its joint");
    w.reset();
    w.finalize_frame(0);
    require(w.debug_snapshot().shapes.empty() && w.debug_snapshot().velocities.empty(),
            "Reset leaves no stale debug data");
    (void)joint;
}
} // namespace
int main()
{
    categories();
    joint_geometry();
    chains();
    targets();
    target_contours();
    lifecycle();
    DebugDraw::instance()->set_enabled(false);
    DebugDraw::instance()->set_enabled_categories(C::Default);
}
