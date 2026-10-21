# Gameplay Collision

This guide connects physical contacts to game actors and attacks, focusing on binding order, hit deduplication, and listener lifetime. Damage/inventory rules remain game decisions.

Physics handles bodies, geometry, and contacts. Gameplay collision associates actual ColliderId values with actors, teams, and Body/PushBox/HurtBox/HitBox/Sensor semantics, then publishes events. It does not deduct health, play animation, or implement inventory automatically.

See [from attack hits to receiving damage](damage.md) for a complete composition with attack windows, health changes, death, and cleanup.

## Prerequisites and Integration

For standard scene integration, provide `physics` and `gameplay_collision=true` in `GameplaySceneFeatures`. Default `GameplayScene` has no collision runtime; `physics` alone does not create it. Enabling gameplay collision without `physics` throws `std::invalid_argument` during construction.

This complete type illustrates only those conditions:

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"

class CollisionExampleScene final : public elysia::gameplay::GameplayScene
{
public:
    CollisionExampleScene()
        : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
            .physics = elysia::physics::PhysicsWorldConfig{},
            .gameplay_collision = true})
    {
    }

    // This enables runtime only; helpers below bind existing colliders.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }
};
```

The manager activates runtime only if created. When disabled, `has_collision_runtime()` is `false`; derived scenes can query `try_collision_runtime()`, while forced `collision_runtime()` throws `std::logic_error`. Game code accesses the active runtime through `elysia::gameplay::collision::GameplayCollisionService::instance()`, or a derived scene uses its own `collision_runtime()`.

First create/register `physics` objects using the [physics guide](../systems/physics.md) and obtain real ColliderId values. Allocate nonzero ActorId/TeamId and bind colliders to actors. Semantic binding creates no physical shape and does not replace physical layer/mask filtering.

## Binding Actors

Place this helper in game code. `body` and `hurt` are registered colliders in the active scene's world; `actor` is a game-assigned nonzero identifier.

```cpp
#include "engine/gameplay/collision/gameplay_collision_service.h"

bool bind_player(elysia::gameplay::collision::ActorId actor,
                 elysia::physics::ColliderId body,
                 elysia::physics::ColliderId hurt)
{
    using namespace elysia::gameplay::collision;
    GameplayCollisionService* service = GameplayCollisionService::instance();
    if (!service->has_active_runtime())
        return false;
    // IDs refer to registered colliders; add actor ownership and gameplay roles here.
    ActorCollisionRig rig;
    rig.owner = actor;
    rig.team = teams::Player;
    rig.body = body;
    rig.hurt_boxes = {hurt};
    return service->bind_actor(rig);
}
```

If `false`, stop dependent operations and check IDs, world membership, and binding requirements. `body`/`hurt` should be separate colliders with distinct roles; one ColliderId cannot represent both Body and HurtBox. Actors may also contain PushBox/Sensor; bind independent colliders with `bind_collider(ColliderBinding)`.

## Attacks and Deduplication

Call this when an attack starts. hit is a registered Overlap collider; the game supplies owner, instance, and definition IDs. Physical filters must allow contact with target HurtBoxes.

```cpp
#include "engine/gameplay/collision/gameplay_collision_service.h"

bool begin_hit(elysia::physics::ColliderId hit,
               elysia::gameplay::collision::ActorId owner,
               elysia::gameplay::collision::AttackInstanceId instance,
               elysia::gameplay::collision::AttackDefinitionId definition)
{
    using namespace elysia::gameplay::collision;
    GameplayCollisionService* service = GameplayCollisionService::instance();
    if (!service->has_active_runtime())
        return false;
    // instance identifies this swing; definition identifies its type. They are not interchangeable.
    return service->bind_hit_box({
        .collider = {hit, owner, teams::Player, ColliderRole::HitBox},
        .instigator = owner,
        .attack_instance = instance,
        .attack_definition = definition
    });
}
```

Do not mark failed attacks bound. Save instance after success and call end_attack_instance on completion/cancellation. HitBox hits route only during Begin against hostile HurtBoxes; games still check liveness and calculate damage.

Bind attack colliders through `bind_hit_box(HitBoxBinding)` with `collider.role = ColliderRole::HitBox` and valid owner, team, `instigator`, attack_instance, attack_definition. Instances represent one attack; definitions represent attack types. Never permanently share one instance across all attacks.

Hits deduplicate by attack instance + victim `actor`. Multiple HurtBoxes do not duplicate events for one instance. `end_attack_instance(id)` clears deduplication and that instance's HitBox semantic bindings without destroying physical colliders. Use a new instance next attack.

### Team Relations and Instigators

Default relation checks in order: same valid team is `Friendly` (including two `teams::Neutral`); otherwise any `Neutral`/invalid team gives `Neutral`; different valid teams are `Hostile`.

Hit routing looks up `instigator` registered through `bind_actor` and uses its team. A nonzero source ID or HitBox team alone is insufficient.

For custom relations, call `collision_runtime().set_team_relation_resolver(...)` in a derived scene and query `team_relation(...)`. These belong to `GameplayCollisionRuntime`, not the global service. The resolver is borrowed and must outlive the association; detach with `nullptr` before releasing it.

## Listening and Release

Derive `GameplayCollisionListener` and override `on_body_contact`, `on_push_box_overlap`, `on_hit_overlap`, or `on_sensor_overlap` as needed. Register with `add_listener(listener)`, check results, and update rules from event values. Registration transfers no ownership.

Call `remove_listener` before exit/destruction. Global services may already be unbound during scene exit; derived `on_exit()` should clean listeners/bindings through its own `collision_runtime()`. Listener changes during dispatch are deferred; do not immediately free listeners that may remain in the current snapshot.

Use `unbind_actor` for removed actors and `unbind_collider` for independent colliders. Physics owners still manage physical lifetime. End events may arrive after unbinding; do not blindly dereference destroyed objects from event `actor` IDs. On match reset, clean runtime bindings, listeners, and attacks under game rules, then restore necessary registrations.

## Common Misuse

- Global service calls without active runtime do not create one.
- HitBox/HurtBox names without registered physical colliders produce no contacts.
- Repeated attack effects require instance rules and game validity checks.
- Freeing listeners without unregistering or expecting unbinding to destroy `physics` confuses separate lifetimes.

## References

- [Service/API](../../../../engine/gameplay/collision/gameplay_collision_service.h), [binding types](../../../../engine/gameplay/collision/gameplay_collision_types.h)
- [Events](../../../../engine/gameplay/collision/gameplay_collision_events.h), [runtime reference](../../architecture/subsystems/physics/07-gameplay-collision-runtime.md)
- [GameplayScene](scene.md), [back to user guide](../README.md)
