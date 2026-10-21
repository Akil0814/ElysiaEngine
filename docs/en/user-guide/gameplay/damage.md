# From Attack Hits to Receiving Damage

Paired actor/scene examples connect hits, damage, death, and reference cleanup. Physical contact, gameplay events, and health changes belong to different layers.

This combines [gameplay collision](collision.md), [physics](../systems/physics.md), and [object lifetime](../scene/game-objects.md): enable HitBox → hit HurtBox → validate business state → deduct health → presentation/death → end attack.

For binding APIs alone, read collision first. This page assumes scene/physics object creation and helps implement a complete attack flow.

The engine owns contact, team filtering, instance deduplication, and notification. Games own health, attack power, invulnerability, death, and rewards. This adds no engine damage interface and depends on no actor animations/assets.

## Distinguish Three Identifiers

| Identifier | Example purpose | Lifetime |
| --- | --- | --- |
| ActorId | Find attacker/victim business objects | Stable while alive; do not reuse before old events are handled |
| AttackDefinitionId | Attack type; 1 means ordinary 25-damage attack here | Shared across attacks |
| AttackInstanceId | This swing | Shared by one attack's HitBoxes; new value next attack |

All IDs must be nonzero. ActorId is not an address; AttackInstanceId is not an animation frame. Concurrent attackers need a common allocator avoiding instance collisions in the current runtime.

## Preparing Damageable Objects

Place the next two C++ blocks in order in one game scene header to form a registrable scene, not a standalone application. Each object owns a HurtBox and initially disabled HitBox with separate filter bits. Both are Overlap and push neither actor.

```cpp
#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/core/render/render_command.h"
#include "engine/tools/logger.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace damage_example
{
namespace gc = elysia::gameplay::collision;
namespace ph = elysia::physics;

class Fighter final : public elysia::core::GameObject,
                      public ph::PhysicsParticipant
{
public:
    Fighter(gc::ActorId actor, gc::TeamId faction, elysia::core::Vector2 position)
        : GameObject(elysia::core::DepthLayer::Character), id(actor), team(faction)
    {
        set_world_rect({position, {32.0f, 32.0f}});
        // Index 0 is hurt, 1 attack; filter bits must match both ways.
        _colliders[0].shape = ph::AabbShape{{0, 0, 32, 32}};
        _colliders[0].filter = {.category = 2u, .mask = 1u};
        _colliders[0].response = ph::CollisionResponse::Overlap;
        _colliders[1].shape = ph::AabbShape{{24, 0, 40, 32}};
        _colliders[1].filter = {.category = 1u, .mask = 2u};
        _colliders[1].response = ph::CollisionResponse::Overlap;
        // Register the disabled attack area first; enable when a fixed-step attack request arrives.
        _colliders[1].enabled = false;
    }

    ph::BodyDefinition body_definition() const override
    {
        ph::BodyDefinition body;
        body.type = ph::BodyType::Dynamic;
        body.mass_policy = ph::MassPolicy::ExplicitMass;
        body.mass = 1.0f;
        body.gravity_scale = 0.0f;
        return body;
    }

    std::span<const ph::Collider> collider_definitions() const override
    {
        return _colliders;
    }

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override
    {
        elysia::core::RenderCommand command;
        command.type = elysia::core::RenderCommandType::FillRect;
        command.command_rect = render_rect();
        command.color = team == gc::teams::Player
            ? elysia::core::Color{80, 160, 255} : elysia::core::Color{255, 100, 100};
        out.push_back(command);
    }

    void reset() noexcept override
    {
        GameObject::reset();
        // The base restores basic state only; games reset health and invulnerability.
        hp = 40;
        invulnerable = false;
    }

    gc::ActorId id;
    gc::TeamId team;
    int hp = 40;
    bool invulnerable = false;

private:
    std::array<ph::Collider, 2> _colliders{};
};
} // namespace damage_example
```

Valid ColliderId values exist only after scene registration. Never register one `Collider` as both HurtBox and HitBox. Two static bodies are unsuitable for this overlap example, so it explicitly uses Dynamic. Production games choose body types for their movement models.

## Starting Attacks, Receiving Damage, and Cleanup

Entry automatically queues one attack. Later input/commands can call `queue_attack()`; no universal attack key is specified. Positions overlap the attack with the victim HurtBox. First hit lowers health from 40 to 15; the second valid attack kills.

