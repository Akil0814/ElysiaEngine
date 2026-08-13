#include "block_actor.h"

#include "../../../engine/core/render/colors.h"
#include "../../../engine/core/render/render_command.h"

#include <algorithm>
#include <cmath>

namespace example::demo::physics
{
namespace
{
elysia::physics::Collider make_actor_collider(
    elysia::physics::CollisionBits category,
    elysia::physics::CollisionBits mask,
    elysia::physics::CollisionResponse response,
    const elysia::core::Rect& local_rect,
    const char* tag)
{
    elysia::physics::Collider collider;
    collider.shape = elysia::physics::AabbShape{local_rect};
    collider.filter = {category, mask, 0};
    collider.response = response;
    collider.tag = tag;
    return collider;
}

bool close_enough(const BlockCombatActor& first,
    const BlockCombatActor& second, float x, float y)
{
    const auto delta = second.center() - first.center();
    return std::fabs(delta.x) <= x && std::fabs(delta.y) <= y;
}
}

BlockCombatActor::BlockCombatActor(ActorConfig config)
    : ColoredBlockObject(elysia::core::DepthLayer::Character,
          config.rect, config.color),
      _health(config.maximum_health), _team(config.team),
      _move_speed(config.move_speed)
{
    const auto size = config.rect.size();
    _colliders[0] = make_actor_collider(
        collision_layers::Body,
        collision_layers::World | collision_layers::Body,
        elysia::physics::CollisionResponse::Block,
        {0, 0, size.x, size.y}, "body");
    _colliders[0].material = {0.0f, 0.0f, 0.0f};
    _colliders[1] = make_actor_collider(
        collision_layers::HurtBox, collision_layers::HitBox,
        elysia::physics::CollisionResponse::Overlap,
        {2, 2, std::max(1.0f, size.x - 4.0f), std::max(1.0f, size.y - 4.0f)},
        "hurt_box");
    _colliders[2] = make_actor_collider(
        collision_layers::HitBox, collision_layers::HurtBox,
        elysia::physics::CollisionResponse::Overlap,
        {}, "hit_box");
    _colliders[2].enabled = false;
    _body.type = elysia::physics::BodyType::Dynamic;
    _body.gravity_scale = config.gravity_enabled ? 1.0f : 0.0f;
    _body.mass = 1.0f;
    _body.linear_damping = config.gravity_enabled ? 0.0f : 7.0f;
    _body.max_speed = {config.move_speed, config.gravity_enabled ? 700.0f : config.move_speed};
    update_hit_box_shape();
}

BlockCombatActor::~BlockCombatActor() = default;

void BlockCombatActor::update(double delta)
{
    tick_actor(delta);
}

void BlockCombatActor::tick_actor(double delta)
{
    update_visual(delta);
    const double safe_delta = std::max(0.0, delta);
    _cooldown_remaining = std::max(0.0, _cooldown_remaining - safe_delta);
    if (!_attacking)
        return;

    _attack_elapsed += safe_delta;
    const bool should_be_active = _attack_elapsed >= _active_begin
        && _attack_elapsed < _active_end;
    _colliders[2].enabled = should_be_active && alive();
    _hit_box_active = _colliders[2].enabled;
    if (_attack_elapsed >= _attack_total)
        stop_attack();
}

void BlockCombatActor::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    ColoredBlockObject::submit_render_commands(out_commands);
    if (health().maximum() <= 0)
        return;

    constexpr float bar_height = 4.0f;
    const float ratio = static_cast<float>(health().current())
        / static_cast<float>(health().maximum());
    const elysia::core::Rect background{
        world_rect().left(), world_rect().top() - 8.0f,
        world_rect().width(), bar_height};
    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        background, {40, 40, 40, 255}));
    if (ratio > 0.0f)
    {
        auto fill = background;
        fill.set_width(background.width() * ratio);
        out_commands.push_back(elysia::core::make_world_fill_rect_command(
            fill, {76, 175, 80, 255}));
    }
}

bool BlockCombatActor::bind_combat(DemoCombatSession& session)
{
    if (_session == &session)
        return true;
    if (_session)
        return false;
    if (!session.register_actor(*this))
        return false;
    _session = &session;
    return true;
}

bool BlockCombatActor::attack_ready() const noexcept
{
    return alive() && !_attacking && _cooldown_remaining <= 0.0 && _session;
}

