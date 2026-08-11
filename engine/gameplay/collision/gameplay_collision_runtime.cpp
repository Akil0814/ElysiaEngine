#include "gameplay_collision_runtime.h"

#include <algorithm>

namespace elysia::gameplay::collision
{
namespace
{
[[nodiscard]] TeamRelation default_relation(TeamId source, TeamId target) noexcept
{
    if (source != InvalidTeamId && source == target)
        return TeamRelation::Friendly;
    if (source == teams::Neutral || target == teams::Neutral
        || source == InvalidTeamId || target == InvalidTeamId)
    {
        return TeamRelation::Neutral;
    }
    return TeamRelation::Hostile;
}

[[nodiscard]] bool valid_binding(
    const ColliderBinding& binding,
    const elysia::physics::PhysicsWorld& world) noexcept
{
    return binding.collider != elysia::physics::InvalidColliderId
        && binding.owner != InvalidActorId
        && world.contains_collider(binding.collider);
}
}

GameplayCollisionRuntime::GameplayCollisionRuntime(elysia::physics::PhysicsWorld& world)
    : _world(&world)
{
    if (!_world->add_listener(*this))
        _world = nullptr;
}

GameplayCollisionRuntime::~GameplayCollisionRuntime()
{
    if (_world)
        (void)_world->remove_listener(*this);
}

std::size_t GameplayCollisionRuntime::AttackHitKeyHash::operator()(
    const AttackHitKey& key) const noexcept
{
    return std::hash<AttackInstanceId>{}(key.attack)
        ^ (std::hash<ActorId>{}(key.hurt_owner) << 1u);
}

bool GameplayCollisionRuntime::bind_actor(const ActorCollisionRig& rig)
{
    if (!_world || rig.owner == InvalidActorId || _rigs.contains(rig.owner))
        return false;
    std::vector<ColliderBinding> additions;
    const auto add = [&](elysia::physics::ColliderId id, ColliderRole role)
    {
        if (id != elysia::physics::InvalidColliderId)
            additions.push_back({id, rig.owner, rig.team, role});
    };
    add(rig.body, ColliderRole::Body);
    add(rig.push_box, ColliderRole::PushBox);
    for (const auto id : rig.hurt_boxes) add(id, ColliderRole::HurtBox);
    for (const auto id : rig.sensors) add(id, ColliderRole::Sensor);
    std::ranges::sort(additions, {}, &ColliderBinding::collider);
    if (std::adjacent_find(additions.begin(), additions.end(),
            [](const auto& a, const auto& b) { return a.collider == b.collider; })
        != additions.end())
    {
        return false;
    }
    for (const ColliderBinding& binding : additions)
        if (!valid_binding(binding, *_world) || _bindings.contains(binding.collider))
            return false;
    _rigs.emplace(rig.owner, rig);
    for (const ColliderBinding& binding : additions)
        _bindings.emplace(binding.collider, binding);
    return true;
}

bool GameplayCollisionRuntime::bind_collider(const ColliderBinding& binding)
{
    if (!_world || !valid_binding(binding, *_world))
        return false;
    const auto found = _bindings.find(binding.collider);
    if (found != _bindings.end())
        return found->second.collider == binding.collider
            && found->second.owner == binding.owner
            && found->second.team == binding.team
            && found->second.role == binding.role;
    _bindings.emplace(binding.collider, binding);
    return true;
}

bool GameplayCollisionRuntime::bind_hit_box(const HitBoxBinding& binding)
{
    if (!_world || binding.collider.role != ColliderRole::HitBox
        || !valid_binding(binding.collider, *_world)
        || binding.instigator == InvalidActorId
        || binding.attack_instance == InvalidAttackInstanceId
        || binding.attack_definition == InvalidAttackDefinitionId
        || _bindings.contains(binding.collider.collider)
        || _hit_boxes.contains(binding.collider.collider))
    {
        return false;
    }
    _bindings.emplace(binding.collider.collider, binding.collider);
    try
    {
        _hit_boxes.emplace(binding.collider.collider, binding);
    }
    catch (...)
    {
        _bindings.erase(binding.collider.collider);
        throw;
    }
    return true;
}

bool GameplayCollisionRuntime::unbind_collider(elysia::physics::ColliderId collider)
{
    if (collider == elysia::physics::InvalidColliderId)
        return false;
    const bool removed = _bindings.erase(collider) > 0;
    _hit_boxes.erase(collider);
    return removed;
}

bool GameplayCollisionRuntime::request_drop_through(const DropThroughRequest& request)
{
    return _world && _world->request_pass_through(request.actor, request.target);
}

bool GameplayCollisionRuntime::add_listener(GameplayCollisionListener& listener)
{
    if (_dispatching)
    {
        _pending_listener_operations.emplace_back(&listener, true);
        return true;
    }
    if (std::ranges::find(_listeners, &listener) == _listeners.end())
        _listeners.push_back(&listener);
    return true;
}

bool GameplayCollisionRuntime::remove_listener(const GameplayCollisionListener& listener)
{
    if (_dispatching)
    {
        _pending_listener_operations.emplace_back(
            const_cast<GameplayCollisionListener*>(&listener), false);
        return true;
    }
    const auto found = std::ranges::find(_listeners, &listener);
    if (found == _listeners.end())
        return false;
    _listeners.erase(found);
    return true;
}

void GameplayCollisionRuntime::end_attack_instance(AttackInstanceId attack_instance)
{
    if (attack_instance == InvalidAttackInstanceId)
        return;
    std::erase_if(_attack_hits, [attack_instance](const AttackHitKey& key)
    { return key.attack == attack_instance; });
    std::vector<elysia::physics::ColliderId> ended_hit_boxes;
    for (const auto& [collider, binding] : _hit_boxes)
        if (binding.attack_instance == attack_instance)
            ended_hit_boxes.push_back(collider);
    for (const auto collider : ended_hit_boxes)
    {
        _hit_boxes.erase(collider);
        _bindings.erase(collider);
    }
}

void GameplayCollisionRuntime::set_team_relation_resolver(
    const TeamRelationResolver* resolver) noexcept
{
    _team_resolver = resolver;
}

TeamRelation GameplayCollisionRuntime::team_relation(TeamId source, TeamId target) const noexcept
{
    return _team_resolver ? _team_resolver->relation(source, target)
        : default_relation(source, target);
}

void GameplayCollisionRuntime::clear() noexcept
{
    _rigs.clear();
    _bindings.clear();
    _hit_boxes.clear();
    _attack_hits.clear();
    _listeners.clear();
    _pending_listener_operations.clear();
    _team_resolver = nullptr;
}

void GameplayCollisionRuntime::dispatch_body(
    const ColliderBinding& body,
    elysia::physics::CollisionTarget other,
    const elysia::physics::CollisionEvent& event)
{
    BodyContactEvent routed{event.phase, body, other, event.contact};
    for (GameplayCollisionListener* listener : _listeners)
        if (listener) listener->on_body_contact(routed);
}

void GameplayCollisionRuntime::on_collision_event(
    const elysia::physics::CollisionEvent& event)
{
    const auto first = event.contact.pair.first;
    const auto second = event.contact.pair.second;
    const auto first_binding = first.kind == elysia::physics::CollisionTargetKind::Collider
        ? _bindings.find(first.collider) : _bindings.end();
    const auto second_binding = second.kind == elysia::physics::CollisionTargetKind::Collider
        ? _bindings.find(second.collider) : _bindings.end();
    _dispatching = true;
    try
    {

    if (first_binding != _bindings.end() && first_binding->second.role == ColliderRole::Body)
        dispatch_body(first_binding->second, second, event);
    if (second_binding != _bindings.end() && second_binding->second.role == ColliderRole::Body)
        dispatch_body(second_binding->second, first, event);

    if (first_binding != _bindings.end() && second_binding != _bindings.end()
        && first_binding->second.role == ColliderRole::PushBox
        && second_binding->second.role == ColliderRole::PushBox)
    {
        PushBoxOverlapEvent routed{
            event.phase, first_binding->second, second_binding->second,
            {event.contact.pair, event.contact.manifold}
        };
        for (GameplayCollisionListener* listener : _listeners)
            if (listener) listener->on_push_box_overlap(routed);
    }

    if (event.phase == elysia::physics::CollisionEventPhase::Begin
        && first_binding != _bindings.end() && second_binding != _bindings.end())
    {
        const auto route_hit = [&](const ColliderBinding& hit_binding,
                                   const ColliderBinding& hurt_binding)
        {
            if (hit_binding.role != ColliderRole::HitBox
                || hurt_binding.role != ColliderRole::HurtBox)
                return;
            const auto hit = _hit_boxes.find(hit_binding.collider);
            if (hit == _hit_boxes.end()
                || team_relation(hit->second.collider.team, hurt_binding.team)
                    != TeamRelation::Hostile)
                return;
            const AttackHitKey key{hit->second.attack_instance, hurt_binding.owner};
            if (!_attack_hits.insert(key).second)
                return;
            HitOverlapEvent routed{
                event.phase, hit->second, hurt_binding,
                {event.contact.pair, event.contact.manifold}
            };
            for (GameplayCollisionListener* listener : _listeners)
                if (listener) listener->on_hit_overlap(routed);
        };
        route_hit(first_binding->second, second_binding->second);
        route_hit(second_binding->second, first_binding->second);
    }

    }
    catch (...)
    {
        _dispatching = false;
        flush_listener_operations();
        throw;
    }
    _dispatching = false;
    flush_listener_operations();
}

void GameplayCollisionRuntime::flush_listener_operations()
{
    for (const auto& [listener, add] : _pending_listener_operations)
    {
        const auto found = std::ranges::find(_listeners, listener);
        if (add && found == _listeners.end())
            _listeners.push_back(listener);
        else if (!add && found != _listeners.end())
            _listeners.erase(found);
    }
    _pending_listener_operations.clear();
}
}
