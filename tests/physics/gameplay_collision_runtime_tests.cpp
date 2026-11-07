#define SDL_MAIN_HANDLED

#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include "tests/support/test_assertions.h"
#include "engine/core/game_object.h"

#include <iostream>
#include <memory>
#include <vector>

using elysia::tests::require;

namespace
{
class ColliderObject final
    : public elysia::core::GameObject
{
public:
    ColliderObject() : GameObject(elysia::core::DepthLayer::Character)
    {
        collider.shape = elysia::physics::BoxShape::from_rect({0, 0, 10, 10});
        collider.response = elysia::physics::CollisionResponse::Overlap;
    }
    std::span<elysia::physics::Collider> colliders() noexcept
    { return {&collider, 1}; }
    std::span<const elysia::physics::Collider> colliders() const noexcept
    { return {&collider, 1}; }
    elysia::physics::Collider collider;
    elysia::physics::ColliderId id = 0;
};

elysia::physics::PhysicsObjectHandle add_physics(elysia::physics::PhysicsWorld& w,ColliderObject& o){
 auto h=w.register_object(o,elysia::physics::BodyDefinition{},o.colliders());o.id=w.collider_id(h,0);return h;
}

class Listener final : public elysia::gameplay::collision::GameplayCollisionListener
{
public:
    void on_body_contact(
        const elysia::gameplay::collision::BodyContactEvent&) override
    { ++body_events; }
    void on_push_box_overlap(
        const elysia::gameplay::collision::PushBoxOverlapEvent&) override
    { ++push_events; }
    void on_hit_overlap(
        const elysia::gameplay::collision::HitOverlapEvent& event) override
    {
        ++hit_events;
        last_hurt_owner = event.hurt_box.owner;
        last_hit_box = event.hit_box;
    }
    void on_sensor_overlap(
        const elysia::gameplay::collision::SensorOverlapEvent& event) override
    {
        ++sensor_events;
        sensor_phases.push_back(event.phase);
        last_sensor = event.sensor;
        last_sensor_body = event.body;
    }
    int body_events = 0;
    int push_events = 0;
    int hit_events = 0;
    int sensor_events = 0;
    std::vector<elysia::physics::CollisionEventPhase> sensor_phases;
    elysia::gameplay::collision::ActorId last_hurt_owner = 0;
    elysia::gameplay::collision::HitBoxBinding last_hit_box{};
    elysia::gameplay::collision::ColliderBinding last_sensor{};
    elysia::gameplay::collision::ColliderBinding last_sensor_body{};
};

class ClearingListener final
    : public elysia::gameplay::collision::GameplayCollisionListener
{
public:
    explicit ClearingListener(
        elysia::gameplay::collision::GameplayCollisionRuntime& runtime)
        : _runtime(&runtime) {}

    void on_body_contact(
        const elysia::gameplay::collision::BodyContactEvent&) override
    {
        ++body_events;
        _runtime->clear();
    }

    void on_sensor_overlap(
        const elysia::gameplay::collision::SensorOverlapEvent&) override
    {
        ++sensor_events;
    }

    int body_events = 0;
    int sensor_events = 0;
private:
    elysia::gameplay::collision::GameplayCollisionRuntime* _runtime = nullptr;
};

struct IndependentHitFixture
{
    IndependentHitFixture() : runtime(world)
    {
        require(add_physics(world, hit).is_valid()
                && add_physics(world, hurt).is_valid(),
            "Independent attack colliders must register");
        move_hit({30, 0});
        require(runtime.bind_collider({
                    hurt.id, 20, elysia::gameplay::collision::teams::Enemy,
                    elysia::gameplay::collision::ColliderRole::HurtBox})
                && runtime.add_listener(listener),
            "Independent attack target and listener must bind");
    }

    elysia::gameplay::collision::HitBoxBinding binding() const
    {
        using namespace elysia::gameplay::collision;
        return {{hit.id, 30, teams::Player, ColliderRole::HitBox}, 10, 100, 1000};
    }

