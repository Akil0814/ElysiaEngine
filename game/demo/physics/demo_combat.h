#pragma once

#include "../../../engine/gameplay/collision/gameplay_collision_listener.h"
#include "../../../engine/gameplay/collision/gameplay_collision_runtime.h"
#include "../../../engine/physics/physics_world.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace example::demo::physics
{
class BlockCombatActor;

class Health final
{
public:
    explicit Health(int maximum = 1) noexcept;
    [[nodiscard]] int maximum() const noexcept { return _maximum; }
    [[nodiscard]] int current() const noexcept { return _current; }
    [[nodiscard]] bool alive() const noexcept { return _current > 0; }
    [[nodiscard]] int apply_damage(int amount) noexcept;
    void reset() noexcept { _current = _maximum; }
private:
    int _maximum = 1;
    int _current = 1;
};

struct DamageDefinition
{
    elysia::gameplay::collision::AttackDefinitionId id =
        elysia::gameplay::collision::InvalidAttackDefinitionId;
    int damage = 0;
    elysia::core::Vector2 knockback{};
};

struct DamageRequest
{
    elysia::gameplay::collision::ActorId source =
        elysia::gameplay::collision::InvalidActorId;
    elysia::gameplay::collision::ActorId target =
        elysia::gameplay::collision::InvalidActorId;
    elysia::gameplay::collision::AttackDefinitionId definition =
        elysia::gameplay::collision::InvalidAttackDefinitionId;
    elysia::core::Vector2 hit_point{};
};

struct DamageResult
{
    int applied = 0;
    int remaining = 0;
    bool killed = false;
};

class IDamageableActor
{
public:
    virtual ~IDamageableActor() = default;
    [[nodiscard]] virtual elysia::gameplay::collision::ActorId actor_id() const noexcept = 0;
    [[nodiscard]] virtual const Health& health() const noexcept = 0;
    virtual DamageResult apply_damage(const DamageRequest&, const DamageDefinition&) = 0;
};

class DemoCombatSession final
    : public elysia::gameplay::collision::GameplayCollisionListener
{
public:
    using DeathCallback = std::function<void(BlockCombatActor&)>;
    using DamageCallback = std::function<void(const BlockCombatActor&, const DamageResult&)>;

    DemoCombatSession(
        elysia::physics::PhysicsWorld& world,
        elysia::gameplay::collision::GameplayCollisionRuntime& runtime);
    ~DemoCombatSession() override;

    DemoCombatSession(const DemoCombatSession&) = delete;
    DemoCombatSession& operator=(const DemoCombatSession&) = delete;

    [[nodiscard]] bool register_actor(BlockCombatActor& actor);
    void unregister_actor(BlockCombatActor& actor) noexcept;
    [[nodiscard]] elysia::gameplay::collision::AttackInstanceId begin_attack(
        BlockCombatActor& actor,
        elysia::gameplay::collision::AttackDefinitionId definition);
    void end_attack(BlockCombatActor& actor,
        elysia::gameplay::collision::AttackInstanceId instance) noexcept;

    [[nodiscard]] bool is_grounded(const BlockCombatActor& actor) const;
    [[nodiscard]] bool wall_left(const BlockCombatActor& actor) const;
    [[nodiscard]] bool wall_right(const BlockCombatActor& actor) const;
    [[nodiscard]] bool request_drop_through(const BlockCombatActor& actor);
    void update(double delta);
    void flush_deaths();
    void set_death_callback(DeathCallback callback) { _death_callback = std::move(callback); }
    void set_damage_callback(DamageCallback callback) { _damage_callback = std::move(callback); }

    [[nodiscard]] elysia::physics::PhysicsWorld& world() noexcept { return *_world; }
    [[nodiscard]] const elysia::physics::PhysicsWorld& world() const noexcept { return *_world; }

    void on_body_contact(
        const elysia::gameplay::collision::BodyContactEvent& event) override;
    void on_hit_overlap(
        const elysia::gameplay::collision::HitOverlapEvent& event) override;

private:
    [[nodiscard]] bool contact_direction(
        const BlockCombatActor& actor, elysia::core::Vector2 direction) const;
    void apply_environment_damage(BlockCombatActor& actor,
        elysia::core::Vector2 hit_point);
    void queue_death(BlockCombatActor& actor);

    elysia::physics::PhysicsWorld* _world = nullptr;
    elysia::gameplay::collision::GameplayCollisionRuntime* _runtime = nullptr;
    std::unordered_map<elysia::gameplay::collision::ActorId, BlockCombatActor*> _actors;
    std::unordered_map<elysia::gameplay::collision::AttackDefinitionId, DamageDefinition> _definitions;
    std::unordered_map<elysia::gameplay::collision::ActorId, double> _hazard_cooldowns;
    std::vector<BlockCombatActor*> _pending_deaths;
    DeathCallback _death_callback;
    DamageCallback _damage_callback;
    elysia::gameplay::collision::ActorId _next_actor = 1;
    elysia::gameplay::collision::AttackInstanceId _next_attack = 1;
};

inline constexpr elysia::gameplay::collision::AttackDefinitionId PlayerAttack = 1;
inline constexpr elysia::gameplay::collision::AttackDefinitionId EnemyAttack = 2;
inline constexpr elysia::gameplay::collision::AttackDefinitionId EnvironmentAttack = 3;
}