```cpp
namespace damage_example
{
class DamageScene final : public elysia::gameplay::GameplayScene,
                          private gc::GameplayCollisionListener
{
public:
    DamageScene()
        : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
            .physics = ph::PhysicsWorldConfig{},
            .gameplay_collision = true,
            .camera = elysia::scene::CameraSceneConfig{}})
    {
    }

    ~DamageScene() override
    {
        teardown();
    }

    void on_enter(const elysia::scene::ScenePayload&) override
    {
        // Reuse retains live objects; restore this scene's gameplay bindings on entry.
        if (!_attacker)
        {
            _attacker = create_and_add_object<Fighter>(
                1, gc::teams::Player, elysia::core::Vector2{100, 100});
        }
        if (!_defender)
        {
            _defender = create_and_add_object<Fighter>(
                2, gc::teams::Enemy, elysia::core::Vector2{140, 100});
        }
        if (!_attacker || !_defender || !_attacker->physics_state() ||
            !_defender->physics_state() || !bind_fighter(*_attacker) ||
            !bind_fighter(*_defender))
        {
            teardown();
            throw std::runtime_error("Damage example actor binding failed.");
        }
        _listening = collision_runtime().add_listener(*this);
        if (!_listening)
        {
            teardown();
            throw std::runtime_error("Damage example listener binding failed.");
        }
        // Explicitly frame both objects when no follow strategy is used.
        camera_runtime().set_center(elysia::camera::CameraSlot::Main, {140, 116});
        resume();
        _queued = true;
    }

    void on_exit() override
    {
        teardown();
    }

    void on_reset() override
    {
        teardown();
        if (_attacker)
            _attacker->reset();
        if (_defender)
            _defender->reset();
    }

    void queue_attack()
    {
        // Input records intent only; fixed steps manage collider enablement.
        if (_listening)
            _queued = true;
    }

    void on_after_update(double) override
    {
        // Physics callbacks finished; reclamation is pending. Keep feedback values, not victim pointers.
        for (const Feedback& feedback : _feedback)
        {
            ELYSIA_LOG_INFO("combat", "Actor " << feedback.target
                << " took " << feedback.damage << ", hp=" << feedback.remaining_hp);
            // Notify UI or call audio/effect services here.
        }
        _feedback.clear();
    }

protected:
    void on_game_fixed_update(std::uint64_t, double delta) override
    {
        if (_active_attack)
        {
            _remaining -= delta;
            if (_remaining <= 0.0)
                finish_attack();
            return; // Do not reopen this step; physics must observe at least one disabled step.
        }
        if (!_queued)
            return;
        _queued = false;
        if (!_attacker || _attacker->is_destroyed() || _attacker->hp <= 0)
            return;
        if (_next_attack == 0)
            throw std::overflow_error("Attack id exhausted.");

        // Allocate a new swing instance; finish_attack rolls back partial bindings on failure.
        _active_attack = _next_attack++;
        const ph::ColliderId hit = _attacker->physics_collider(1);
        const bool bound = collision_runtime().bind_hit_box({
            .collider = {hit, _attacker->id, _attacker->team, gc::ColliderRole::HitBox},
            .instigator = _attacker->id,
            .attack_instance = _active_attack,
            .attack_definition = 1});
        if (!bound || !physics_world().set_collider_enabled(hit, true))
        {
            finish_attack();
            ELYSIA_LOG_ERROR("combat", "Attack could not be enabled.");
            return;
        }
        _remaining = 0.15; // Seconds; decrement starting next fixed step.
    }

    void on_control_target_removing(elysia::core::SceneObject& object) override
    {
        // Detach gameplay while the object is valid, then clear scene borrows.
        if (&object == _attacker)
        {
            finish_attack();
            (void)collision_runtime().unbind_actor(_attacker->id);
            _attacker = nullptr;
        }
        if (&object == _defender)
        {
            (void)collision_runtime().unbind_actor(_defender->id);
            _defender = nullptr;
        }
    }

private:
    struct Feedback
    {
        gc::ActorId target;
        int damage;
        int remaining_hp;
    };

    bool bind_fighter(Fighter& actor)
    {
        gc::ActorCollisionRig rig;
        rig.owner = actor.id;
        rig.team = actor.team;
        rig.hurt_boxes = {actor.physics_collider(0)};
        return collision_runtime().bind_actor(rig);
    }

    Fighter* find_actor(gc::ActorId id) const
    {
        if (_attacker && _attacker->id == id)
            return _attacker;
        if (_defender && _defender->id == id)
            return _defender;
        return nullptr;
    }

    void on_hit_overlap(const gc::HitOverlapEvent& event) override
    {
        if (event.phase != ph::CollisionEventPhase::Begin ||
            !_active_attack || event.hit_box.attack_instance != _active_attack ||
            event.hit_box.attack_definition != 1)
            return;

        // Physical hits still require game liveness/invulnerability checks before damage.
        Fighter* source = find_actor(event.hit_box.instigator);
        Fighter* target = find_actor(event.hurt_box.owner);
        if (!source || !target || source == target || source->is_destroyed() ||
            target->is_destroyed() || source->hp <= 0 || target->hp <= 0 ||
            target->invulnerable)
            return;

        constexpr int kDamage = 25;
        target->hp = std::max(0, target->hp - kDamage);
        _feedback.push_back({target->id, kDamage, target->hp});
        if (target->hp == 0)
            target->destroy(); // Mark only; do not delete.
    }

    void finish_attack()
    {
        // Disable physics and clear gameplay deduplication separately; both are required at attack end.
        if (_attacker && physics_world().contains_collider(_attacker->physics_collider(1)))
            (void)physics_world().set_collider_enabled(_attacker->physics_collider(1), false);
        if (_active_attack)
            collision_runtime().end_attack_instance(_active_attack);
        _active_attack = 0;
        _remaining = 0.0;
    }

    void teardown()
    {
        // Exit/reset/destruction may all arrive here; repeated detachment is allowed.
        // These bools indicate prior bindings; already detached is not another failure.
        finish_attack();
        if (_listening)
            (void)collision_runtime().remove_listener(*this);
        _listening = false;
        if (_attacker)
            (void)collision_runtime().unbind_actor(_attacker->id);
        if (_defender)
            (void)collision_runtime().unbind_actor(_defender->id);
        _queued = false;
        _feedback.clear();
    }

private:
    // The scene owns objects; removal notifications clear these two borrowed pointers.
    Fighter* _attacker = nullptr;
    Fighter* _defender = nullptr;
    bool _listening = false;
    bool _queued = false;
    gc::AttackInstanceId _active_attack = 0;
    gc::AttackInstanceId _next_attack = 1;
    double _remaining = 0.0; // Remaining attack window seconds, decremented only in fixed steps.
    std::vector<Feedback> _feedback;
};
} // namespace damage_example
```

