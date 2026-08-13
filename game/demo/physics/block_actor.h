#pragma once

#include "demo_collision_layers.h"
#include "demo_combat.h"
#include "colored_block_object.h"

#include "../../../engine/core/interface/updatable.h"
#include "../../../engine/gameplay/input/contracts/gameplay_input_frame_receiver.h"
#include "../../../engine/physics/contracts/collider_provider.h"
#include "../../../engine/physics/contracts/physics_body_provider.h"

#include <array>

namespace example::demo::physics
{
enum class Facing
{
    Left,
    Right,
    Up,
    Down
};

struct ActorConfig
{
    elysia::core::Rect rect{};
    elysia::core::Color color{};
    elysia::gameplay::collision::TeamId team =
        elysia::gameplay::collision::teams::Neutral;
    int maximum_health = 1;
    float move_speed = 0.0f;
    bool gravity_enabled = true;
};

class BlockCombatActor
    : public ColoredBlockObject
    , public elysia::core::Updatable
    , public elysia::physics::PhysicsBodyProvider
    , public elysia::physics::ColliderProvider
    , public IDamageableActor
{
public:
    explicit BlockCombatActor(ActorConfig config);
    ~BlockCombatActor() override;

    void update(double delta) override;
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;

    [[nodiscard]] bool bind_combat(DemoCombatSession& session);
    void start_attack();
    void stop_attack() noexcept;
    void mark_dead() noexcept;

    [[nodiscard]] elysia::gameplay::collision::ActorId actor_id() const noexcept override { return _actor_id; }
    [[nodiscard]] const Health& health() const noexcept override { return _health; }
    [[nodiscard]] DamageResult apply_damage(
        const DamageRequest&, const DamageDefinition&) override;
    [[nodiscard]] bool alive() const noexcept { return _health.alive(); }
    [[nodiscard]] elysia::gameplay::collision::TeamId team() const noexcept { return _team; }
    [[nodiscard]] elysia::physics::ColliderId body_collider_id() const noexcept { return _colliders[0].id; }
    [[nodiscard]] elysia::physics::ColliderId hurt_collider_id() const noexcept { return _colliders[1].id; }
    [[nodiscard]] elysia::physics::ColliderId hit_collider_id() const noexcept { return _colliders[2].id; }
    [[nodiscard]] Facing facing() const noexcept { return _facing; }
    [[nodiscard]] float move_speed() const noexcept { return _move_speed; }
    [[nodiscard]] DemoCombatSession* combat_session() const noexcept { return _session; }

    void set_facing(Facing facing) noexcept;
    void set_attack_definition(elysia::gameplay::collision::AttackDefinitionId definition) noexcept { _attack_definition = definition; }
    void set_attack_timing(double total, double active_begin, double active_end,
        double cooldown) noexcept;

    [[nodiscard]] elysia::physics::PhysicsBody* physics_body() noexcept override { return &_body; }
    [[nodiscard]] const elysia::physics::PhysicsBody* physics_body() const noexcept override { return &_body; }
    [[nodiscard]] std::span<elysia::physics::Collider> colliders() noexcept override { return _colliders; }
    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept override { return _colliders; }

protected:
    void tick_actor(double delta);
    [[nodiscard]] bool attack_ready() const noexcept;
    void face_toward(elysia::core::Vector2 direction) noexcept;

private:
    friend class DemoCombatSession;
    void assign_actor_id(elysia::gameplay::collision::ActorId id) noexcept { _actor_id = id; }
    void update_hit_box_shape() noexcept;

    DemoCombatSession* _session = nullptr;
    elysia::physics::PhysicsBody _body;
    std::array<elysia::physics::Collider, 3> _colliders;
    Health _health;
    elysia::gameplay::collision::ActorId _actor_id =
        elysia::gameplay::collision::InvalidActorId;
    elysia::gameplay::collision::TeamId _team;
    elysia::gameplay::collision::AttackDefinitionId _attack_definition = PlayerAttack;
    elysia::gameplay::collision::AttackInstanceId _attack_instance =
        elysia::gameplay::collision::InvalidAttackInstanceId;
    Facing _facing = Facing::Right;
    float _move_speed = 0.0f;
    double _attack_elapsed = 0.0;
    double _attack_total = 0.30;
    double _active_begin = 0.07;
    double _active_end = 0.18;
    double _cooldown_remaining = 0.0;
    double _attack_cooldown = 0.35;
    bool _attacking = false;
    bool _hit_box_active = false;
};

class PlatformPlayerCharacter final
    : public BlockCombatActor
    , public elysia::gameplay::GameplayInputFrameReceiver
{
public:
    explicit PlatformPlayerCharacter(const elysia::core::Rect& rect);
    void update(double delta) override;
    void on_gameplay_input_frame(
        const elysia::gameplay::GameplayInputFrame& input) override;
private:
    float _move_axis = 0.0f;
    bool _jump_requested = false;
    bool _drop_requested = false;
};

class TopDownPlayerCharacter final
    : public BlockCombatActor
    , public elysia::gameplay::GameplayInputFrameReceiver
{
public:
    explicit TopDownPlayerCharacter(const elysia::core::Rect& rect);
    void update(double delta) override;
    void on_gameplay_input_frame(
        const elysia::gameplay::GameplayInputFrame& input) override;
private:
    elysia::core::Vector2 _move{};
};

class StationaryEnemy final : public BlockCombatActor
{
public:
    StationaryEnemy(const elysia::core::Rect& rect, BlockCombatActor& target);
    void update(double delta) override;
private:
    BlockCombatActor* _target = nullptr;
};

class PlatformPatrolEnemy final : public BlockCombatActor
{
public:
    PlatformPatrolEnemy(
        const elysia::core::Rect& rect, BlockCombatActor& target,
        float patrol_left, float patrol_right);
    void update(double delta) override;
private:
    BlockCombatActor* _target = nullptr;
    float _patrol_left = 0.0f;
    float _patrol_right = 0.0f;
    float _direction = -1.0f;
};

class TopDownChaseEnemy final : public BlockCombatActor
{
public:
    TopDownChaseEnemy(
        const elysia::core::Rect& rect, BlockCombatActor& target);
    void update(double delta) override;
private:
    [[nodiscard]] bool has_line_of_sight() const;
    BlockCombatActor* _target = nullptr;
};
}