void BlockCombatActor::start_attack()
{
    if (!attack_ready())
        return;
    update_hit_box_shape();
    _attack_instance = _session->begin_attack(*this, _attack_definition);
    if (_attack_instance == elysia::gameplay::collision::InvalidAttackInstanceId)
        return;
    _attacking = true;
    _attack_elapsed = 0.0;
    _cooldown_remaining = _attack_cooldown;
}

void BlockCombatActor::stop_attack() noexcept
{
    _colliders[2].enabled = false;
    _hit_box_active = false;
    _attacking = false;
    _attack_elapsed = 0.0;
    if (_session && _attack_instance != elysia::gameplay::collision::InvalidAttackInstanceId)
        _session->end_attack(*this, _attack_instance);
    _attack_instance = elysia::gameplay::collision::InvalidAttackInstanceId;
}

void BlockCombatActor::mark_dead() noexcept
{
    // Binding removal is deferred because this can run inside a gameplay
    // collision listener batch.
    _colliders[2].enabled = false;
    _hit_box_active = false;
    _attacking = false;
    _attack_elapsed = 0.0;
    _body.velocity = {};
    _body.enabled = false;
    for (auto& collider : _colliders)
        collider.enabled = false;
    set_dead_visual(true);
}

DamageResult BlockCombatActor::apply_damage(
    const DamageRequest&, const DamageDefinition& definition)
{
    const bool was_alive = _health.alive();
    const int applied = _health.apply_damage(definition.damage);
    if (applied > 0)
    {
        flash(0.12);
        _body.velocity += definition.knockback;
    }
    const bool killed = was_alive && !_health.alive();
    if (killed)
        mark_dead();
    return {applied, _health.current(), killed};
}

void BlockCombatActor::set_facing(Facing facing) noexcept
{
    _facing = facing;
    update_hit_box_shape();
}

void BlockCombatActor::face_toward(elysia::core::Vector2 direction) noexcept
{
    if (direction.is_zero())
        return;
    if (std::fabs(direction.x) >= std::fabs(direction.y))
        set_facing(direction.x < 0.0f ? Facing::Left : Facing::Right);
    else
        set_facing(direction.y < 0.0f ? Facing::Up : Facing::Down);
}

void BlockCombatActor::set_attack_timing(
    double total, double active_begin, double active_end, double cooldown) noexcept
{
    if (total > 0.0 && active_begin >= 0.0 && active_end > active_begin
        && active_end <= total && cooldown >= 0.0)
    {
        _attack_total = total;
        _active_begin = active_begin;
        _active_end = active_end;
        _attack_cooldown = cooldown;
    }
}

void BlockCombatActor::update_hit_box_shape() noexcept
{
    const auto size = world_rect().size();
    constexpr float range = 30.0f;
    elysia::core::Rect rect;
    switch (_facing)
    {
    case Facing::Left:
        rect = {-range, size.y * 0.15f, range, size.y * 0.70f}; break;
    case Facing::Right:
        rect = {size.x, size.y * 0.15f, range, size.y * 0.70f}; break;
    case Facing::Up:
        rect = {size.x * 0.15f, -range, size.x * 0.70f, range}; break;
    case Facing::Down:
        rect = {size.x * 0.15f, size.y, size.x * 0.70f, range}; break;
    }
    _colliders[2].shape = elysia::physics::AabbShape{rect};
}

PlatformPlayerCharacter::PlatformPlayerCharacter(
    const elysia::core::Rect& rect)
    : BlockCombatActor({rect, elysia::core::colors::blue_500,
          elysia::gameplay::collision::teams::Player, 100, 220.0f, true})
{
    set_attack_definition(PlayerAttack);
}

void PlatformPlayerCharacter::on_gameplay_input_frame(
    const elysia::gameplay::GameplayInputFrame& input)
{
    _move_axis = std::clamp(input.move().x, -1.0f, 1.0f);
    if (input.jump_pressed())
    {
        _jump_requested = true;
        _drop_requested = input.move().y > 0.5f;
    }
    if (input.primary_pressed())
        start_attack();
}

void PlatformPlayerCharacter::update(double delta)
{
    tick_actor(delta);
    if (!alive())
        return;
    physics_body()->velocity.x = _move_axis * move_speed();
    if (_move_axis != 0.0f)
        set_facing(_move_axis < 0.0f ? Facing::Left : Facing::Right);
    if (_jump_requested && combat_session())
    {
        if (_drop_requested)
            (void)combat_session()->request_drop_through(*this);
        else if (combat_session()->is_grounded(*this))
            physics_body()->velocity.y = -520.0f;
    }
    _jump_requested = false;
    _drop_requested = false;
}

