#include "physics_world.h"

#include "collision/default_collision_strategies.h"
#include "tile/tile_coordinate_range.h"
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

[[nodiscard]] TileCollisionCell tile_cell(
    const ITileCollisionWorld& world,
    TileCoordinate coordinate) noexcept
{
    if (coordinate.x < 0 || coordinate.y < 0
        || coordinate.x >= world.columns() || coordinate.y >= world.rows())
    {
        TileCollisionCell cell;
        cell.type = world.out_of_bounds_policy() == TileOutOfBoundsPolicy::Block
            ? TileCollisionType::Block
            : TileCollisionType::Empty;
        return cell;
    }
    return world.cell_at(coordinate);
}

[[nodiscard]] elysia::core::Rect tile_rect(
    const ITileCollisionWorld& world,
    TileCoordinate coordinate) noexcept
{
    const auto origin = world.world_origin();
    const auto size = world.tile_size();
    return {
        origin.x + static_cast<float>(coordinate.x) * size.x,
        origin.y + static_cast<float>(coordinate.y) * size.y,
        size.x,
        size.y
    };
}

struct RayShapeHit
{
    float distance = 0.0f;
    elysia::core::Vector2 normal{};
};

[[nodiscard]] std::optional<RayShapeHit> ray_aabb(
    elysia::core::Vector2 origin,
    elysia::core::Vector2 direction,
    float max_distance,
    const elysia::core::Rect& rect,
    float epsilon) noexcept
{
    float enter = 0.0f;
    float exit = max_distance;
    elysia::core::Vector2 enter_normal{};
    const auto axis = [&](float ray_origin, float ray_direction, float minimum,
                          float maximum, elysia::core::Vector2 negative_normal,
                          elysia::core::Vector2 positive_normal) -> bool
    {
        if (std::fabs(ray_direction) <= epsilon)
            return ray_origin >= minimum && ray_origin <= maximum;
        float first = (minimum - ray_origin) / ray_direction;
        float second = (maximum - ray_origin) / ray_direction;
        auto normal = negative_normal;
        if (first > second)
        {
            std::swap(first, second);
            normal = positive_normal;
        }
        if (first > enter)
        {
            enter = first;
            enter_normal = normal;
        }
        exit = std::min(exit, second);
        return enter <= exit + epsilon;
    };
    if (!axis(origin.x, direction.x, rect.left(), rect.right(), {-1.0f, 0.0f}, {1.0f, 0.0f})
        || !axis(origin.y, direction.y, rect.top(), rect.bottom(), {0.0f, -1.0f}, {0.0f, 1.0f})
        || exit < 0.0f || enter > max_distance + epsilon)
    {
        return std::nullopt;
    }
    if (rect.contains(origin))
        return RayShapeHit{0.0f, -direction};
    return RayShapeHit{std::max(0.0f, enter), enter_normal};
}

[[nodiscard]] std::optional<RayShapeHit> ray_circle(
    elysia::core::Vector2 origin,
    elysia::core::Vector2 direction,
    float max_distance,
    const WorldCircle& circle,
    float epsilon) noexcept
{
    const auto offset = origin - circle.center;
    const float c = offset.length_squared() - circle.radius * circle.radius;
    if (c <= 0.0f)
        return RayShapeHit{0.0f, -direction};
    const float b = offset.dot(direction);
    const float discriminant = b * b - c;
    if (discriminant < -epsilon)
        return std::nullopt;
    const float distance = -b - std::sqrt(std::max(0.0f, discriminant));
    if (distance < -epsilon || distance > max_distance + epsilon)
        return std::nullopt;
    const auto point = origin + direction * std::max(0.0f, distance);
    return RayShapeHit{
        std::max(0.0f, distance),
        (point - circle.center).normalized(epsilon)
    };
}

[[nodiscard]] bool query_filter_allows(
    const CollisionFilter& query,
    const CollisionFilter& target) noexcept
{
    return collision_filters_allow(query, target);
}

