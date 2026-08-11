#include "physics_world.h"

#include "physics_service.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace elysia::physics
{
namespace
{
[[nodiscard]] bool finite_vector(const elysia::core::Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool valid_config(const PhysicsWorldConfig& config) noexcept
{
    return std::isfinite(config.fixed_delta_seconds)
        && config.fixed_delta_seconds > 0.0
        && config.max_steps_per_advance > 0
        && config.solver_iterations > 0
        && finite_vector(config.gravity);
}
}

PhysicsWorld::PhysicsWorld(PhysicsWorldConfig config)
    : _config(config)
{
    if (!valid_config(_config))
        throw std::invalid_argument("PhysicsWorld requires a valid fixed-step configuration.");
}

PhysicsWorld::~PhysicsWorld()
{
    reset();
}

bool PhysicsWorld::configure_strategies(const PhysicsService& service)
{
    if (_advancing)
        return false;
    return service.apply_to(_collision_system);
}

PhysicsObjectHandle PhysicsWorld::register_object(
    elysia::core::GameObject& owner,
    PhysicsBodyProvider* body_provider,
    ColliderProvider* collider_provider)
{
    if (_advancing || (!body_provider && !collider_provider))
        return InvalidPhysicsObjectHandle;

    const auto existing = std::find_if(
        _registrations.begin(),
        _registrations.end(),
        [&owner](const Registration& registration)
        {
            return registration.owner == &owner;
        });
    if (existing != _registrations.end())
    {
        return existing->body_provider == body_provider
            && existing->collider_provider == collider_provider
            ? existing->handle
            : InvalidPhysicsObjectHandle;
    }

    PhysicsBody* body = body_provider ? body_provider->physics_body() : nullptr;
    std::span<Collider> collider_span = collider_provider
        ? collider_provider->colliders()
        : std::span<Collider>{};

    if (!body && collider_span.empty())
        return InvalidPhysicsObjectHandle;

    for (const Collider& collider : collider_span)
    {
        if (collider.id != InvalidColliderId)
            return InvalidPhysicsObjectHandle;
    }

    if (_next_object_handle == 0
        || _next_object_handle == std::numeric_limits<std::uint64_t>::max())
    {
        return InvalidPhysicsObjectHandle;
    }

    const std::size_t collider_count = collider_span.size();
    if (collider_count > 0)
    {
        const ColliderId max_id = std::numeric_limits<ColliderId>::max();
        if (_next_collider_id == InvalidColliderId
            || collider_count - 1 > max_id - _next_collider_id)
        {
            return InvalidPhysicsObjectHandle;
        }
    }

    Registration registration;
    registration.handle = PhysicsObjectHandle{_next_object_handle};
    registration.owner = &owner;
    registration.body_provider = body_provider;
    registration.collider_provider = collider_provider;
    registration.body = body;
    registration.previous_owner_origin = owner.position();
    registration.current_owner_origin = owner.position();
    registration.colliders.reserve(collider_count);
    for (Collider& collider : collider_span)
        registration.colliders.push_back(&collider);

    _registrations.push_back(std::move(registration));
    Registration& committed = _registrations.back();
    std::vector<ColliderId> inserted_ids;
    inserted_ids.reserve(collider_count);

    try
    {
        ColliderId id = _next_collider_id;
        for (Collider* collider : committed.colliders)
        {
            collider->id = id;
            _collider_index.emplace(id, collider);
            inserted_ids.push_back(id);
            ++id;
        }
    }
    catch (...)
    {
        for (ColliderId id : inserted_ids)
            _collider_index.erase(id);
        for (Collider* collider : committed.colliders)
        {
            if (collider)
                collider->id = InvalidColliderId;
        }
        _registrations.pop_back();
        throw;
    }

    ++_next_object_handle;
    _next_collider_id += static_cast<ColliderId>(collider_count);
    return committed.handle;
}

bool PhysicsWorld::unregister_object(PhysicsObjectHandle handle) noexcept
{
    if (_advancing || !handle.is_valid())
        return false;

    const auto existing = std::find_if(
        _registrations.begin(),
        _registrations.end(),
        [handle](const Registration& registration)
        {
            return registration.handle == handle;
        });
    if (existing == _registrations.end())
        return false;

    for (Collider* collider : existing->colliders)
    {
        if (!collider)
            continue;
        _collider_index.erase(collider->id);
        collider->id = InvalidColliderId;
    }
    _registrations.erase(existing);
    return true;
}

bool PhysicsWorld::contains_object(PhysicsObjectHandle handle) const noexcept
{
    return find_registration(handle) != nullptr;
}

bool PhysicsWorld::contains_object(
    const elysia::core::GameObject& owner) const noexcept
{
    return std::ranges::any_of(
        _registrations,
        [&owner](const Registration& registration)
        {
            return registration.owner == &owner;
        });
}

bool PhysicsWorld::contains_collider(ColliderId collider) const noexcept
{
    return collider != InvalidColliderId
        && _collider_index.contains(collider);
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
    if (_advancing)
        return false;
    if (_tile_world)
        return _tile_world == &world;

    const elysia::core::Vector2 origin = world.world_origin();
    const elysia::core::Vector2 size = world.tile_size();
    if (!finite_vector(origin)
        || !finite_vector(size)
        || size.x <= elysia::core::Vector2::k_epsilon
        || size.y <= elysia::core::Vector2::k_epsilon
        || world.columns() < 0
        || world.rows() < 0)
    {
        return false;
    }

    _tile_world = &world;
    return true;
}

bool PhysicsWorld::clear_tile_world(const ITileCollisionWorld& world) noexcept
{
    if (_advancing || _tile_world != &world)
        return false;
    _tile_world = nullptr;
    return true;
}

const ITileCollisionWorld* PhysicsWorld::tile_world() const noexcept
{
    return _tile_world;
}

bool PhysicsWorld::add_listener(ICollisionListener& listener) noexcept
{
    if (_advancing)
        return false;
    if (std::ranges::find(_listeners, &listener) != _listeners.end())
        return true;
    try
    {
        _listeners.push_back(&listener);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool PhysicsWorld::remove_listener(
    const ICollisionListener& listener) noexcept
{
    if (_advancing)
        return false;
    const auto existing = std::ranges::find(_listeners, &listener);
    if (existing == _listeners.end())
        return false;
    _listeners.erase(existing);
    return true;
}

std::uint32_t PhysicsWorld::advance(double frame_delta_seconds)
{
    if (_advancing
        || !std::isfinite(frame_delta_seconds)
        || frame_delta_seconds <= 0.0)
    {
        return 0;
    }

    const double accumulated = _accumulator_seconds + frame_delta_seconds;
    if (!std::isfinite(accumulated))
        return 0;
    _accumulator_seconds = accumulated;
    _advancing = true;
    std::uint32_t steps = 0;

    try
    {
        while (_accumulator_seconds >= _config.fixed_delta_seconds
            && steps < _config.max_steps_per_advance)
        {
            fixed_step(_config.fixed_delta_seconds);
            _accumulator_seconds -= _config.fixed_delta_seconds;
            ++steps;
        }

        if (_accumulator_seconds >= _config.fixed_delta_seconds)
        {
            _accumulator_seconds = std::fmod(
                _accumulator_seconds,
                _config.fixed_delta_seconds);
        }
    }
    catch (...)
    {
        _advancing = false;
        throw;
    }

    _advancing = false;
    return steps;
}

void PhysicsWorld::reset() noexcept
{
    if (_advancing)
        return;

    for (Registration& registration : _registrations)
    {
        for (Collider* collider : registration.colliders)
        {
            if (collider)
                collider->id = InvalidColliderId;
        }
    }

    _registrations.clear();
    _collider_index.clear();
    _listeners.clear();
    _tile_world = nullptr;
    _collision_frame.clear();
    _accumulator_seconds = 0.0;
}

const PhysicsWorldConfig& PhysicsWorld::config() const noexcept
{
    return _config;
}

double PhysicsWorld::accumulator_seconds() const noexcept
{
    return _accumulator_seconds;
}

std::optional<CollisionQueryHit> PhysicsWorld::raycast(
    const RayCastQuery& query) const
{
    (void)query;
    return std::nullopt;
}

std::optional<CollisionQueryHit> PhysicsWorld::segment_cast(
    const SegmentCastQuery& query) const
{
    (void)query;
    return std::nullopt;
}

void PhysicsWorld::fixed_step(double fixed_delta_seconds)
{
    std::vector<PhysicsBodyView> body_views;
    std::vector<ColliderView> collider_views;
    body_views.reserve(_registrations.size());
    collider_views.reserve(_collider_index.size());

    for (Registration& registration : _registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed())
        {
            continue;
        }

        registration.previous_owner_origin = registration.current_owner_origin;
        registration.current_owner_origin = registration.owner->position();

        if (!registration.owner->is_active())
            continue;

        if (registration.body)
        {
            body_views.push_back(PhysicsBodyView{
                registration.handle,
                registration.body,
                registration.previous_owner_origin,
                registration.current_owner_origin
            });
        }
    }

    _physics_system.integrate(body_views, _config, fixed_delta_seconds);

    for (const PhysicsBodyView& view : body_views)
    {
        Registration* registration = find_registration(view.object);
        if (!registration || !registration->owner)
            continue;
        registration->current_owner_origin = view.current_owner_origin;
        registration->owner->set_position(view.current_owner_origin);
    }

    for (Registration& registration : _registrations)
    {
        if (!registration.owner
            || registration.owner->is_destroyed()
            || !registration.owner->is_active())
        {
            continue;
        }

        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled)
                continue;
            collider_views.push_back(ColliderView{
                registration.handle,
                collider,
                registration.previous_owner_origin,
                registration.current_owner_origin
            });
        }
    }

    _collision_system.evaluate(
        collider_views,
        _tile_world,
        fixed_delta_seconds,
        _collision_frame);

    for (const CollisionEvent& event : _collision_frame.events)
    {
        for (ICollisionListener* listener : _listeners)
        {
            if (listener)
                listener->on_collision_event(event);
        }
    }

    _physics_system.clear_forces(body_views);
}

PhysicsWorld::Registration* PhysicsWorld::find_registration(
    PhysicsObjectHandle handle) noexcept
{
    const auto found = std::find_if(
        _registrations.begin(),
        _registrations.end(),
        [handle](const Registration& registration)
        {
            return registration.handle == handle;
        });
    return found == _registrations.end() ? nullptr : &*found;
}

const PhysicsWorld::Registration* PhysicsWorld::find_registration(
    PhysicsObjectHandle handle) const noexcept
{
    const auto found = std::find_if(
        _registrations.begin(),
        _registrations.end(),
        [handle](const Registration& registration)
        {
            return registration.handle == handle;
        });
    return found == _registrations.end() ? nullptr : &*found;
}
}
