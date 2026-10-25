#include "engine/physics/contracts/physics_participant.h"
#include "physics_test_support.h"
#include <box2d/box2d.h>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace
{
void same(Vector2 a, Vector2 b, const char *message, float tolerance = .01f)
{
    require(near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance), message);
}
Vector2 cross(float w, Vector2 r)
{
    return {-w * r.y, w * r.x};
}
PhysicsWorldConfig config(float scale = 32)
{
    PhysicsWorldConfig c;
    c.units_per_meter = scale;
    c.gravity = {};
    return c;
}
void setup(Probe &body, float scale)
{
    body.collider.shape = BoxShape{{}, {scale, scale / 2}, 0};
    body.collider.density = {.5f};
    body.definition.fixed_rotation = false;
}
void formulas()
{
    for (float scale : {1.f, 32.f, 100.f, 1000.f})
    {
        Probe body;
        setup(body, scale);
        PhysicsWorld world(config(scale));
        const auto h = body.add(world);
        require(h.is_valid(), "Density registration");
        require(std::holds_alternative<DensityMass>(*world.mass_properties(h)), "Default density configuration");
        const auto density = *world.body_state(h);
        require(near(density.mass, 1), "Density mass");
        require(world.set_mass_properties(h, ExplicitMass{3}), "Set explicit mass");
        require(near(world.body_state(h)->rotational_inertia, density.rotational_inertia * 3, .002f * scale * scale),
                "Explicit mass scales density inertia");
        BodyMassData custom{2, {scale / 4, -scale / 8}, 4 * scale * scale};
        require(world.set_mass_properties(h, custom), "Set custom properties");
        auto state = *world.body_state(h);
        same(state.local_center, custom.local_center, "Local center uses EU");
        same(state.world_center, custom.local_center, "World center");
        require(near(state.rotational_inertia / (scale * scale), 4), "Inertia squared unit conversion");
        world.apply_impulse(h, {6 * scale, 0});
        same(world.body_state(h)->velocity, {3 * scale, 0}, "Impulse divided by independent mass");
        world.apply_angular_impulse(h, 8 * scale * scale);
        require(near(world.body_state(h)->angular_velocity, 2), "Angular impulse divided by independent inertia");
        world.set_angular_velocity(h, 0);
        world.set_velocity(h, {});
        world.apply_impulse(h, {0, 2 * scale}, state.world_center + Vector2{scale, 0});
        require(near(world.body_state(h)->angular_velocity, .5f), "Off-center impulse uses custom COM and inertia");
        world.set_angular_velocity(h, 0);
        world.set_velocity(h, {});
        world.apply_torque(h, 4 * scale * scale);
        world.apply_force(h, {0, 2 * scale}, state.world_center + Vector2{scale, 0});
        world.step(1. / 60);
        require(near(world.body_state(h)->angular_velocity, .025f, .001f), "Force and torque use independent inertia");
        require(world.set_mass_properties(h, DensityMass{}), "Restore density");
        require(near(world.body_state(h)->mass, density.mass), "Restored density mass");
        require(near(world.body_state(h)->rotational_inertia, density.rotational_inertia, .002f * scale * scale),
                "Restored density inertia");
    }
    for (bool explicit_mass : {false, true})
    {
        Probe plank;
        setup(plank, 32);
        plank.collider.shape = BoxShape{{}, {128, 4}, 0};
        plank.definition.mass_properties =
            explicit_mass ? MassProperties{ExplicitMass{.5f}} : MassProperties{BodyMassData{.5f, {}, 5466.666667f}};
        PhysicsWorld world(config());
        const auto h = plank.add(world);
        require(h.is_valid() && near(world.body_state(h)->mass, .5f), "Seesaw mass");
        require(near(world.body_state(h)->rotational_inertia, explicit_mass ? 2733.333333f : 5466.666667f, .02f),
                "Seesaw custom inertia is retained while explicit mass halves it");
    }
}
void transitions()
{
    Probe body, anchor;
    setup(body, 32);
    body.set_position({100, 200});
    body.definition.angle = std::numbers::pi_v<float> / 2;
    PhysicsWorld world(config());
    const auto h = body.add(world), a = world.register_object(anchor, {.type = BodyType::Static}, {});
    auto joint = world.create_revolute_joint({.first = a, .second = h, .local_anchor_second = {10, 3}});
    const auto anchors = *world.joint_state(joint);
    world.set_velocity(h, {20, 30});
    world.set_angular_velocity(h, 2);
    const auto before = *world.body_state(h);
    const auto origin_velocity =
        before.velocity - cross(before.angular_velocity, before.world_center - before.position);
    require(world.set_mass_properties(h, BodyMassData{2, {16, 8}, 500}), "Shift COM");
    const auto after = *world.body_state(h);
    same(after.position, before.position, "Body origin does not jump");
    require(after.angle == before.angle && after.angular_velocity == before.angular_velocity,
            "Rotation remains continuous");
    same(after.world_center, after.position + Vector2{-8, 16}, "Rotated local COM");
    same(after.velocity - cross(after.angular_velocity, after.world_center - after.position), origin_velocity,
         "Body-origin instantaneous velocity is continuous");
    const auto changed = *world.joint_state(joint);
    same(changed.anchor_first, anchors.anchor_first, "First joint anchor stable");
    same(changed.anchor_second, anchors.anchor_second, "Second joint anchor stable");
    const auto id = world.collider_id(h, 0);
    auto collider = body.collider;
    collider.shape = CircleShape{{-20, 30}, 16};
    collider.density = {4};
    require(world.update_collider(id, collider), "Replace geometry and density");
    require(world.collider_id(h, 0) == id && near(world.body_state(h)->mass, 2), "Stable collider ID and custom mass");
    same(world.body_state(h)->local_center, {16, 8}, "Custom center survives shape change");
    require(world.set_collider_enabled(id, false) && world.set_collider_enabled(id, true), "Toggle collider");
    require(near(world.body_state(h)->rotational_inertia, 500), "Custom inertia survives collider toggle");
    collider.response = CollisionResponse::Overlap;
    require(world.set_mass_properties(h, DensityMass{}) && world.update_collider(id, collider), "Non-mass sensor");
    require(world.body_state(h)->mass == 0, "Sensor mass is excluded");
    collider.sensor_contributes_mass = true;
    require(world.update_collider(id, collider) && world.body_state(h)->mass > 0, "Mass sensor is included");
    require(world.set_mass_properties(h, ExplicitMass{3}), "Return to explicit policy");
    collider.shape = BoxShape{{}, {32, 16}, 0};
    require(world.update_collider(id, collider) && near(world.body_state(h)->mass, 3),
            "Explicit mass survives shape update");
    world.set_awake(h, false);
    require(world.set_mass_properties(h, BodyMassData{2, {}, 500}) && world.body_state(h)->awake,
            "Mass update wakes body");
    world.destroy_joint(joint); // The separate disabled-joint-island issue is outside this regression.
    require(world.set_body_enabled(h, false), "Disable unconnected body");
    require(world.set_mass_properties(h, BodyMassData{4, {1, 2}, 700}), "Disabled body accepts mass update");
    require(!world.body_state(h)->enabled && !world.body_state(h)->awake, "Mass update does not enable disabled body");
    require(world.set_body_enabled(h, true) && near(world.body_state(h)->mass, 4),
            "Re-enabled body retains custom data");
    world.set_debug_capture(PhysicsDebugCapture::Shapes);
    require(!world.debug_snapshot().bodies.empty(), "Inspector mass state is captured");
    const auto saved = world.debug_snapshot();
    world.reset();
    require(!saved.bodies.empty() && saved.bodies.front().owner.is_valid(), "Owned mass snapshot survives world reset");
}
void validation()
{
    const float nan = std::numeric_limits<float>::quiet_NaN(), inf = std::numeric_limits<float>::infinity();
    for (float scale : {1.f, 1000.f})
    {
        Probe body;
        setup(body, scale);
        PhysicsWorld world(config(scale));
        const auto h = body.add(world);
        const auto original = *world.mass_properties(h);
        const auto old = *world.body_state(h);
        const MassProperties invalid[] = {ExplicitMass{0},
                                          ExplicitMass{-1},
                                          ExplicitMass{nan},
                                          ExplicitMass{inf},
                                          ExplicitMass{std::numeric_limits<float>::denorm_min()},
                                          BodyMassData{0, {}, 1},
                                          BodyMassData{1, {nan, 0}, 1},
                                          BodyMassData{1, {inf, 0}, 1},
                                          BodyMassData{1, {}, -1},
                                          BodyMassData{1, {}, nan},
                                          BodyMassData{1, {}, inf},
                                          BodyMassData{1, {}, std::numeric_limits<float>::denorm_min()},
                                          BodyMassData{1, {std::numeric_limits<float>::max(), 0}, 1}};
        for (const auto &properties : invalid)
        {
            require(!world.set_mass_properties(h, properties), "Invalid properties are rejected");
            require(world.mass_properties(h) == original && world.body_state(h)->mass == old.mass,
                    "Invalid update leaves configuration and native state untouched");
            Probe invalid_owner;
            setup(invalid_owner, scale);
            invalid_owner.definition.mass_properties = properties;
            require(!invalid_owner.add(world).is_valid(), "Invalid registration is rejected before native calls");
        }
        require(world.set_mass_properties(h, BodyMassData{2, {}, 0}), "Zero inertia is supported");
        world.apply_angular_impulse(h, scale * scale);
        require(world.body_state(h)->angular_velocity == 0, "Zero inertia has no angular response");
        for (auto type : {BodyType::Static, BodyType::Kinematic})
        {
            Probe other;
            other.definition.type = type;
            const auto other_h = other.add(world);
            require(other_h.is_valid() && !world.set_mass_properties(other_h, ExplicitMass{1}) &&
                        !world.set_mass_properties(other_h, BodyMassData{}),
                    "Only dynamic bodies support overrides");
            require(world.set_mass_properties(other_h, DensityMass{}), "Non-dynamic density configuration");
            Probe invalid_other;
            invalid_other.definition.type = type;
            invalid_other.definition.mass_properties = ExplicitMass{1};
            require(!invalid_other.add(world).is_valid(), "Non-dynamic override registration rejected");
        }
        require(world.unregister_object(h) && !world.set_mass_properties(h, ExplicitMass{1}) &&
                    !world.mass_properties(h),
                "Removed identity is invalid");
    }
    Probe fixed;
    fixed.definition.mass_properties = BodyMassData{3, {1, 2}, 800};
    PhysicsWorld world(config());
    const auto h = fixed.add(world);
    require(h.is_valid() && world.body_state(h)->rotational_inertia == 0, "Fixed rotation masks effective inertia");
    require(std::get<BodyMassData>(*world.mass_properties(h)).rotational_inertia == 800, "Requested inertia retained");
    world.apply_torque(h, 10000);
    world.step(1. / 60);
    require(world.body_state(h)->angular_velocity == 0, "Custom data cannot bypass fixed rotation");
    world.reset();
    require(!world.mass_properties(h) && !world.set_mass_properties(h, DensityMass{}), "Reset invalidates mass API");
    struct Participant : elysia::physics::PhysicsParticipant
    {
        std::span<const Collider> collider_definitions() const override
        {
            return {};
        }
    } participant;
    require(!participant.mass_properties() && !participant.set_mass_properties(ExplicitMass{1}), "Unbound shortcuts");
    Probe no_shapes;
    const auto empty = world.register_object(no_shapes, {.mass_properties = BodyMassData{2, {3, 4}, 5}}, {});
    participant.bind_physics(world, empty);
    require(participant.mass_properties() && participant.set_mass_properties(BodyMassData{3, {}, 8}),
            "Bound shortcuts");
    world.set_debug_capture(PhysicsDebugCapture::Shapes);
    require(world.debug_snapshot().bodies.size() == 1 && world.debug_snapshot().shapes.empty(),
            "Colliderless mass inspection");
    participant.unbind_physics();
}
void deferred()
{
    Probe owner, created;
    setup(owner, 32);
    setup(created, 32);
    PhysicsWorld world(config());
    const auto h = owner.add(world);
    PhysicsObjectHandle deferred;
    owner.tick = [&] {
        const auto old = *world.mass_properties(h);
        BodyMassData command{2, {}, 100};
        require(world.set_mass_properties(h, command), "Queue mass update");
        command.mass = 99;
        require(world.mass_properties(h) == old, "Getter returns committed configuration");
        require(world.set_mass_properties(h, ExplicitMass{4}), "FIFO second mass update");
        deferred = created.add(world);
        require(deferred.is_valid() && !world.body_state(deferred) && !world.mass_properties(deferred),
                "Pending query is empty");
        require(world.set_mass_properties(deferred, command), "Update immediately after deferred creation");
    };
    world.step(1. / 60);
    owner.tick = {};
    require(near(world.body_state(h)->mass, 4) && near(world.body_state(deferred)->mass, 99),
            "FIFO and deferred creation");
    owner.tick = [&] {
        BodyMassData command{2, {}, 100};
        require(world.set_mass_properties(h, command), "Copied command");
        command.mass = 99;
    };
    world.step(1. / 60);
    owner.tick = {};
    require(near(world.body_state(h)->mass, 2), "Queued payload is owned by value");
    owner.tick = [&] {
        world.set_mass_properties(deferred, BodyMassData{5, {}, 100});
        world.unregister_object(deferred);
    };
    world.step(1. / 60);
    owner.tick = {};
    require(!world.mass_properties(deferred), "Queued update skips removed body");
    owner.tick = [&] {
        auto pending = created.add(world);
        require(pending.is_valid(), "Re-register retired owner");
        world.set_mass_properties(pending, ExplicitMass{7});
        throw std::runtime_error("abort queued creation");
    };
    try
    {
        world.step(1. / 60);
        require(false, "Callback must throw");
    }
    catch (const std::runtime_error &)
    {
    }
    owner.tick = {};
    require(!world.contains_object(created), "Exception retires pending mass creation");
    owner.tick = [&] {
        world.set_mass_properties(h, ExplicitMass{9});
        world.reset();
    };
    world.step(1. / 60);
    owner.tick = {};
    require(!world.mass_properties(h), "Reset retires queued mass update");
}
void ccd()
{
    for (float scale : {16.f, 100.f})
    {
        Probe wall, bullet;
        wall.set_position({5 * scale, 0});
        wall.collider.shape = BoxShape{{}, {.05f * scale, 4 * scale}, 0};
        wall.definition.type = BodyType::Static;
        setup(bullet, scale);
        bullet.collider.shape = CircleShape{{}, .1f * scale};
        bullet.definition.bullet = true;
        bullet.definition.mass_properties = BodyMassData{1, {0, .03f * scale}, .02f * scale * scale};
        PhysicsWorld world(config(scale));
        wall.add(world);
        const auto h = bullet.add(world);
        world.set_velocity(h, {1000 * scale, 0});
        world.step(1. / 60);
        require(world.body_state(h)->position.x < 5.1f * scale, "Offset COM bullet does not tunnel through thin wall");
    }
}
void native_extents()
{
    // Isolate the bundled native cache fix using public Box2D APIs. No engine ID
    // is exposed: these are independent test-owned native objects.
    auto wd = b2DefaultWorldDef();
    const auto world = b2CreateWorld(&wd);
    auto bd = b2DefaultBodyDef();
    bd.type = b2_dynamicBody;
    const auto body = b2CreateBody(world, &bd);
    auto sd = b2DefaultShapeDef();
    sd.updateBodyMass = false;
    const b2Circle circle{{0, 0}, 2};
    b2CreateCircleShape(body, &sd, &circle);
    b2Body_SetMassData(body, {1, {0, 0}, 1});
    b2Body_SetTargetTransform(body, {{0, 0}, b2MakeRot(.1f)}, 1);
    // Native rotation/angle approximations need not reproduce the input scalar
    // exactly. A stale zero extent rejects this command and leaves speed zero.
    require(b2Body_GetAngularVelocity(body) > .09f && b2Body_GetAngularVelocity(body) < .11f,
            "Explicit native data initializes shape extents");
    b2Body_SetLinearVelocity(body, {});
    b2Body_SetAngularVelocity(body, 0);
    b2Body_SetMassData(body, {1, {10, 0}, 1});
    const auto q = b2MakeRot(.01f);
    b2Body_SetTargetTransform(body, {{10 - 10 * q.c, -10 * q.s}, q}, 1);
    require(b2Body_GetAngularVelocity(body) > .009f && b2Body_GetAngularVelocity(body) < .011f,
            "Changing native COM refreshes extent used by sleep threshold");
    b2DestroyWorld(world);
}
void compound_and_overflow()
{
    Probe body;
    PhysicsWorld world(config(1));
    Collider shapes[2];
    shapes[0].shape = BoxShape{{-2, 0}, {.5f, .5f}, 0};
    shapes[0].density = {1};
    shapes[1].shape = BoxShape{{2, 0}, {.5f, .5f}, 0};
    shapes[1].density = {3};
    const auto h = world.register_object(body, {.fixed_rotation = false}, shapes);
    const auto state = *world.body_state(h);
    require(near(state.mass, 4) && near(state.rotational_inertia, 12 + 4.f / 6),
            "Compound density inertia includes parallel-axis contributions");
    same(state.local_center, {1, 0}, "Compound density center is mass weighted");
    const auto id = world.collider_id(h, 1);
    require(world.set_collider_enabled(id, false), "Disable compound contributor");
    same(world.body_state(h)->local_center, {-2, 0}, "Density center tracks enabled shapes");
    require(world.set_collider_enabled(id, true), "Restore contributor");
    const auto properties = *world.mass_properties(h);
    require(!world.set_mass_properties(h, ExplicitMass{std::numeric_limits<float>::max()}),
            "Derived explicit inertia overflow rejected atomically");
    require(world.mass_properties(h) == properties && near(world.body_state(h)->mass, 4),
            "Overflow preserves original data");
    world.set_angular_velocity(h, std::numeric_limits<float>::max());
    require(!world.set_mass_properties(h, BodyMassData{1, {0, 100}, 1}),
            "COM velocity overflow rejected before mutation");
    require(world.mass_properties(h) == properties, "Overflowing velocity adjustment preserves configuration");
    world.set_angular_velocity(h, 0);
    BodyDefinition disabled;
    disabled.fixed_rotation = false;
    disabled.enabled = false;
    disabled.mass_properties = BodyMassData{2, {4, 5}, 6};
    Probe other;
    const auto d = world.register_object(other, disabled, shapes);
    require(d.is_valid() && !world.body_state(d)->enabled && world.body_state(d)->mass == 2,
            "Initially disabled registration applies custom mass");
}
} // namespace
int main()
{
    formulas();
    transitions();
    validation();
    deferred();
    ccd();
    native_extents();
    compound_and_overflow();
}
