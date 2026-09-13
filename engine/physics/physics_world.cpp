#include "physics_world.h"

#include "collision/default_collision_strategies.h"
#include "tile/tile_coordinate_range.h"
#include "tile/tile_geometry.h"
#include "collision/world_shape.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace elysia::physics
{
namespace
{
[[nodiscard]] bool valid_config(const PhysicsWorldConfig& config) noexcept
{
    return std::isfinite(config.fixed_delta_seconds)
        && config.fixed_delta_seconds > 0.0
        && config.max_steps_per_advance > 0
        && config.solver_iterations > 0
        && config.max_ccd_iterations > 0
        && config.max_tile_candidates_per_operation > 0
        && finite_vector(config.gravity)
        && std::isfinite(config.collision_epsilon)
        && config.collision_epsilon > 0.0f
        && std::isfinite(config.penetration_slop)
        && config.penetration_slop >= 0.0f
        && std::isfinite(config.position_correction_percent)
        && config.position_correction_percent >= 0.0f
        && config.position_correction_percent <= 1.0f
        && std::isfinite(config.contact_normal_threshold)
        && config.contact_normal_threshold >= 0.0f
        && config.contact_normal_threshold <= 1.0f
        && std::isfinite(config.restitution_velocity_threshold)
        && config.restitution_velocity_threshold >= 0.0f;
}

[[nodiscard]] bool valid_tile_world(const ITileCollisionWorld& world) noexcept
{
    const auto origin = world.world_origin();
    const auto size = world.tile_size();
    return finite_vector(origin)
        && finite_vector(size)
        && size.x > elysia::core::Vector2::k_epsilon
        && size.y > elysia::core::Vector2::k_epsilon
        && world.columns() >= 0
        && world.rows() >= 0;
}


}

PhysicsWorld::PhysicsWorld(PhysicsWorldConfig config)
    : PhysicsWorld(config, make_default_collision_strategies())
{
}

PhysicsWorld::PhysicsWorld(
    PhysicsWorldConfig config,
    CollisionStrategySet strategies)
    : _config(config),
      _collision_system(std::move(strategies))
{
    if (!valid_config(_config))
        throw std::invalid_argument("PhysicsWorld requires a valid configuration.");
}

PhysicsWorld::~PhysicsWorld()
{
    reset();
}

std::optional<PhysicsWorld::Registration> PhysicsWorld::prepare_registration(
    elysia::core::GameObject& owner,
    PhysicsBodyProvider* body_provider,
    ColliderProvider* collider_provider)
{
    if (!body_provider && !collider_provider)
        return std::nullopt;
    PhysicsBody* body = body_provider ? body_provider->physics_body() : nullptr;
    const std::span<Collider> collider_span = collider_provider
        ? collider_provider->colliders() : std::span<Collider>{};
    if (!body && collider_span.empty())
        return std::nullopt;
    if (std::ranges::any_of(collider_span, [](const Collider& collider)
        { return collider.id != InvalidColliderId; }))
    {
        return std::nullopt;
    }
    if (_next_object_handle == 0
        || _next_object_handle == std::numeric_limits<std::uint64_t>::max())
    {
        return std::nullopt;
    }
    const std::size_t count = collider_span.size();
    if (count > 0
        && (_next_collider_id == InvalidColliderId
            || count - 1 > std::numeric_limits<ColliderId>::max() - _next_collider_id))
    {
        return std::nullopt;
    }

    Registration registration;
    registration.handle = PhysicsObjectHandle{_next_object_handle++};
    registration.owner = &owner;
    registration.body_provider = body_provider;
    registration.collider_provider = collider_provider;
    registration.body = body;
    registration.step_participant = dynamic_cast<PhysicsStepParticipant*>(&owner);
    registration.previous_owner_origin = owner.position();
    registration.current_owner_origin = owner.position();
    registration.colliders.reserve(count);
    ColliderId id = _next_collider_id;
    for (Collider& collider : collider_span)
    {
        collider.id = id++;
        registration.colliders.push_back(&collider);
    }
    _next_collider_id = id;
    return registration;
}

void PhysicsWorld::commit_registration(Registration registration)
{
    const auto handle = registration.handle;
    try
    {
        _registrations.push_back(std::move(registration));
        Registration& committed = _registrations.back();
        _registration_index.emplace(handle.value, _registrations.size() - 1);
        for (Collider* collider : committed.colliders)
            _collider_index.emplace(collider->id, ColliderRecord{collider, handle});
    }
    catch (...)
    {
        _registration_index.erase(handle.value);
        if (!_registrations.empty() && _registrations.back().handle == handle)
        {
            for (Collider* collider : _registrations.back().colliders)
            {
                if (collider)
                {
                    _collider_index.erase(collider->id);
                    collider->id = InvalidColliderId;
                }
            }
            _registrations.pop_back();
        }
        else
        {
            for (Collider* collider : registration.colliders)
                if (collider) collider->id = InvalidColliderId;
        }
        throw;
    }
}

PhysicsObjectHandle PhysicsWorld::register_object(
    elysia::core::GameObject& owner,
    PhysicsBodyProvider* body_provider,
    ColliderProvider* collider_provider)
{
    const auto matches = [&](const Registration& registration)
    {
        return registration.owner == &owner;
    };
    const auto active = std::ranges::find_if(_registrations, matches);
    if (active != _registrations.end())
    {
        if (pending_removal(active->handle))
            return InvalidPhysicsObjectHandle;
        return active->body_provider == body_provider
            && active->collider_provider == collider_provider
            ? active->handle : InvalidPhysicsObjectHandle;
    }
    const auto pending = std::ranges::find_if(_pending_registrations, matches);
    if (pending != _pending_registrations.end())
    {
        if (pending_removal(pending->handle))
            return InvalidPhysicsObjectHandle;
        return pending->body_provider == body_provider
            && pending->collider_provider == collider_provider
            ? pending->handle : InvalidPhysicsObjectHandle;
    }

    auto registration = prepare_registration(owner, body_provider, collider_provider);
    if (!registration)
        return InvalidPhysicsObjectHandle;
    const auto handle = registration->handle;
    if (_advancing)
    {
        try
        {
            _pending_operations.reserve(_pending_operations.size() + 1);
            _pending_registrations.push_back(std::move(*registration));
            _pending_operations.push_back(PendingRegistration{handle});
        }
        catch (...)
        {
            for (Collider* collider : registration->colliders)
                if (collider) collider->id = InvalidColliderId;
            throw;
        }
    }
    else
    {
        commit_registration(std::move(*registration));
    }
    return handle;
}

bool PhysicsWorld::unregister_immediate(PhysicsObjectHandle handle)
{
    const auto found = std::ranges::find(_registrations, handle, &Registration::handle);
    if (found == _registrations.end())
        return false;
    for (Collider* collider : found->colliders)
    {
        if (!collider)
            continue;
        remove_cached_target(CollisionTarget::from_collider(collider->id));
        _collider_index.erase(collider->id);
        collider->id = InvalidColliderId;
    }
    if (found->owner)
        found->owner->_render_offset = {};
    const auto removed_index = static_cast<std::size_t>(found - _registrations.begin());
    _registrations.erase(found);
    _registration_index.erase(handle.value);
    for (std::size_t i = removed_index; i < _registrations.size(); ++i)
        _registration_index.at(_registrations[i].handle.value) = i;
    return true;
}

bool PhysicsWorld::unregister_object(PhysicsObjectHandle handle)
{
    if (!handle.is_valid() || !contains_object(handle) || pending_removal(handle))
        return false;
    if (_advancing)
    {
        _pending_operations.push_back(PendingUnregistration{handle});
        return true;
    }
    return unregister_immediate(handle);
}

bool PhysicsWorld::contains_object(PhysicsObjectHandle handle) const noexcept
{
    return find_registration(handle) != nullptr
        || std::ranges::find(_pending_registrations, handle, &Registration::handle)
            != _pending_registrations.end();
}

bool PhysicsWorld::contains_object(const elysia::core::GameObject& owner) const noexcept
{
    const auto owns = [&owner](const Registration& value) { return value.owner == &owner; };
    return std::ranges::any_of(_registrations, owns)
        || std::ranges::any_of(_pending_registrations, owns);
}

bool PhysicsWorld::contains_collider(ColliderId collider) const noexcept
{
    if (collider == InvalidColliderId)
        return false;
    if (_collider_index.contains(collider))
        return true;
    return std::ranges::any_of(_pending_registrations, [collider](const Registration& r)
    {
        return std::ranges::any_of(r.colliders, [collider](const Collider* value)
        { return value && value->id == collider; });
    });
}

std::size_t PhysicsWorld::registered_object_count() const noexcept
{
    return _registrations.size();
}

std::size_t PhysicsWorld::registered_collider_count() const noexcept
{
    return _collider_index.size();
}

bool PhysicsWorld::set_tile_world(const ITileCollisionWorld& world)
{
    if (!valid_tile_world(world))
        return false;
    const ITileCollisionWorld* logical = logical_tile_world();
    if (logical)
        return logical == &world;
    if (_advancing)
    {
        try { _pending_operations.push_back(PendingTileOperation{&world}); }
        catch (...) { return false; }
    }
    else
    {
        replace_tile_world(&world);
    }
    return true;
}

bool PhysicsWorld::clear_tile_world(const ITileCollisionWorld& world)
{
    const ITileCollisionWorld* logical = logical_tile_world();
    if (logical != &world)
        return false;
    if (_advancing)
    {
        try { _pending_operations.push_back(PendingTileOperation{nullptr}); }
        catch (...) { return false; }
    }
    else
    {
        replace_tile_world(nullptr);
    }
    return true;
}

const ITileCollisionWorld* PhysicsWorld::tile_world() const noexcept
{
    return _tile_world;
}

bool PhysicsWorld::add_listener(ICollisionListener& listener) noexcept
{
    if (_advancing)
    {
        try { _pending_operations.push_back(PendingListenerOperation{&listener, true}); }
        catch (...) { return false; }
        return true;
    }
    if (std::ranges::find(_listeners, &listener) != _listeners.end())
        return true;
    try { _listeners.push_back(&listener); }
    catch (...) { return false; }
    return true;
}

bool PhysicsWorld::remove_listener(const ICollisionListener& listener) noexcept
{
    if (_advancing)
    {
        try
        {
            _pending_operations.push_back(PendingListenerOperation{
                const_cast<ICollisionListener*>(&listener), false});
        }
        catch (...) { return false; }
        return true;
    }
    const auto found = std::ranges::find(_listeners, &listener);
    if (found == _listeners.end())
        return false;
    _listeners.erase(found);
    return true;
}

bool PhysicsWorld::teleport_immediate(
    PhysicsObjectHandle handle,
    elysia::core::Vector2 position,
    TeleportVelocityMode velocity_mode)
{
    Registration* registration = find_registration(handle);
    if (!registration || !registration->owner)
        return false;
    registration->previous_owner_origin = position;
    registration->current_owner_origin = position;
    registration->owner->set_position(position);
    if (velocity_mode == TeleportVelocityMode::Clear && registration->body)
        registration->body->velocity = {};
    for (const Collider* collider : registration->colliders)
        if (collider) remove_cached_target(CollisionTarget::from_collider(collider->id));
    return true;
}

bool PhysicsWorld::teleport_object(
    PhysicsObjectHandle handle,
    elysia::core::Vector2 position,
    TeleportVelocityMode velocity_mode)
{
    if (!handle.is_valid() || !finite_vector(position) || !contains_object(handle)
        || pending_removal(handle))
        return false;
    if (_advancing)
    {
        _pending_operations.push_back(PendingTeleport{handle, position, velocity_mode});
        return true;
    }
    return teleport_immediate(handle, position, velocity_mode);
}

std::optional<OneWayCollision> PhysicsWorld::one_way_for_target(
    CollisionTarget target) const noexcept
{
    if (target.kind == CollisionTargetKind::Collider)
    {
        const auto found = _collider_index.find(target.collider);
        return found == _collider_index.end() || !found->second.collider
            ? std::nullopt : found->second.collider->one_way;
    }
    if (target.kind == CollisionTargetKind::Tile && _tile_world)
    {
        const auto cell = detail::tile_cell(*_tile_world, target.tile);
        return cell.type == TileCollisionType::OneWay ? cell.one_way : std::nullopt;
    }
    return std::nullopt;
}

bool PhysicsWorld::request_pass_through(ColliderId actor, CollisionTarget support)
{
    const CollisionTarget actor_target = CollisionTarget::from_collider(actor);
    if (!contains_collider(actor) || !support.is_valid() || actor_target == support
        || !one_way_for_target(support))
    {
        return false;
    }
    const CollisionPair requested = normalized_collision_pair(actor_target, support);
    const auto current = _contact_cache.contacts();
    const auto requested_contact = std::ranges::find(
        current, requested, &CollisionContact::pair);
    if (requested_contact == current.end()
        || requested_contact->response != CollisionResponse::Block)
        return false;

    const auto target_bounds = [&](CollisionTarget target)
        -> std::optional<elysia::core::Rect>
    {
        if (target.kind == CollisionTargetKind::Tile && _tile_world)
            return detail::tile_rect(*_tile_world, target.tile);
        if (target.kind != CollisionTargetKind::Collider)
            return std::nullopt;
        const auto collider = _collider_index.find(target.collider);
        const Registration* registration = find_registration(target.collider);
        if (collider == _collider_index.end() || !collider->second.collider || !registration)
            return std::nullopt;
        const auto shape = make_world_shape(
            collider->second.collider->shape, registration->current_owner_origin);
        return shape ? std::optional<elysia::core::Rect>{shape_bounds(*shape)}
            : std::nullopt;
    };
    const auto actor_normal = [&](const CollisionContact& contact)
    {
        return contact.pair.first == actor_target
            ? contact.manifold.normal : -contact.manifold.normal;
    };
    const auto requested_one_way = one_way_for_target(support);
    const auto requested_bounds = target_bounds(support);
    if (!requested_one_way || !requested_bounds)
        return false;
    const auto requested_normal = actor_normal(*requested_contact);
    const auto support_plane = [](const elysia::core::Rect& bounds,
                                  elysia::core::Vector2 normal) noexcept
    {
        if (std::fabs(normal.x) >= std::fabs(normal.y))
            return normal.x >= 0.0f ? bounds.left() : bounds.right();
        return normal.y >= 0.0f ? bounds.top() : bounds.bottom();
    };
    const float requested_plane = support_plane(*requested_bounds, requested_normal);
    bool inserted = false;
    for (const CollisionContact& contact : current)
    {
        if (contact.response != CollisionResponse::Block)
            continue;
        CollisionTarget other;
        if (contact.pair.first == actor_target)
            other = contact.pair.second;
        else if (contact.pair.second == actor_target)
            other = contact.pair.first;
        else
            continue;
        const auto other_one_way = one_way_for_target(other);
        const auto other_bounds = target_bounds(other);
        if (!other_one_way || !other_bounds
            || other_one_way->pass_through != requested_one_way->pass_through)
            continue;
        const auto normal = actor_normal(contact);
        if (normal.dot(requested_normal) < 1.0f - _config.collision_epsilon)
            continue;
        const float tolerance = std::max({
            requested_one_way->tolerance,
            other_one_way->tolerance,
            _config.collision_epsilon});
        if (std::fabs(support_plane(*other_bounds, normal) - requested_plane) > tolerance)
            continue;
        const bool adjacent = std::fabs(normal.x) >= std::fabs(normal.y)
            ? other_bounds->bottom() >= requested_bounds->top() - tolerance
                && other_bounds->top() <= requested_bounds->bottom() + tolerance
            : other_bounds->right() >= requested_bounds->left() - tolerance
                && other_bounds->left() <= requested_bounds->right() + tolerance;
        if (!adjacent)
            continue;
        _transient_ignored_pairs.push_back(
            normalized_collision_pair(actor_target, other));
        inserted = true;
    }
    std::ranges::sort(_transient_ignored_pairs);
    _transient_ignored_pairs.erase(
        std::unique(_transient_ignored_pairs.begin(), _transient_ignored_pairs.end()),
        _transient_ignored_pairs.end());
    return inserted;
}

void PhysicsWorld::collect_contacts(
    CollisionTarget target,
    std::vector<CollisionContact>& out_contacts) const
{
    _contact_cache.collect_contacts(target, out_contacts);
}

PhysicsContactState PhysicsWorld::contact_state(PhysicsObjectHandle object) const noexcept
{
    PhysicsContactState result;
    const Registration* registration = find_registration(object);
    if (!registration)
        return result;
    for (const Collider* collider : registration->colliders)
    {
        if (!collider)
            continue;
        const auto state = contact_state(CollisionTarget::from_collider(collider->id));
        result.grounded |= state.grounded;
        result.ceiling |= state.ceiling;
        result.wall_right |= state.wall_right;
        result.wall_left |= state.wall_left;
    }
    return result;
}

PhysicsContactState PhysicsWorld::contact_state(CollisionTarget target) const noexcept
{
    PhysicsContactState result;
    if (!target.is_valid()) return result;
    for (const auto& contact : _contact_cache.contacts())
    {
        if (contact.response != CollisionResponse::Block) continue;
        elysia::core::Vector2 normal;
        if (contact.pair.first == target) normal = contact.manifold.normal;
        else if (contact.pair.second == target) normal = -contact.manifold.normal;
        else continue;
        result.grounded |= normal.y >= _config.contact_normal_threshold;
        result.ceiling |= normal.y <= -_config.contact_normal_threshold;
        result.wall_right |= normal.x >= _config.contact_normal_threshold;
        result.wall_left |= normal.x <= -_config.contact_normal_threshold;
    }
    return result;
}

std::uint32_t PhysicsWorld::advance(double frame_delta_seconds)
{
    if (_advancing || !std::isfinite(frame_delta_seconds) || frame_delta_seconds <= 0.0)
        return 0;
    const double total = _accumulator_seconds + frame_delta_seconds;
    if (!std::isfinite(total))
        return 0;
    _accumulator_seconds = total;
    _advancing = true;
    std::uint32_t steps = 0;
    try
    {
        while (_accumulator_seconds + std::numeric_limits<double>::epsilon()
                >= _config.fixed_delta_seconds
            && steps < _config.max_steps_per_advance)
        {
            _accumulator_seconds -= _config.fixed_delta_seconds;
            ++steps;
            if (fixed_step(_config.fixed_delta_seconds))
                break;
        }
        if (_accumulator_seconds >= _config.fixed_delta_seconds)
        {
            const double dropped_value = std::floor(
                _accumulator_seconds / _config.fixed_delta_seconds);
            const auto available = std::numeric_limits<std::uint64_t>::max()
                - _dropped_fixed_steps;
            const auto dropped = dropped_value >= static_cast<double>(available)
                ? available
                : static_cast<std::uint64_t>(dropped_value);
            _dropped_fixed_steps += dropped;
            _accumulator_seconds = std::fmod(
                _accumulator_seconds, _config.fixed_delta_seconds);
            _last_step_stats.dropped_fixed_steps = _dropped_fixed_steps;
        }
        (void)flush_pending_operations();
    }
    catch (...)
    {
        _advancing = false;
        throw;
    }
    _advancing = false;
    const float alpha = static_cast<float>(std::clamp(
        _accumulator_seconds / _config.fixed_delta_seconds, 0.0, 1.0));
    for (const Registration& registration : _registrations)
    {
        auto* owner = registration.owner;
        if (!owner)
            continue;
        owner->_render_offset = {};
        if (!owner->is_active() || owner->is_destroyed() || !registration.body
            || !registration.body->enabled || registration.body->type == BodyType::Static
            || owner->position() != registration.current_owner_origin)
            continue;
        owner->_render_offset = (registration.previous_owner_origin
            - registration.current_owner_origin) * (1.0f - alpha);
    }
    return steps;
}

bool PhysicsWorld::fixed_step(double fixed_delta_seconds)
{
    // Registry mutations remain deferred while callbacks run. Commit them before
    // constructing the step snapshot, so spawned/teleported objects are coherent.
    for (const Registration& registration : _registrations)
    {
        if (_pending_reset)
            break;
        if (registration.owner && registration.owner->is_active()
            && !registration.owner->is_destroyed() && registration.step_participant
            && !pending_removal(registration.handle))
            registration.step_participant->fixed_update(fixed_delta_seconds);
    }
    if (flush_pending_operations())
        return true;

    std::vector<PhysicsObjectHandle> destroyed;
    for (const Registration& registration : _registrations)
        if (!registration.owner || registration.owner->is_destroyed())
            destroyed.push_back(registration.handle);
    for (PhysicsObjectHandle handle : destroyed)
        (void)unregister_immediate(handle);

    std::vector<PhysicsObjectState> states;
    states.reserve(_registrations.size());
    for (Registration& registration : _registrations)
    {
        // Direct position changes are teleports, not motion to interpolate across.
        registration.previous_owner_origin =
            registration.owner->position() == registration.current_owner_origin
            ? registration.current_owner_origin : registration.owner->position();
        registration.current_owner_origin = registration.owner->position();
        if (!registration.owner->is_active())
            continue;
        states.push_back({
            registration.handle,
            registration.body,
            registration.previous_owner_origin,
            registration.current_owner_origin
        });
    }
    _physics_system.integrate(states, _config, fixed_delta_seconds);

    std::vector<CollisionShapeView> views;
    views.reserve(_collider_index.size());
    for (const PhysicsObjectState& state : states)
    {
        const Registration* registration = find_registration(state.object);
        if (!registration)
            continue;
        for (const Collider* collider : registration->colliders)
        {
            if (!collider || !collider->enabled || !valid_shape(collider->shape))
                continue;
            const auto previous = make_world_shape(collider->shape, state.previous_owner_origin);
            const auto current = make_world_shape(collider->shape, state.current_owner_origin);
            if (!previous || !current)
                continue;
            views.push_back({
                CollisionTarget::from_collider(collider->id), state.object,
                *previous, *current, shape_bounds(*current),
                swept_shape_bounds(*previous, *current),
                state.previous_owner_origin, state.current_owner_origin,
                collider->filter, collider->response,
                collider->detection_mode, collider->one_way,
                collider->material
            });
        }
    }
    std::ranges::sort(views, {}, &CollisionShapeView::target);

    _last_step_stats = {};
    _last_step_stats.registered_objects = _registrations.size();
    _last_step_stats.registered_colliders = _collider_index.size();
    _last_step_stats.dropped_fixed_steps = _dropped_fixed_steps;
    PhysicsDebugSnapshot* debug_snapshot = _debug_capture != PhysicsDebugCapture::None
        ? &_debug_snapshot
        : nullptr;
    _collision_system.evaluate(
        states, views, _tile_world, _transient_ignored_pairs, _config,
        fixed_delta_seconds, _collision_frame, _last_step_stats,
        _debug_capture, debug_snapshot);

    if (debug_snapshot
        && captures_physics_debug(_debug_capture, PhysicsDebugCapture::Velocities))
    {
        for (const PhysicsObjectState& state : states)
        {
            if (state.body && !state.body->velocity.is_zero(_config.collision_epsilon))
            {
                debug_snapshot->velocities.push_back(
                    {state.object, state.current_owner_origin, state.body->velocity});
            }
        }
    }

    for (const PhysicsObjectState& state : states)
    {
        Registration* registration = find_registration(state.object);
        if (!registration || !registration->owner)
            continue;
        registration->current_owner_origin = state.current_owner_origin;
        registration->owner->set_position(state.current_owner_origin);
    }

    _contact_cache.update(_collision_frame.contacts, _collision_frame.events);
    std::vector<CollisionPair> retained;
    std::ranges::set_intersection(
        _transient_ignored_pairs,
        _collision_frame.ignored_pairs_overlapping,
        std::back_inserter(retained));
    _transient_ignored_pairs = std::move(retained);

    const auto listener_snapshot = _listeners;
    try
    {
        for (const CollisionEvent& event : _collision_frame.events)
            for (ICollisionListener* listener : listener_snapshot)
                if (listener) listener->on_collision_event(event);
    }
    catch (...)
    {
        (void)flush_pending_operations();
        throw;
    }
    return flush_pending_operations();
}

bool PhysicsWorld::flush_pending_operations()
{
    if (_pending_reset)
    {
        reset_immediate();
        return true;
    }
    for (const PendingOperation& command : _pending_operations)
    {
        std::visit([&](const auto& operation)
        {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<Operation, PendingRegistration>)
            {
                const auto found = std::ranges::find(
                    _pending_registrations, operation.handle, &Registration::handle);
                if (found == _pending_registrations.end())
                    return;
                if (found->owner && !found->owner->is_destroyed())
                    commit_registration(std::move(*found));
                else
                    for (Collider* collider : found->colliders)
                        if (collider) collider->id = InvalidColliderId;
                _pending_registrations.erase(found);
            }
            else if constexpr (std::is_same_v<Operation, PendingUnregistration>)
                (void)unregister_immediate(operation.handle);
            else if constexpr (std::is_same_v<Operation, PendingTeleport>)
                (void)teleport_immediate(operation.handle, operation.position, operation.velocity_mode);
            else if constexpr (std::is_same_v<Operation, PendingTileOperation>)
                replace_tile_world(operation.world);
            else
            {
                const auto found = std::ranges::find(_listeners, operation.listener);
                if (operation.add && found == _listeners.end())
                    _listeners.push_back(operation.listener);
                else if (!operation.add && found != _listeners.end())
                    _listeners.erase(found);
            }
        }, command);
    }
    _pending_operations.clear();
    return false;
}