[[nodiscard]] bool finite_rect(const elysia::core::Rect& rect) noexcept
{
    return finite_vector(rect.position())
        && finite_vector(rect.size());
}

[[nodiscard]] CollisionResponse tile_response(
    const TileCollisionCell& cell) noexcept
{
    if (cell.type == TileCollisionType::Empty)
        return CollisionResponse::Ignore;
    return cell.type == TileCollisionType::Overlap
        ? CollisionResponse::Overlap
        : CollisionResponse::Block;
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
        for (Collider* collider : committed.colliders)
            _collider_index.emplace(collider->id, collider);
    }
    catch (...)
    {
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
        return active->body_provider == body_provider
            && active->collider_provider == collider_provider
            ? active->handle : InvalidPhysicsObjectHandle;
    }
    const auto pending = std::ranges::find_if(_pending_registrations, matches);
    if (pending != _pending_registrations.end())
    {
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
            _pending_registrations.push_back(std::move(*registration));
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

bool PhysicsWorld::unregister_immediate(PhysicsObjectHandle handle) noexcept
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
    _registrations.erase(found);
    return true;
}

bool PhysicsWorld::unregister_object(PhysicsObjectHandle handle)
{
    if (!handle.is_valid())
        return false;
    const auto pending = std::ranges::find(
        _pending_registrations, handle, &Registration::handle);
    if (pending != _pending_registrations.end())
    {
        for (Collider* collider : pending->colliders)
            if (collider) collider->id = InvalidColliderId;
        _pending_registrations.erase(pending);
        return true;
    }
    if (!contains_object(handle))
        return false;
    if (_advancing)
    {
        if (std::ranges::find(_pending_unregistrations, handle)
            == _pending_unregistrations.end())
        {
            _pending_unregistrations.push_back(handle);
        }
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

bool PhysicsWorld::set_tile_world(const ITileCollisionWorld& world) noexcept
{
    if (!valid_tile_world(world))
        return false;
    const ITileCollisionWorld* logical = _pending_tile_operation == PendingTileOperation::Set
        ? _pending_tile_world
        : (_pending_tile_operation == PendingTileOperation::Clear ? nullptr : _tile_world);
    if (logical)
        return logical == &world;
    if (_advancing)
    {
        _pending_tile_operation = PendingTileOperation::Set;
        _pending_tile_world = &world;
    }
    else
    {
        _tile_world = &world;
    }
    return true;
}

bool PhysicsWorld::clear_tile_world(const ITileCollisionWorld& world) noexcept
{
    const ITileCollisionWorld* logical = _pending_tile_operation == PendingTileOperation::Set
        ? _pending_tile_world
        : (_pending_tile_operation == PendingTileOperation::Clear ? nullptr : _tile_world);
    if (logical != &world)
        return false;
    if (_advancing)
    {
        _pending_tile_operation = PendingTileOperation::Clear;
        _pending_tile_world = &world;
    }
    else
    {
        _tile_world = nullptr;
        _contact_cache.remove_tiles();
        std::erase_if(_transient_ignored_pairs, [](const CollisionPair& pair)
        {
            return pair.first.kind == CollisionTargetKind::Tile
                || pair.second.kind == CollisionTargetKind::Tile;
        });
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
        try { _pending_listener_operations.push_back({&listener, true}); }
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
            _pending_listener_operations.push_back(
                {const_cast<ICollisionListener*>(&listener), false});
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
    TeleportVelocityMode velocity_mode) noexcept
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
    if (!handle.is_valid() || !finite_vector(position) || !contains_object(handle))
        return false;
    if (_advancing)
    {
        _pending_teleports.push_back({handle, position, velocity_mode});
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
        return found == _collider_index.end() || !found->second
            ? std::nullopt : found->second->one_way;
    }
    if (target.kind == CollisionTargetKind::Tile && _tile_world)
    {
        const auto cell = tile_cell(*_tile_world, target.tile);
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
            return tile_rect(*_tile_world, target.tile);
        if (target.kind != CollisionTargetKind::Collider)
            return std::nullopt;
        const auto collider = _collider_index.find(target.collider);
        const Registration* registration = find_registration(target.collider);
        if (collider == _collider_index.end() || !collider->second || !registration)
            return std::nullopt;
        const auto shape = make_world_shape(
            collider->second->shape, registration->current_owner_origin);
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
        const auto target = CollisionTarget::from_collider(collider->id);
        for (const CollisionContact& contact : _contact_cache.contacts())
        {
            if (contact.response != CollisionResponse::Block)
                continue;
            elysia::core::Vector2 normal;
            if (contact.pair.first == target)
                normal = contact.manifold.normal;
            else if (contact.pair.second == target)
                normal = -contact.manifold.normal;
            else
                continue;
            result.grounded |= normal.y >= _config.contact_normal_threshold;
            result.ceiling |= normal.y <= -_config.contact_normal_threshold;
            result.wall_right |= normal.x >= _config.contact_normal_threshold;
            result.wall_left |= normal.x <= -_config.contact_normal_threshold;
        }
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
    return steps;
}

bool PhysicsWorld::fixed_step(double fixed_delta_seconds)
{
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
        registration.previous_owner_origin = registration.current_owner_origin;
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
    for (PhysicsObjectHandle handle : _pending_unregistrations)
        (void)unregister_immediate(handle);
    _pending_unregistrations.clear();
    for (const PendingTeleport& teleport : _pending_teleports)
        (void)teleport_immediate(teleport.handle, teleport.position, teleport.velocity_mode);
    _pending_teleports.clear();
    for (Registration& registration : _pending_registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed())
        {
            for (Collider* collider : registration.colliders)
                if (collider) collider->id = InvalidColliderId;
            continue;
        }
        commit_registration(std::move(registration));
    }
    _pending_registrations.clear();
    for (const PendingListenerOperation& operation : _pending_listener_operations)
    {
        const auto found = std::ranges::find(_listeners, operation.listener);
        if (operation.add)
        {
            if (found == _listeners.end())
                _listeners.push_back(operation.listener);
        }
        else if (found != _listeners.end())
        {
            _listeners.erase(found);
        }
    }
    _pending_listener_operations.clear();
    if (_pending_tile_operation == PendingTileOperation::Set)
        _tile_world = _pending_tile_world;
    else if (_pending_tile_operation == PendingTileOperation::Clear)
    {
        _tile_world = nullptr;
        _contact_cache.remove_tiles();
        std::erase_if(_transient_ignored_pairs, [](const CollisionPair& pair)
        {
            return pair.first.kind == CollisionTargetKind::Tile
                || pair.second.kind == CollisionTargetKind::Tile;
        });
    }
    _pending_tile_operation = PendingTileOperation::None;
    _pending_tile_world = nullptr;
    return false;
}

void PhysicsWorld::remove_cached_target(CollisionTarget target) noexcept
{
    _contact_cache.remove_target(target);
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
        for (Collider* collider : registration.colliders)
            if (collider) collider->id = InvalidColliderId;
    for (Registration& registration : _pending_registrations)
        for (Collider* collider : registration.colliders)
            if (collider) collider->id = InvalidColliderId;
    _registrations.clear();
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
    _pending_unregistrations.clear();
    _pending_listener_operations.clear();
    _pending_teleports.clear();
    _pending_tile_operation = PendingTileOperation::None;
    _pending_tile_world = nullptr;
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

std::optional<CollisionQueryHit> PhysicsWorld::raycast(const RayCastQuery& query) const
{
    std::vector<CollisionQueryHit> hits;
    raycast_all(query, hits);
    return hits.empty() ? std::nullopt
        : std::optional<CollisionQueryHit>{hits.front()};
}

void PhysicsWorld::raycast_all(
    const RayCastQuery& query,
    std::vector<CollisionQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_vector(query.origin) || !finite_vector(query.direction)
        || !std::isfinite(query.max_distance) || query.max_distance < 0.0f)
        return;
    const auto direction = query.direction.normalized(_config.collision_epsilon);
    if (direction.is_zero(_config.collision_epsilon))
        return;
    const auto consider = [&](CollisionTarget target,
                              CollisionResponse response,
                              const RayShapeHit& hit)
    {
        if (response == CollisionResponse::Ignore
            || hit.distance > query.max_distance + _config.collision_epsilon)
            return;
        out_hits.push_back(CollisionQueryHit{
            target,
            query.origin + direction * hit.distance,
            hit.normal,
            hit.distance,
            query.max_distance > 0.0f
                ? hit.distance / query.max_distance
                : 0.0f,
            response
        });
    };
    for (const Registration& registration : _registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed()
            || !registration.owner->is_active())
            continue;
        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled || collider->response == CollisionResponse::Ignore
                || !query_filter_allows(query.filter, collider->filter))
                continue;
            const auto shape = make_world_shape(
                collider->shape, registration.current_owner_origin);
            if (!shape)
                continue;
            std::optional<RayShapeHit> hit;
            if (const auto* box = std::get_if<WorldAabb>(&*shape))
                hit = ray_aabb(query.origin, direction, query.max_distance, box->rect, _config.collision_epsilon);
            else
                hit = ray_circle(query.origin, direction, query.max_distance,
                    std::get<WorldCircle>(*shape), _config.collision_epsilon);
            if (hit)
                consider(
                    CollisionTarget::from_collider(collider->id),
                    collider->response,
                    *hit);
        }
    }
    if (_tile_world)
    {
        const auto map_origin = _tile_world->world_origin();
        const auto size = _tile_world->tile_size();
        const auto initial_coordinate = checked_world_to_tile(
            query.origin, map_origin, size);
        if (!initial_coordinate)
            goto finish_raycast;
        TileCoordinate coordinate = *initial_coordinate;
        const int step_x = direction.x > 0.0f ? 1 : (direction.x < 0.0f ? -1 : 0);
        const int step_y = direction.y > 0.0f ? 1 : (direction.y < 0.0f ? -1 : 0);
        const float infinity = std::numeric_limits<float>::infinity();
        const auto next_grid_line = [](float map_origin,
                                       float tile_extent,
                                       int coordinate_value,
                                       bool positive_step) noexcept
        {
            const std::int64_t grid_index = static_cast<std::int64_t>(coordinate_value)
                + (positive_step ? 1 : 0);
            return static_cast<float>(
                static_cast<double>(map_origin)
                + static_cast<double>(grid_index) * static_cast<double>(tile_extent));
        };
        float next_x = step_x == 0 ? infinity :
            (next_grid_line(map_origin.x, size.x, coordinate.x, step_x > 0)
                - query.origin.x) / direction.x;
        float next_y = step_y == 0 ? infinity :
            (next_grid_line(map_origin.y, size.y, coordinate.y, step_y > 0)
                - query.origin.y) / direction.y;
        const float delta_x = step_x == 0 ? infinity : size.x / std::fabs(direction.x);
        const float delta_y = step_y == 0 ? infinity : size.y / std::fabs(direction.y);
        float entered = 0.0f;
        elysia::core::Vector2 entered_normal = -direction;
        const auto visit_tile = [&](TileCoordinate value,
                                    float distance,
                                    elysia::core::Vector2 normal)
        {
            const auto cell = tile_cell(*_tile_world, value);
            const auto response = tile_response(cell);
            if (response != CollisionResponse::Ignore
                && query_filter_allows(query.filter, cell.filter))
            {
                consider(
                    CollisionTarget::from_tile(value),
                    response,
                    {std::max(0.0f, distance), normal});
            }
        };
        std::uint32_t tile_iterations = 0;
        while (entered <= query.max_distance + _config.collision_epsilon
            && tile_iterations++ < _config.max_tile_candidates_per_operation)
        {
            visit_tile(coordinate, entered, entered_normal);
            if (std::fabs(next_x - next_y) <= _config.collision_epsilon)
            {
                const float corner_distance = next_x;
                if ((step_x > 0 && coordinate.x == std::numeric_limits<int>::max())
                    || (step_x < 0 && coordinate.x == std::numeric_limits<int>::min())
                    || (step_y > 0 && coordinate.y == std::numeric_limits<int>::max())
                    || (step_y < 0 && coordinate.y == std::numeric_limits<int>::min()))
                    break;
                if (corner_distance <= query.max_distance + _config.collision_epsilon)
                {
                    if (step_x != 0)
                        visit_tile(
                            {coordinate.x + step_x, coordinate.y},
                            corner_distance,
                            {-static_cast<float>(step_x), 0.0f});
                    if (step_y != 0)
                        visit_tile(
                            {coordinate.x, coordinate.y + step_y},
                            corner_distance,
                            {0.0f, -static_cast<float>(step_y)});
                }
                entered = corner_distance;
                next_x += delta_x;
                next_y += delta_y;
                coordinate.x += step_x;
                coordinate.y += step_y;
                entered_normal = -direction;
            }
            else if (next_x < next_y)
            {
                entered = next_x;
                next_x += delta_x;
                if ((step_x > 0 && coordinate.x == std::numeric_limits<int>::max())
                    || (step_x < 0 && coordinate.x == std::numeric_limits<int>::min()))
                    break;
                coordinate.x += step_x;
                entered_normal = {-static_cast<float>(step_x), 0.0f};
            }
            else
            {
                entered = next_y;
                next_y += delta_y;
                if ((step_y > 0 && coordinate.y == std::numeric_limits<int>::max())
                    || (step_y < 0 && coordinate.y == std::numeric_limits<int>::min()))
                    break;
                coordinate.y += step_y;
                entered_normal = {0.0f, -static_cast<float>(step_y)};
            }
            if (!std::isfinite(entered))
                break;
        }
    }