    void move_hit(elysia::core::Vector2 position)
    {
        require(world.teleport_object(*world.object_handle(hit), position),
            "Independent attack must teleport");
    }

    void step() { world.step(1.0 / 60.0); }

    elysia::physics::PhysicsWorld world;
    ColliderObject hit;
    ColliderObject hurt;
    elysia::gameplay::collision::GameplayCollisionRuntime runtime;
    Listener listener;
};

void test_independent_hit_lifetime_and_deduplication()
{
    using namespace elysia::gameplay::collision;
    IndependentHitFixture fixture;
    ColliderObject second_hurt;
    require(add_physics(fixture.world, second_hurt).is_valid()
            && fixture.runtime.bind_collider({
                second_hurt.id, 20, teams::Enemy, ColliderRole::HurtBox})
            && fixture.runtime.bind_hit_box(fixture.binding()),
        "An independent HitBox must bind without any ActorCollisionRig");
    fixture.move_hit({0, 0});
    fixture.step();
    require(fixture.listener.hit_events == 1
            && fixture.listener.last_hit_box.collider.owner == 30
            && fixture.listener.last_hit_box.instigator == 10
            && fixture.listener.last_hit_box.attack_instance == 100
            && fixture.listener.last_hit_box.attack_definition == 1000,
        "An unregistered source must hit once across multiple HurtBoxes and retain attribution");
    fixture.step();
    require(fixture.listener.hit_events == 1, "Independent attack Stay must not hit again");
    fixture.move_hit({30, 0});
    fixture.step();
    fixture.move_hit({0, 0});
    fixture.step();
    require(fixture.listener.hit_events == 1,
        "Re-entering multiple HurtBoxes must retain per-instance victim deduplication");

    fixture.move_hit({30, 0});
    fixture.step();
    fixture.runtime.end_attack_instance(100);
    auto next_attack = fixture.binding();
    next_attack.attack_instance = 101;
    require(fixture.runtime.bind_hit_box(next_attack), "The next attack instance must bind");
    fixture.move_hit({0, 0});
    fixture.step();
    require(fixture.listener.hit_events == 2
            && fixture.listener.last_hit_box.attack_instance == 101,
        "A new independent attack instance may hit the same victim again");

    IndependentHitFixture orphan;
    auto source = std::make_unique<ColliderObject>();
    const auto source_handle = add_physics(orphan.world, *source);
    require(source_handle.is_valid()
            && orphan.world.teleport_object(source_handle, {100, 0})
            && orphan.runtime.bind_actor({10, teams::Player, source->id})
            && orphan.runtime.bind_hit_box(orphan.binding()),
        "An independent projectile must launch while its source is registered");
    orphan.step();
    require(orphan.listener.hit_events == 0, "The projectile must not hit before arrival");
    require(orphan.runtime.unbind_actor(10) && orphan.world.unregister_object(source_handle),
        "The source must leave gameplay and physics before projectile arrival");
    source.reset();
    orphan.move_hit({0, 0});
    orphan.step();
    require(orphan.listener.hit_events == 1
            && orphan.listener.last_hit_box.instigator == 10
            && orphan.listener.last_hit_box.collider.owner == 30,
        "A projectile must survive source destruction and preserve the original source ID");
}

class SameTeamHostileResolver final
    : public elysia::gameplay::collision::TeamRelationResolver
{
public:
    elysia::gameplay::collision::TeamRelation relation(
        elysia::gameplay::collision::TeamId source,
        elysia::gameplay::collision::TeamId target) const noexcept override
    {
        using namespace elysia::gameplay::collision;
        return source == target ? TeamRelation::Hostile : TeamRelation::Friendly;
    }
};

void test_independent_hit_teams()
{
    using namespace elysia::gameplay::collision;
    for (const TeamId hit_team : {teams::Player, teams::Enemy, teams::Neutral})
    {
        IndependentHitFixture fixture;
        ColliderObject source;
        const auto source_handle = add_physics(fixture.world, source);
        // Give the source the opposite relation to the target from the attack.
        const TeamId source_team = hit_team == teams::Player ? teams::Enemy : teams::Player;
        require(source_handle.is_valid()
                && fixture.world.teleport_object(source_handle, {100, 0})
                && fixture.runtime.bind_actor({10, source_team, source.id}),
            "A source with a different team must register");
        auto attack = fixture.binding();
        attack.collider.team = hit_team;
        require(fixture.runtime.bind_hit_box(attack), "Team test attack must bind");
        fixture.move_hit({0, 0});
        fixture.step();
        require(fixture.listener.hit_events == (hit_team == teams::Player ? 1 : 0),
            "Hostile, friendly, and neutral filtering must use the HitBox team, not its source");
    }

    IndependentHitFixture changed_source;
    ColliderObject source;
    require(add_physics(changed_source.world, source).is_valid()
            && changed_source.world.teleport_object(*changed_source.world.object_handle(source), {100, 0})
            && changed_source.runtime.bind_actor({10, teams::Player, source.id})
            && changed_source.runtime.bind_hit_box(changed_source.binding())
            && changed_source.runtime.unbind_actor(10)
            && changed_source.runtime.bind_actor({10, teams::Enemy, source.id}),
        "A source must be able to change teams after launching an attack");
    changed_source.move_hit({0, 0});
    changed_source.step();
    require(changed_source.listener.hit_events == 1
            && changed_source.listener.last_hit_box.collider.team == teams::Player,
        "Changing the source team must not update an already bound attack's team");

    SameTeamHostileResolver resolver;
    IndependentHitFixture custom;
    custom.runtime.set_team_relation_resolver(&resolver);
    auto attack = custom.binding();
    attack.collider.team = teams::Enemy;
    require(custom.runtime.bind_hit_box(attack), "Custom relation attack must bind");
    custom.move_hit({0, 0});
    custom.step();
    require(custom.listener.hit_events == 1,
        "Independent attacks must use the custom resolver with their own team");
    custom.move_hit({30, 0});
    custom.step();
    custom.runtime.end_attack_instance(100);
    attack.collider.team = teams::Player;
    attack.attack_instance = 101;
    require(custom.runtime.bind_hit_box(attack), "Custom friendly attack must bind");
    custom.move_hit({0, 0});
    custom.step();
    require(custom.listener.hit_events == 1,
        "A custom non-hostile relation must suppress otherwise hostile independent hits");
    custom.runtime.set_team_relation_resolver(nullptr);
}

void test_independent_hit_cleanup_and_validation()
{
    using namespace elysia::gameplay::collision;
    for (int cleanup = 0; cleanup < 3; ++cleanup)
    {
        IndependentHitFixture fixture;
        require(fixture.runtime.bind_hit_box(fixture.binding()), "Cleanup attack must bind");
        fixture.step();
        if (cleanup == 0)
            require(fixture.runtime.unbind_actor(30), "Unregistered carrier owner must unbind its HitBox");
        else if (cleanup == 1)
            require(fixture.runtime.unbind_collider(fixture.hit.id), "HitBox collider must unbind");
        else
            fixture.runtime.end_attack_instance(100);
        require(fixture.world.contains_collider(fixture.hit.id)
                && !fixture.runtime.unbind_collider(fixture.hit.id),
            "Attack cleanup must remove the gameplay binding without destroying physics");
        fixture.move_hit({0, 0});
        fixture.step();
        require(fixture.listener.hit_events == 0, "A retired attack must not route new hits");
    }

    IndependentHitFixture fixture;
    const auto valid = fixture.binding();
    const auto rejects = [&](const HitBoxBinding& invalid)
    {
        require(!fixture.runtime.bind_hit_box(invalid), "Invalid independent HitBox must be rejected");
    };
    auto invalid = valid;
    invalid.collider.collider = elysia::physics::InvalidColliderId;
    rejects(invalid);
    ColliderObject removed;
    const auto removed_handle = add_physics(fixture.world, removed);
    require(removed_handle.is_valid() && fixture.world.unregister_object(removed_handle),
        "Stale collider validation probe must unregister");
    invalid.collider.collider = removed.id;
    rejects(invalid);
    invalid = valid; invalid.collider.owner = InvalidActorId; rejects(invalid);
    invalid = valid; invalid.collider.team = InvalidTeamId; rejects(invalid);
    invalid = valid; invalid.collider.role = ColliderRole::Body; rejects(invalid);
    invalid = valid; invalid.instigator = InvalidActorId; rejects(invalid);
    invalid = valid; invalid.attack_instance = InvalidAttackInstanceId; rejects(invalid);
    invalid = valid; invalid.attack_definition = InvalidAttackDefinitionId; rejects(invalid);
    require(fixture.runtime.bind_actor({30, teams::Player, fixture.hit.id})
            && !fixture.runtime.bind_hit_box(valid)
            && fixture.runtime.unbind_actor(30)
            && fixture.runtime.bind_hit_box(valid)
            && !fixture.runtime.bind_hit_box(valid)
            && !fixture.runtime.bind_actor({30, teams::Player, fixture.hit.id})
            && !fixture.runtime.bind_collider({fixture.hit.id, 30, teams::Player, ColliderRole::Body}),
        "Each collider must retain exactly one gameplay role in either binding order");
}
}

