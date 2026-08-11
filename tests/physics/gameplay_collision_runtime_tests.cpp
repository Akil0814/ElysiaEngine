#define SDL_MAIN_HANDLED

#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include "tests/support/test_assertions.h"

#include <iostream>

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
    int body_events = 0;
    int push_events = 0;
    int hit_events = 0;
    elysia::gameplay::collision::ActorId last_hurt_owner = 0;
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
    const auto add = [&](ColliderObject& object)
    {
        return world.register_object(object, nullptr, &object);
    };
    require(add(hit_object).is_valid() && add(hurt_object).is_valid()
            && add(push_first).is_valid() && add(push_second).is_valid()
            && add(body_object).is_valid() && add(world_object).is_valid(),
        "Gameplay collision probes must register in the core world");
    hit_object.set_position({0, 0});
    hurt_object.set_position({0, 0});
    push_first.set_position({30, 0});
    push_second.set_position({30, 0});
    body_object.set_position({60, 0});
    world_object.set_position({60, 0});

    GameplayCollisionRuntime runtime(world);
    Listener listener;
    require(runtime.add_listener(listener), "Gameplay listeners must bind");
    require(runtime.bind_hit_box({
                {hit_object.collider.id, 10, teams::Player, ColliderRole::HitBox},
                10, 100, 1000})
            && runtime.bind_collider(
                {hurt_object.collider.id, 20, teams::Enemy, ColliderRole::HurtBox})
            && runtime.bind_collider(
                {push_first.collider.id, 10, teams::Player, ColliderRole::PushBox})
            && runtime.bind_collider(
                {push_second.collider.id, 20, teams::Enemy, ColliderRole::PushBox})
            && runtime.bind_collider(
                {body_object.collider.id, 10, teams::Player, ColliderRole::Body}),
        "Gameplay bindings must commit valid core collider identities");

    (void)world.advance(1.0 / 60.0);
    require(listener.hit_events == 1 && listener.last_hurt_owner == 20,
        "Hostile HitBox/HurtBox Begin must route one hit");
    require(listener.push_events == 1,
        "PushBox overlap Begin must route in stable collider order");
    require(listener.body_events == 1,
        "Body contacts must route against an unbound world collider");
    (void)world.advance(1.0 / 60.0);
    require(listener.hit_events == 1,
        "Stay must not deal repeated damage for one attack instance");
    require(listener.push_events == 2 && listener.body_events == 2,
        "PushBox and Body routes must retain Stay events");

    runtime.end_attack_instance(100);
    require(!runtime.unbind_collider(hit_object.collider.id),
        "Ending an attack must retire its HitBox binding");
    require(runtime.team_relation(teams::Player, teams::Player) == TeamRelation::Friendly
            && runtime.team_relation(teams::Player, teams::Enemy) == TeamRelation::Hostile
            && runtime.team_relation(teams::Player, teams::Neutral) == TeamRelation::Neutral,
        "The built-in Team resolver must implement Friendly/Hostile/Neutral defaults");

    std::cout << "gameplay collision runtime tests passed\n";
    return 0;
}