bool PhysicsWorld::pending_removal(PhysicsObjectHandle handle) const noexcept
{
    return std::ranges::any_of(_pending_operations, [handle](const PendingOperation& command)
    {
        const auto* removal = std::get_if<PendingUnregistration>(&command);
        return removal && removal->handle == handle;
    });
}

const ITileCollisionWorld* PhysicsWorld::logical_tile_world() const noexcept
{
    for (auto it = _pending_operations.rbegin(); it != _pending_operations.rend(); ++it)
        if (const auto* change = std::get_if<PendingTileOperation>(&*it))
            return change->world;
    return _tile_world;
}

void PhysicsWorld::replace_tile_world(const ITileCollisionWorld* world)
{
    if (_tile_world == world)
        return;
    _contact_cache.invalidate_tiles();
    _tile_world = world;
    std::erase_if(_transient_ignored_pairs, [](const CollisionPair& pair)
    {
        return pair.first.kind == CollisionTargetKind::Tile
            || pair.second.kind == CollisionTargetKind::Tile;
    });
}

void PhysicsWorld::remove_cached_target(CollisionTarget target)
{
    _contact_cache.invalidate_target(target);
    std::erase_if(_transient_ignored_pairs, [target](const CollisionPair& pair)
    { return pair.first == target || pair.second == target; });
}