finish_raycast:
    std::ranges::stable_sort(out_hits, [](
        const CollisionQueryHit& first,
        const CollisionQueryHit& second)
    {
        if (first.distance != second.distance)
            return first.distance < second.distance;
        return first.target < second.target;
    });
    std::vector<CollisionQueryHit> unique;
    unique.reserve(out_hits.size());
    for (const CollisionQueryHit& hit : out_hits)
    {
        if (std::ranges::find(unique, hit.target, &CollisionQueryHit::target)
            == unique.end())
            unique.push_back(hit);
    }
    out_hits = std::move(unique);
}

std::optional<CollisionQueryHit> PhysicsWorld::segment_cast(const SegmentCastQuery& query) const
{
    std::vector<CollisionQueryHit> hits;
    segment_cast_all(query, hits);
    return hits.empty() ? std::nullopt
        : std::optional<CollisionQueryHit>{hits.front()};
}

void PhysicsWorld::segment_cast_all(
    const SegmentCastQuery& query,
    std::vector<CollisionQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_vector(query.start) || !finite_vector(query.end))
        return;
    const auto delta = query.end - query.start;
    const float distance = delta.length();
    if (!std::isfinite(distance) || distance <= _config.collision_epsilon)
        return;
    raycast_all(
        RayCastQuery{query.start, delta / distance, distance, query.filter},
        out_hits);
}