TopDownPlayerCharacter::TopDownPlayerCharacter(
    const elysia::core::Rect& rect)
    : BlockCombatActor({rect, elysia::core::colors::blue_500,
          elysia::gameplay::collision::teams::Player, 100, 200.0f, false})
{
    set_attack_definition(PlayerAttack);
}

void TopDownPlayerCharacter::on_gameplay_input_frame(
    const elysia::gameplay::GameplayInputFrame& input)
{
    _move = input.move();
    if (_move.length_squared() > 1.0f)
        _move.normalize_in_place();
    if (!_move.is_zero())
        face_toward(_move);
    if (input.primary_pressed())
        start_attack();
}

void TopDownPlayerCharacter::update(double delta)
{
    tick_actor(delta);
    physics_body()->velocity = alive() ? _move * move_speed() : elysia::core::Vector2{};
}

StationaryEnemy::StationaryEnemy(
    const elysia::core::Rect& rect, BlockCombatActor& target)
    : BlockCombatActor({rect, elysia::core::colors::red_500,
          elysia::gameplay::collision::teams::Enemy, 50, 0.0f, true}),
      _target(&target)
{
    set_attack_definition(EnemyAttack);
    set_attack_timing(0.45, 0.15, 0.28, 1.0);
}

void StationaryEnemy::update(double delta)
{
    tick_actor(delta);
    if (!_target || !_target->alive() || !alive())
        return;
    const auto toward = _target->center() - center();
    face_toward(toward);
    if (close_enough(*this, *_target, 58.0f, 44.0f))
        start_attack();
}

PlatformPatrolEnemy::PlatformPatrolEnemy(
    const elysia::core::Rect& rect, BlockCombatActor& target,
    float patrol_left, float patrol_right)
    : BlockCombatActor({rect, elysia::core::colors::orange_500,
          elysia::gameplay::collision::teams::Enemy, 50, 85.0f, true}),
      _target(&target), _patrol_left(patrol_left), _patrol_right(patrol_right)
{
    set_attack_definition(EnemyAttack);
    set_attack_timing(0.45, 0.15, 0.28, 1.0);
}

void PlatformPatrolEnemy::update(double delta)
{
    tick_actor(delta);
    if (!alive())
        return;
    const bool target_close = _target && _target->alive()
        && close_enough(*this, *_target, 62.0f, 46.0f);
    if (target_close)
    {
        physics_body()->velocity.x = 0.0f;
        face_toward(_target->center() - center());
        start_attack();
        return;
    }
    if (position().x <= _patrol_left || (combat_session() && combat_session()->wall_left(*this)))
        _direction = 1.0f;
    if (position().x >= _patrol_right || (combat_session() && combat_session()->wall_right(*this)))
        _direction = -1.0f;
    physics_body()->velocity.x = _direction * move_speed();
    set_facing(_direction < 0.0f ? Facing::Left : Facing::Right);
}

TopDownChaseEnemy::TopDownChaseEnemy(
    const elysia::core::Rect& rect, BlockCombatActor& target)
    : BlockCombatActor({rect, elysia::core::colors::red_500,
          elysia::gameplay::collision::teams::Enemy, 50, 105.0f, false}),
      _target(&target)
{
    set_attack_definition(EnemyAttack);
    set_attack_timing(0.45, 0.15, 0.28, 1.0);
}

bool TopDownChaseEnemy::has_line_of_sight() const
{
    if (!_target || !combat_session())
        return false;
    elysia::physics::SegmentCastQuery query;
    query.start = center();
    query.end = _target->center();
    query.filter = {collision_layers::Body, collision_layers::World, 0};
    return !combat_session()->world().segment_cast(query).has_value();
}

void TopDownChaseEnemy::update(double delta)
{
    tick_actor(delta);
    if (!_target || !_target->alive() || !alive())
    {
        physics_body()->velocity = {};
        return;
    }
    const auto toward = _target->center() - center();
    const float distance = toward.length();
    face_toward(toward);
    if (distance <= 48.0f)
    {
        physics_body()->velocity = {};
        start_attack();
    }
    else if (distance <= 420.0f && has_line_of_sight())
    {
        physics_body()->velocity = toward.normalized() * move_speed();
    }
    else
    {
        physics_body()->velocity = {};
    }
}
}