Choose an unused game SceneKey in module `register_scenes`, call `register_game_scene<damage_example::DamageScene>(key)`, and enter after resource initialization. No controllers are used, so collision alone needs no control session. Add player sessions/bindings through [controller guide](control.md).

The example uses its own `collision_runtime()` for reliable exit cleanup. Other code may use `elysia::gameplay::collision::GameplayCollisionService::instance()` for active runtime; do not assume the global service still binds the old scene on exit.

## Why an Event Alone Is Not Enough to Deduct Health

- **Physical/gameplay filters both apply**: category/mask must match in both directions, with registered actors, HitBox/HurtBox, and hostile teams. instigator must be registered via bind_actor; an attack `Collider` alone is insufficient.
- **Hits need not cause damage**: games decide invulnerability, death, blocking. Invulnerability rejects damage here, but the engine already deduplicates that instance/victim, so expiry does not automatically replay the hit.
- **Only Begin notifies attacks**: replacing bindings/instance IDs while still overlapping need not create Begin. This example lets physics actually observe a disabled HitBox between attacks; disabling/re-enabling before one step does not guarantee another hit.
- **Multiple colliders deduplicate**: one attack's HitBoxes share an instance; one victim's HurtBoxes share ActorId. An instance can hit multiple actors once each. Different listeners still receive the same event; assign one damage owner.
- **Fast attacks**: Overlap does not guarantee continuous hits. For sweeps, games feed query results into the same damage rules and maintain deduplication; queries do not automatically emit `HitOverlapEvent`.

## Cancellation, Death, and Scene Exit

On completion/interruption, disable the physical HitBox and call `end_attack_instance`. The latter only clears semantic bindings/deduplication, without destroying/disabling physics. A window containing no actual step may produce no collision.

Pause freezes fixed steps/attack windows here. If pause should cancel immediately, invoke your game cancellation next to pause; do not override `GameplayScene`'s final pause callback.

Damage callbacks change business state and record value-type presentation requests only. After `destroy()` marks death, later same-frame events may still arrive; check liveness again. Spawn drops/sounds/effects in safe phases after callbacks. Do not add objects during traversal or capture dead object pointers in delayed callbacks.

Detach semantic associations/borrowed pointers in `on_control_target_removing`; do not override final `on_scene_object_removing`. Exit/destruction detach listeners, which must outlive current dispatch. Exit preserves survivors here; reset restores health; Recreate makes new instances. Deferred production events should use generation-bearing actor IDs or other verifiable handles to prevent old events hitting new actors.

## Validating Your Integration

1. Hostile overlapping attacks: one damage per attack; new instances with effective disable/enable cycles hit again.
2. Same team, mismatched mask, or invulnerability: no damage; invulnerability expiry does not replay the same hit.
3. Multiple HurtBoxes on one victim: one damage; separate victims hit independently.
4. Cancellation, death, exit/reentry: no stale attack bindings, duplicate listeners, or dangling pointers.
5. Low frame rates and zero/multiple fixed steps: no per-render duplicate damage; presentation queues do not depend on reclaimed objects.

These are runtime integration suggestions; syntax checking this example does not replace actual project execution.

- [Gameplay collision](collision.md), [GameplayScene](scene.md), [physics](../systems/physics.md)
- [Animation/effects](../systems/animation-and-effects.md), [audio](../systems/audio.md), [back to user guide](../README.md)