void PhysicsWorld::overlap_aabb(
    const AabbOverlapQuery& query,
    std::vector<CollisionOverlapQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_rect(query.bounds) || query.bounds.is_empty())
        return;

    const WorldColliderShape query_shape = WorldAabb{query.bounds};
    const auto consider = [&](CollisionTarget target,
                              CollisionResponse response,
                              const CollisionFilter& filter,
                              const WorldColliderShape& target_shape)
    {
        if (response == CollisionResponse::Ignore
            || !query_filter_allows(query.filter, filter))
            return;
        if (const auto hit = detect_discrete_shapes(
                query_shape, target_shape, _config.collision_epsilon))
            out_hits.push_back({target, hit->manifold, response});
    };
    for (const Registration& registration : _registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed()
            || !registration.owner->is_active())
            continue;
        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled)
                continue;
            const auto shape = make_world_shape(
                collider->shape, registration.current_owner_origin);
            if (shape)
                consider(
                    CollisionTarget::from_collider(collider->id),
                    collider->response,
                    collider->filter,
                    *shape);
        }
    }
    if (_tile_world)
    {
        const auto origin = _tile_world->world_origin();
        const auto size = _tile_world->tile_size();
        const auto range = checked_tile_range(
            query.bounds, origin, size, TileRangeBoundary::InclusiveTouching,
            _config.max_tile_candidates_per_operation);
        if (range)
            for (std::int64_t y = range->min_y; y <= range->max_y; ++y)
                for (std::int64_t x = range->min_x; x <= range->max_x; ++x)
                {
                    const TileCoordinate coordinate{
                        static_cast<int>(x), static_cast<int>(y)};
                    const TileCollisionCell cell = tile_cell(*_tile_world, coordinate);
                    consider(
                        CollisionTarget::from_tile(coordinate),
                        tile_response(cell),
                        cell.filter,
                        WorldAabb{tile_rect(*_tile_world, coordinate)});
                }
    }
    std::ranges::stable_sort(out_hits, {}, &CollisionOverlapQueryHit::target);
    out_hits.erase(
        std::unique(out_hits.begin(), out_hits.end(), [](const auto& first, const auto& second)
        { return first.target == second.target; }),
        out_hits.end());
}

