#include "demo_combat.h"

#include "block_actor.h"
#include "../../engine/effects/effect_service.h"
#include "../../engine/effects/effect_types.h"

#include <algorithm>
#include <string>

namespace example::physics_demo
{
Health::Health(int maximum) noexcept
    : _maximum(std::max(1, maximum)), _current(_maximum)
{
}

int Health::apply_damage(int amount) noexcept
{
    if (amount <= 0 || !alive())
        return 0;
    const int applied = std::min(amount, _current);
    _current -= applied;
    return applied;
}

DemoCombatSession::DemoCombatSession(
    elysia::physics::PhysicsWorld& world,
    elysia::gameplay::collision::GameplayCollisionRuntime& runtime)
    : _world(&world), _runtime(&runtime)
{
    _definitions.emplace(PlayerAttack,
        DamageDefinition{PlayerAttack, 25, {35.0f, -25.0f}});
    _definitions.emplace(EnemyAttack,
        DamageDefinition{EnemyAttack, 15, {20.0f, -12.0f}});
    _definitions.emplace(EnvironmentAttack,
        DamageDefinition{EnvironmentAttack, 10, {}});
    (void)_runtime->add_listener(*this);
}

DemoCombatSession::~DemoCombatSession()
{
    if (!_runtime)
        return;
    (void)_runtime->remove_listener(*this);
    for (const auto& [id, actor] : _actors)
    {
        (void)id;
        if (!actor)
            continue;
        (void)_runtime->unbind_collider(actor->body_collider_id());
        (void)_runtime->unbind_collider(actor->hurt_collider_id());
        (void)_runtime->unbind_collider(actor->hit_collider_id());
    }
}

bool DemoCombatSession::register_actor(BlockCombatActor& actor)
{
    if (actor.actor_id() != elysia::gameplay::collision::InvalidActorId
        || actor.body_collider_id() == elysia::physics::InvalidColliderId
        || actor.hurt_collider_id() == elysia::physics::InvalidColliderId)
        return false;
    const auto id = _next_actor++;
    actor.assign_actor_id(id);
    const elysia::gameplay::collision::ActorCollisionRig rig{
        id, actor.team(), actor.body_collider_id(),
        elysia::physics::InvalidColliderId,
        {actor.hurt_collider_id()}, {}};
    if (!_runtime->bind_actor(rig))
    {
        actor.assign_actor_id(elysia::gameplay::collision::InvalidActorId);
        return false;
    }
    _actors.emplace(id, &actor);
    return true;
}

void DemoCombatSession::unregister_actor(BlockCombatActor& actor) noexcept
{
    if (!_runtime || actor.actor_id() == elysia::gameplay::collision::InvalidActorId)
        return;
    actor.stop_attack();
    (void)_runtime->unbind_collider(actor.body_collider_id());
    (void)_runtime->unbind_collider(actor.hurt_collider_id());
    (void)_runtime->unbind_collider(actor.hit_collider_id());
    _hazard_cooldowns.erase(actor.actor_id());
    _actors.erase(actor.actor_id());
}

elysia::gameplay::collision::AttackInstanceId DemoCombatSession::begin_attack(
    BlockCombatActor& actor,
    elysia::gameplay::collision::AttackDefinitionId definition)
{
    if (!_definitions.contains(definition) || !actor.alive())
        return elysia::gameplay::collision::InvalidAttackInstanceId;
    const auto instance = _next_attack++;
    const elysia::gameplay::collision::HitBoxBinding binding{
        {actor.hit_collider_id(), actor.actor_id(), actor.team(),
            elysia::gameplay::collision::ColliderRole::HitBox},
        actor.actor_id(), instance, definition};
    return _runtime->bind_hit_box(binding)
        ? instance : elysia::gameplay::collision::InvalidAttackInstanceId;
}

void DemoCombatSession::end_attack(BlockCombatActor&,
    elysia::gameplay::collision::AttackInstanceId instance) noexcept
{
    if (instance != elysia::gameplay::collision::InvalidAttackInstanceId)
        _runtime->end_attack_instance(instance);
}

bool DemoCombatSession::contact_direction(
    const BlockCombatActor& actor, elysia::core::Vector2 direction) const
{
    std::vector<elysia::physics::CollisionContact> contacts;
    const auto target = elysia::physics::CollisionTarget::from_collider(
        actor.body_collider_id());
    _world->collect_contacts(target, contacts);
    for (const auto& contact : contacts)
    {
        if (contact.response != elysia::physics::CollisionResponse::Block)
            continue;
        auto normal = contact.pair.first == target
            ? contact.manifold.normal : -contact.manifold.normal;
        if (normal.dot(direction) >= 0.5f)
            return true;
    }
    return false;
}

bool DemoCombatSession::is_grounded(const BlockCombatActor& actor) const
{
    return contact_direction(actor, {0, 1});
}

bool DemoCombatSession::wall_left(const BlockCombatActor& actor) const
{
    return contact_direction(actor, {-1, 0});
}

bool DemoCombatSession::wall_right(const BlockCombatActor& actor) const
{
    return contact_direction(actor, {1, 0});
}

bool DemoCombatSession::request_drop_through(const BlockCombatActor& actor)
{
    std::vector<elysia::physics::CollisionContact> contacts;
    const auto body = elysia::physics::CollisionTarget::from_collider(
        actor.body_collider_id());
    _world->collect_contacts(body, contacts);
    for (const auto& contact : contacts)
    {
        const auto other = contact.pair.first == body
            ? contact.pair.second : contact.pair.first;
        if (_runtime->request_drop_through({actor.body_collider_id(), other}))
            return true;
    }
    return false;
}

void DemoCombatSession::update(double delta)
{
    const double safe_delta = std::max(0.0, delta);
    for (auto& [actor, cooldown] : _hazard_cooldowns)
    {
        (void)actor;
        cooldown = std::max(0.0, cooldown - safe_delta);
    }
}

void DemoCombatSession::queue_death(BlockCombatActor& actor)
{
    if (std::ranges::find(_pending_deaths, &actor) == _pending_deaths.end())
        _pending_deaths.push_back(&actor);
}

void DemoCombatSession::flush_deaths()
{
    auto deaths = std::move(_pending_deaths);
    _pending_deaths.clear();
    for (BlockCombatActor* actor : deaths)
    {
        if (!actor)
            continue;
        unregister_actor(*actor);
        if (_death_callback)
            _death_callback(*actor);
    }
}

void DemoCombatSession::on_hit_overlap(
    const elysia::gameplay::collision::HitOverlapEvent& event)
{
    const auto actor_it = _actors.find(event.hurt_box.owner);
    const auto definition_it = _definitions.find(event.hit_box.attack_definition);
    if (actor_it == _actors.end() || !actor_it->second
        || definition_it == _definitions.end())
        return;
    BlockCombatActor& actor = *actor_it->second;
    DamageRequest request{
        event.hit_box.instigator, event.hurt_box.owner,
        event.hit_box.attack_definition,
        event.overlap.manifold.contact_points[0]};
    DamageDefinition definition = definition_it->second;
    const auto source = _actors.find(event.hit_box.instigator);
    if (source != _actors.end() && source->second
        && !definition.knockback.is_zero())
    {
        const float strength = definition.knockback.length();
        definition.knockback =
            (actor.center() - source->second->center()).normalized() * strength;
    }
    const DamageResult result = actor.apply_damage(request, definition);
    if (result.applied <= 0)
        return;

    elysia::effects::FloatingNumberEffectSpawnRequest effect;
    effect.text = "-" + std::to_string(result.applied);
    effect.color = elysia::effects::FloatingNumberColor::Red;
    effect.position = actor.world_rect().top_center();
    effect.alignment = elysia::effects::FloatingNumberAlignment::Center;
    effect.effects.motion = elysia::effects::FloatingNumberLinearMotion{{0, -42}};
    effect.effects.fade = elysia::effects::FloatingNumberFade{};
    (void)ELYSIA_EFFECTS->request_floating_number_effect(effect);
    if (_damage_callback)
        _damage_callback(actor, result);
    if (result.killed)
        queue_death(actor);
}

void DemoCombatSession::apply_environment_damage(
    BlockCombatActor& actor, elysia::core::Vector2 hit_point)
{
    double& cooldown = _hazard_cooldowns[actor.actor_id()];
    if (cooldown > 0.0)
        return;
    cooldown = 0.5;
    const DamageRequest request{
        elysia::gameplay::collision::InvalidActorId,
        actor.actor_id(), EnvironmentAttack, hit_point};
    const auto& definition = _definitions.at(EnvironmentAttack);
    const DamageResult result = actor.apply_damage(request, definition);
    if (_damage_callback)
        _damage_callback(actor, result);
    if (result.killed)
        queue_death(actor);
}

void DemoCombatSession::on_body_contact(
    const elysia::gameplay::collision::BodyContactEvent& event)
{
    if (event.phase == elysia::physics::CollisionEventPhase::End
        || event.other.kind != elysia::physics::CollisionTargetKind::Tile
        || !_world->tile_world())
        return;
    const auto cell = _world->tile_world()->cell_at(event.other.tile);
    if (cell.type != elysia::physics::TileCollisionType::Overlap
        || cell.tag != "hazard")
        return;
    const auto found = _actors.find(event.body.owner);
    if (found != _actors.end() && found->second)
        apply_environment_damage(*found->second,
            event.contact.manifold.contact_points[0]);
}
}