int main()
{
    using namespace elysia::physics;
    using namespace elysia::gameplay::collision;

    test_independent_hit_lifetime_and_deduplication();
    test_independent_hit_teams();
    test_independent_hit_cleanup_and_validation();

    PhysicsWorld world;
    ColliderObject hit_object;
    ColliderObject hurt_object;
    ColliderObject push_first;
    ColliderObject push_second;
    ColliderObject body_object;
    ColliderObject world_object;
    ColliderObject sensor_object;
    ColliderObject sensor_body_object;
    const auto add = [&](ColliderObject& object)
    {
        return add_physics(world, object);
    };
    require(add(hit_object).is_valid() && add(hurt_object).is_valid()
            && add(push_first).is_valid() && add(push_second).is_valid()
            && add(body_object).is_valid() && add(world_object).is_valid(),
        "Gameplay collision probes must register in the core world");
    require(add(sensor_object).is_valid() && add(sensor_body_object).is_valid(),
        "Sensor collision probes must register in the core world");
    world.teleport_object(*world.object_handle(hit_object), {0, 0});
    world.teleport_object(*world.object_handle(hurt_object), {0, 0});
    world.teleport_object(*world.object_handle(push_first), {30, 0});
    world.teleport_object(*world.object_handle(push_second), {30, 0});
    world.teleport_object(*world.object_handle(body_object), {60, 0});
    world.teleport_object(*world.object_handle(world_object), {60, 0});
    world.teleport_object(*world.object_handle(sensor_object), {90, 0});
    world.teleport_object(*world.object_handle(sensor_body_object), {90, 0});

    GameplayCollisionRuntime runtime(world);
    Listener listener;
    require(runtime.add_listener(listener), "Gameplay listeners must bind");
    const ActorCollisionRig player_rig{
        10, teams::Player, body_object.id, push_first.id,
        {}, {sensor_object.id}};
    const ActorCollisionRig enemy_rig{
        20, teams::Enemy, InvalidColliderId, push_second.id,
        {hurt_object.id}, {}};
    const ActorCollisionRig sensor_body_rig{
        40, teams::Player, sensor_body_object.id,
        InvalidColliderId, {}, {}};
    require(!runtime.bind_actor({99, InvalidTeamId, world_object.id})
            && !runtime.bind_actor({99, teams::Player})
            && !runtime.bind_collider({
                world_object.id, 99, teams::Player, ColliderRole::HitBox}),
        "Invalid Team, empty rigs, and generic HitBox bindings must be rejected");
    require(runtime.bind_actor(player_rig)
            && runtime.bind_actor(enemy_rig)
            && runtime.bind_actor(sensor_body_rig)
            && runtime.bind_hit_box({
                {hit_object.id, 10, teams::Player, ColliderRole::HitBox},
                10, 100, 1000}),
        "Gameplay bindings must commit valid core collider identities");

    world.step(1.0 / 60.0);
    require(listener.hit_events == 1 && listener.last_hurt_owner == 20,
        "Hostile HitBox/HurtBox Begin must route one hit");
    require(listener.push_events == 1,
        "PushBox overlap Begin must route in stable collider order");
    require(listener.body_events == 2,
        "Body contacts must route against both world and bound Sensor colliders");
    require(listener.sensor_events == 1
            && listener.sensor_phases.back() == CollisionEventPhase::Begin
            && listener.last_sensor.collider == sensor_object.id
            && listener.last_sensor_body.collider == sensor_body_object.id,
        "Sensor/Body overlap Begin must normalize binding order and ignore Team relation");
    world.step(1.0 / 60.0);
    require(listener.hit_events == 1,
        "Stay must not deal repeated damage for one attack instance");
    require(listener.push_events == 2 && listener.body_events == 4,
        "PushBox and Body routes must retain Stay events");
    require(listener.sensor_events == 2
            && listener.sensor_phases.back() == CollisionEventPhase::Stay,
        "Sensor overlaps must route Stay");
    world.teleport_object(*world.object_handle(sensor_body_object), {120, 0});
    world.step(1.0 / 60.0);
    require(listener.sensor_events == 3
            && listener.sensor_phases.back() == CollisionEventPhase::End,
        "Sensor overlaps must route End after natural separation");

    runtime.end_attack_instance(100);
    require(!runtime.unbind_collider(hit_object.id),
        "Ending an attack must retire its HitBox binding");
    require(runtime.team_relation(teams::Player, teams::Player) == TeamRelation::Friendly
            && runtime.team_relation(teams::Player, teams::Enemy) == TeamRelation::Hostile
            && runtime.team_relation(teams::Player, teams::Neutral) == TeamRelation::Neutral,
        "The built-in Team resolver must implement Friendly/Hostile/Neutral defaults");
    require(runtime.unbind_actor(10) && runtime.bind_actor(player_rig),
        "Actor unbind must remove the complete rig so the ActorId can bind again");
    require(runtime.unbind_collider(body_object.id)
            && runtime.unbind_collider(push_first.id)
            && runtime.unbind_collider(sensor_object.id)
            && runtime.bind_actor(player_rig),
        "Individual collider unbinds must maintain and eventually retire their rig");

    {
        PhysicsWorld snapshot_world;
        ColliderObject sensor;
        ColliderObject body;
        require(add_physics(snapshot_world, sensor).is_valid()
                && add_physics(snapshot_world, body).is_valid(),
            "Gameplay callback snapshot probes must register");
        GameplayCollisionRuntime snapshot_runtime(snapshot_world);
        require(snapshot_runtime.bind_actor({
                    1, teams::Player, InvalidColliderId, InvalidColliderId,
                    {}, {sensor.id}})
                && snapshot_runtime.bind_actor({
                    2, teams::Enemy, body.id}),
            "Gameplay callback snapshot rigs must bind");
        ClearingListener clearing(snapshot_runtime);
        require(snapshot_runtime.add_listener(clearing),
            "Gameplay callback snapshot listener must bind");
        snapshot_world.step(1.0 / 60.0);
        require(clearing.body_events == 1 && clearing.sensor_events == 1,
            "Clearing bindings during Body routing must not invalidate the current Sensor event");
    }

    std::cout << "gameplay collision runtime tests passed\n";
    return 0;
}