void PhysicsWorld::overlap_circle(
    const CircleOverlapQuery& query,
    std::vector<CollisionOverlapQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_vector(query.center) || !std::isfinite(query.radius)
        || query.radius <= 0.0f)
        return;

    const WorldColliderShape query_shape = WorldCircle{query.center, query.radius};
    const auto consider = [&](CollisionTarget target,
                              CollisionResponse response,
                              const CollisionFilter& filter,
                              const WorldColliderShape& target_shape)
    {
        if (response == CollisionResponse::Ignore
            || !query_filter_allows(query.filter, filter))
            return;
        if (const auto hit = detect_discrete_shapes(
                query_shape, target_shape, _config.collision_epsilon))
            out_hits.push_back({target, hit->manifold, response});
    };
    for (const Registration& registration : _registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed()
            || !registration.owner->is_active())
            continue;
        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled)
                continue;
            const auto shape = make_world_shape(
                collider->shape, registration.current_owner_origin);
            if (shape)
                consider(
                    CollisionTarget::from_collider(collider->id),
                    collider->response,
                    collider->filter,
                    *shape);
        }
    }
    if (_tile_world)
    {
        const auto origin = _tile_world->world_origin();
        const auto size = _tile_world->tile_size();
        const elysia::core::Rect bounds = elysia::core::Rect::from_center(
            query.center, {query.radius * 2.0f, query.radius * 2.0f});
        const auto range = checked_tile_range(
            bounds, origin, size, TileRangeBoundary::InclusiveTouching,
            _config.max_tile_candidates_per_operation);
        if (range)
            for (std::int64_t y = range->min_y; y <= range->max_y; ++y)
                for (std::int64_t x = range->min_x; x <= range->max_x; ++x)
                {
                    const TileCoordinate coordinate{
                        static_cast<int>(x), static_cast<int>(y)};
                    const TileCollisionCell cell = tile_cell(*_tile_world, coordinate);
                    consider(
                        CollisionTarget::from_tile(coordinate),
                        tile_response(cell),
                        cell.filter,
                        WorldAabb{tile_rect(*_tile_world, coordinate)});
                }
    }
    std::ranges::stable_sort(out_hits, {}, &CollisionOverlapQueryHit::target);
    out_hits.erase(
        std::unique(out_hits.begin(), out_hits.end(), [](const auto& first, const auto& second)
        { return first.target == second.target; }),
        out_hits.end());
}

