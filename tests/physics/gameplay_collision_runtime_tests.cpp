#define SDL_MAIN_HANDLED

#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include "tests/support/test_assertions.h"

#include <iostream>
#include <vector>

using elysia::tests::require;

namespace
{
class ColliderObject final
    : public elysia::core::GameObject
    , public elysia::physics::ColliderProvider
{
public:
    ColliderObject() : GameObject(elysia::core::DepthLayer::Character)
    {
        collider.shape = elysia::physics::AabbShape{{0, 0, 10, 10}};
        collider.response = elysia::physics::CollisionResponse::Overlap;
    }
    std::span<elysia::physics::Collider> colliders() noexcept override
    { return {&collider, 1}; }
    std::span<const elysia::physics::Collider> colliders() const noexcept override
    { return {&collider, 1}; }
    elysia::physics::Collider collider;
};

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
}

int main()
{
    using namespace elysia::physics;
    using namespace elysia::gameplay::collision;

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
        return world.register_object(object, nullptr, &object);
    };
    require(add(hit_object).is_valid() && add(hurt_object).is_valid()
            && add(push_first).is_valid() && add(push_second).is_valid()
            && add(body_object).is_valid() && add(world_object).is_valid(),
        "Gameplay collision probes must register in the core world");
    require(add(sensor_object).is_valid() && add(sensor_body_object).is_valid(),
        "Sensor collision probes must register in the core world");
    hit_object.set_position({0, 0});
    hurt_object.set_position({0, 0});
    push_first.set_position({30, 0});
    push_second.set_position({30, 0});
    body_object.set_position({60, 0});
    world_object.set_position({60, 0});
    sensor_object.set_position({90, 0});
    sensor_body_object.set_position({90, 0});

    GameplayCollisionRuntime runtime(world);
    Listener listener;
    require(runtime.add_listener(listener), "Gameplay listeners must bind");
    const ActorCollisionRig player_rig{
        10, teams::Player, body_object.collider.id, push_first.collider.id,
        {}, {sensor_object.collider.id}};
    const ActorCollisionRig enemy_rig{
        20, teams::Enemy, InvalidColliderId, push_second.collider.id,
        {hurt_object.collider.id}, {}};
    const ActorCollisionRig sensor_body_rig{
        40, teams::Player, sensor_body_object.collider.id,
        InvalidColliderId, {}, {}};
    require(!runtime.bind_actor({99, InvalidTeamId, world_object.collider.id})
            && !runtime.bind_actor({99, teams::Player})
            && !runtime.bind_collider({
                world_object.collider.id, 99, teams::Player, ColliderRole::HitBox}),
        "Invalid Team, empty rigs, and generic HitBox bindings must be rejected");
    require(runtime.bind_actor(player_rig)
            && runtime.bind_actor(enemy_rig)
            && runtime.bind_actor(sensor_body_rig)
            && runtime.bind_hit_box({
                {hit_object.collider.id, 10, teams::Enemy, ColliderRole::HitBox},
                10, 100, 1000}),
        "Gameplay bindings must commit valid core collider identities");

    (void)world.advance(1.0 / 60.0);
    require(listener.hit_events == 1 && listener.last_hurt_owner == 20,
        "Hostile HitBox/HurtBox Begin must route one hit");
    require(listener.push_events == 1,
        "PushBox overlap Begin must route in stable collider order");
    require(listener.body_events == 2,
        "Body contacts must route against both world and bound Sensor colliders");
    require(listener.sensor_events == 1
            && listener.sensor_phases.back() == CollisionEventPhase::Begin
            && listener.last_sensor.collider == sensor_object.collider.id
            && listener.last_sensor_body.collider == sensor_body_object.collider.id,
        "Sensor/Body overlap Begin must normalize binding order and ignore Team relation");
    (void)world.advance(1.0 / 60.0);
    require(listener.hit_events == 1,
        "Stay must not deal repeated damage for one attack instance");
    require(listener.push_events == 2 && listener.body_events == 4,
        "PushBox and Body routes must retain Stay events");
    require(listener.sensor_events == 2
            && listener.sensor_phases.back() == CollisionEventPhase::Stay,
        "Sensor overlaps must route Stay");
    sensor_body_object.set_position({120, 0});
    (void)world.advance(1.0 / 60.0);
    require(listener.sensor_events == 3
            && listener.sensor_phases.back() == CollisionEventPhase::End,
        "Sensor overlaps must route End after natural separation");

    runtime.end_attack_instance(100);
    require(!runtime.unbind_collider(hit_object.collider.id),
        "Ending an attack must retire its HitBox binding");
    require(runtime.team_relation(teams::Player, teams::Player) == TeamRelation::Friendly
            && runtime.team_relation(teams::Player, teams::Enemy) == TeamRelation::Hostile
            && runtime.team_relation(teams::Player, teams::Neutral) == TeamRelation::Neutral,
        "The built-in Team resolver must implement Friendly/Hostile/Neutral defaults");
    require(runtime.unbind_actor(10) && runtime.bind_actor(player_rig),
        "Actor unbind must remove the complete rig so the ActorId can bind again");
    require(runtime.unbind_collider(body_object.collider.id)
            && runtime.unbind_collider(push_first.collider.id)
            && runtime.unbind_collider(sensor_object.collider.id)
            && runtime.bind_actor(player_rig),
        "Individual collider unbinds must maintain and eventually retire their rig");

    {
        PhysicsWorld snapshot_world;
        ColliderObject sensor;
        ColliderObject body;
        require(snapshot_world.register_object(sensor, nullptr, &sensor).is_valid()
                && snapshot_world.register_object(body, nullptr, &body).is_valid(),
            "Gameplay callback snapshot probes must register");
        GameplayCollisionRuntime snapshot_runtime(snapshot_world);
        require(snapshot_runtime.bind_actor({
                    1, teams::Player, InvalidColliderId, InvalidColliderId,
                    {}, {sensor.collider.id}})
                && snapshot_runtime.bind_actor({
                    2, teams::Enemy, body.collider.id}),
            "Gameplay callback snapshot rigs must bind");
        ClearingListener clearing(snapshot_runtime);
        require(snapshot_runtime.add_listener(clearing),
            "Gameplay callback snapshot listener must bind");
        (void)snapshot_world.advance(1.0 / 60.0);
        require(clearing.body_events == 1 && clearing.sensor_events == 1,
            "Clearing bindings during Body routing must not invalidate the current Sensor event");
    }

    std::cout << "gameplay collision runtime tests passed\n";
    return 0;
}