void PhysicsWorld::reset() noexcept
{
    if (_advancing)
    {
        _pending_reset = true;
        return;
    }
    reset_immediate();
}

void PhysicsWorld::reset_immediate() noexcept
{
    for (Registration& registration : _registrations)
    {
        if (registration.owner)
            registration.owner->_render_offset = {};
        for (Collider* collider : registration.colliders)
            if (collider) collider->id = InvalidColliderId;
    }
    for (Registration& registration : _pending_registrations)
        for (Collider* collider : registration.colliders)
            if (collider) collider->id = InvalidColliderId;
    _registrations.clear();
    _registration_index.clear();
    _pending_registrations.clear();
    _collider_index.clear();
    _listeners.clear();
    _tile_world = nullptr;
    _collision_system.clear();
    _contact_cache.clear();
    _collision_frame.clear();
    _transient_ignored_pairs.clear();
    _debug_snapshot.clear();
    _debug_capture = PhysicsDebugCapture::None;
    _last_step_stats = {};
    _pending_operations.clear();
    _accumulator_seconds = 0.0;
    _dropped_fixed_steps = 0;
    _pending_reset = false;
}

const PhysicsWorldConfig& PhysicsWorld::config() const noexcept { return _config; }
double PhysicsWorld::accumulator_seconds() const noexcept { return _accumulator_seconds; }
const PhysicsStepStats& PhysicsWorld::last_step_stats() const noexcept { return _last_step_stats; }
void PhysicsWorld::set_debug_capture(PhysicsDebugCapture capture) noexcept
{
    constexpr auto valid_bits = static_cast<std::uint8_t>(PhysicsDebugCapture::All);
    capture = static_cast<PhysicsDebugCapture>(
        static_cast<std::uint8_t>(capture) & valid_bits);
    if (_debug_capture == capture)
        return;
    _debug_capture = capture;
    _debug_snapshot.clear();
}
PhysicsDebugCapture PhysicsWorld::debug_capture() const noexcept { return _debug_capture; }
const PhysicsDebugSnapshot& PhysicsWorld::debug_snapshot() const noexcept { return _debug_snapshot; }

PhysicsWorld::Registration* PhysicsWorld::find_registration(PhysicsObjectHandle handle) noexcept
{
    const auto found = _registration_index.find(handle.value);
    return found == _registration_index.end() ? nullptr : &_registrations[found->second];
}

const PhysicsWorld::Registration* PhysicsWorld::find_registration(
    PhysicsObjectHandle handle) const noexcept
{
    const auto found = _registration_index.find(handle.value);
    return found == _registration_index.end() ? nullptr : &_registrations[found->second];
}

const PhysicsWorld::Registration* PhysicsWorld::find_registration(
    ColliderId collider) const noexcept
{
    const auto found = _collider_index.find(collider);
    return found == _collider_index.end() ? nullptr : find_registration(found->second.owner);
}
}