std::optional<CollisionQueryHit> PhysicsWorld::sweep_aabb(
    const AabbSweepQuery& query) const
{
    if (!finite_rect(query.start_bounds) || query.start_bounds.is_empty()
        || !finite_vector(query.displacement))
        return std::nullopt;
    const WorldAabb previous{query.start_bounds};
    const WorldAabb current{query.start_bounds.translated(query.displacement)};
    const elysia::core::Rect swept_bounds = query.start_bounds.merged(current.rect);
    const float travel_distance = query.displacement.length();
    std::optional<CollisionQueryHit> best;
    const auto consider = [&](CollisionTarget target,
                              CollisionResponse response,
                              const CollisionFilter& filter,
                              const WorldAabb& target_box)
    {
        if (response == CollisionResponse::Ignore
            || !query_filter_allows(query.filter, filter))
            return;
        const auto hit = detect_swept_aabbs(
            previous, current, target_box, target_box, _config.collision_epsilon);
        if (!hit)
            return;
        const float fraction = std::clamp(hit->time_of_impact, 0.0f, 1.0f);
        CollisionQueryHit candidate{
            target,
            hit->manifold.contact_point_count > 0
                ? hit->manifold.contact_points[0]
                : query.start_bounds.center() + query.displacement * fraction,
            hit->manifold.normal,
            travel_distance * fraction,
            fraction,
            response
        };
        if (!best
            || candidate.fraction + _config.collision_epsilon < best->fraction
            || (std::fabs(candidate.fraction - best->fraction)
                    <= _config.collision_epsilon
                && candidate.target < best->target))
            best = candidate;
    };
    for (const Registration& registration : _registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed()
            || !registration.owner->is_active())
            continue;
        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled
                || !query_filter_allows(query.filter, collider->filter))
                continue;
            const auto shape = make_world_shape(
                collider->shape, registration.current_owner_origin);
            if (!shape)
                continue;
            const auto* box = std::get_if<WorldAabb>(&*shape);
            if (box)
                consider(
                    CollisionTarget::from_collider(collider->id),
                    collider->response,
                    collider->filter,
                    *box);
        }
    }
    if (_tile_world)
    {
        const auto origin = _tile_world->world_origin();
        const auto size = _tile_world->tile_size();
        const auto range = checked_tile_range(
            swept_bounds, origin, size, TileRangeBoundary::InclusiveTouching,
            _config.max_tile_candidates_per_operation);
        if (!range)
            return best;
        for (std::int64_t y = range->min_y; y <= range->max_y; ++y)
            for (std::int64_t x = range->min_x; x <= range->max_x; ++x)
            {
                const TileCoordinate coordinate{
                    static_cast<int>(x), static_cast<int>(y)};
                const TileCollisionCell cell = tile_cell(*_tile_world, coordinate);
                consider(
                    CollisionTarget::from_tile(coordinate),
                    tile_response(cell),
                    cell.filter,
                    WorldAabb{tile_rect(*_tile_world, coordinate)});
            }
    }
    return best;
}

PhysicsWorld::Registration* PhysicsWorld::find_registration(PhysicsObjectHandle handle) noexcept
{
    const auto found = std::ranges::find(_registrations, handle, &Registration::handle);
    return found == _registrations.end() ? nullptr : &*found;
}

const PhysicsWorld::Registration* PhysicsWorld::find_registration(
    PhysicsObjectHandle handle) const noexcept
{
    const auto found = std::ranges::find(_registrations, handle, &Registration::handle);
    return found == _registrations.end() ? nullptr : &*found;
}

const PhysicsWorld::Registration* PhysicsWorld::find_registration(
    ColliderId collider) const noexcept
{
    for (const Registration& registration : _registrations)
        if (std::ranges::any_of(registration.colliders, [collider](const Collider* value)
            { return value && value->id == collider; }))
            return &registration;
    return nullptr;
}
}
