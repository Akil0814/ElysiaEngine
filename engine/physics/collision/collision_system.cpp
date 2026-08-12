#include "collision_system.h"

#include "default_collision_strategies.h"
#include "../tile/tile_collision_world.h"
#include "../tile/tile_coordinate_range.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace elysia::physics
{
namespace
{
[[nodiscard]] PhysicsObjectState* find_state(
    std::span<PhysicsObjectState> states,
    PhysicsObjectHandle object) noexcept
{
    if (!object.is_valid())
        return nullptr;
    const auto found = std::ranges::find(states, object, &PhysicsObjectState::object);
    return found == states.end() ? nullptr : &*found;
}

[[nodiscard]] const PhysicsObjectState* find_state(
    std::span<const PhysicsObjectState> states,
    PhysicsObjectHandle object) noexcept
{
    if (!object.is_valid())
        return nullptr;
    const auto found = std::ranges::find(states, object, &PhysicsObjectState::object);
    return found == states.end() ? nullptr : &*found;
}

[[nodiscard]] float inverse_mass(const PhysicsObjectState* state) noexcept
{
    if (!state || !state->body || !state->body->enabled
        || state->body->type != BodyType::Dynamic
        || !std::isfinite(state->body->mass)
        || state->body->mass <= 0.0f)
    {
        return 0.0f;
    }
    return 1.0f / state->body->mass;
}

[[nodiscard]] CollisionShapeView adjusted_view(
    const CollisionShapeView& view,
    std::span<const PhysicsObjectState> states) noexcept
{
    CollisionShapeView adjusted = view;
    const PhysicsObjectState* state = find_state(states, view.object);
    if (!state)
        return adjusted;
    const auto offset = state->current_owner_origin - view.current_owner_origin;
    adjusted.current_shape = translated_shape(view.current_shape, offset);
    adjusted.current_bounds = shape_bounds(adjusted.current_shape);
    adjusted.current_owner_origin = state->current_owner_origin;
    return adjusted;
}

[[nodiscard]] bool ignored_pair(
    CollisionPair pair,
    std::span<const CollisionPair> ignored) noexcept
{
    return std::ranges::find(ignored, pair) != ignored.end();
}

[[nodiscard]] bool better_contact(
    const CollisionContact& candidate,
    const CollisionContact& current,
    float epsilon) noexcept
{
    if (candidate.time_of_impact + epsilon < current.time_of_impact)
        return true;
    if (current.time_of_impact + epsilon < candidate.time_of_impact)
        return false;
    if (candidate.response != current.response)
        return candidate.response == CollisionResponse::Block;
    return candidate.manifold.penetration > current.manifold.penetration + epsilon;
}

void sort_and_deduplicate_contacts(
    std::vector<CollisionContact>& contacts,
    float epsilon)
{
    std::ranges::stable_sort(contacts, {}, &CollisionContact::pair);
    std::vector<CollisionContact> unique;
    unique.reserve(contacts.size());
    for (const CollisionContact& contact : contacts)
    {
        if (unique.empty() || unique.back().pair != contact.pair)
        {
            unique.push_back(contact);
            continue;
        }
        if (better_contact(contact, unique.back(), epsilon))
            unique.back() = contact;
    }
    contacts = std::move(unique);
}

[[nodiscard]] TileCollisionCell resolved_cell(
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

[[nodiscard]] CollisionShapeView make_tile_view(
    const ITileCollisionWorld& world,
    TileCoordinate coordinate,
    const TileCollisionCell& cell) noexcept
{
    const auto origin = world.world_origin();
    const auto size = world.tile_size();
    const elysia::core::Rect rect{
        origin.x + static_cast<float>(coordinate.x) * size.x,
        origin.y + static_cast<float>(coordinate.y) * size.y,
        size.x,
        size.y
    };
    const WorldAabb shape{rect};
    CollisionShapeView view;
    view.target = CollisionTarget::from_tile(coordinate);
    view.previous_shape = shape;
    view.current_shape = shape;
    view.current_bounds = rect;
    view.swept_bounds = rect;
    view.filter = cell.filter;
    view.response = cell.type == TileCollisionType::Overlap
        ? CollisionResponse::Overlap
        : CollisionResponse::Block;
    view.one_way = cell.type == TileCollisionType::OneWay
        ? cell.one_way
        : std::nullopt;
    view.material = cell.material;
    return view;
}

[[nodiscard]] bool matching_internal_tile_surface(
    const TileCollisionCell& first,
    const TileCollisionCell& second) noexcept
{
    if (first.type == TileCollisionType::Block
        && second.type == TileCollisionType::Block)
    {
        return true;
    }
    return first.type == TileCollisionType::OneWay
        && second.type == TileCollisionType::OneWay
        && first.one_way.has_value()
        && second.one_way.has_value()
        && first.one_way->pass_through == second.one_way->pass_through;
}

[[nodiscard]] bool supports_continuous(
    const CollisionShapeView& first,
    const CollisionShapeView& second) noexcept
{
    return (first.detection_mode == CollisionDetectionMode::Continuous
            || second.detection_mode == CollisionDetectionMode::Continuous)
        && std::holds_alternative<WorldAabb>(first.previous_shape)
        && std::holds_alternative<WorldAabb>(first.current_shape)
        && std::holds_alternative<WorldAabb>(second.previous_shape)
        && std::holds_alternative<WorldAabb>(second.current_shape);
}

struct VelocityImpulseResult
{
    float normal = 0.0f;
    float tangent = 0.0f;
};

[[nodiscard]] elysia::core::Vector2 body_velocity(
    const PhysicsObjectState* state) noexcept
{
    if (!state || !state->body || !state->body->enabled)
        return {};
    const PhysicsBody& body = *state->body;
    if (!finite_vector(body.velocity) || body.type == BodyType::Static)
        return {};
    if (body.type == BodyType::Dynamic
        && (!std::isfinite(body.mass) || body.mass <= 0.0f))
    {
        return {};
    }
    return body.velocity;
}

void apply_impulse(
    PhysicsObjectState* state,
    const elysia::core::Vector2& impulse,
    float inverse_mass_value) noexcept
{
    if (state && state->body && inverse_mass_value > 0.0f)
        state->body->velocity += impulse * inverse_mass_value;
}

[[nodiscard]] VelocityImpulseResult solve_velocity_contact(
    PhysicsObjectState* first,
    PhysicsObjectState* second,
    const elysia::core::Vector2& normal,
    PhysicsMaterial material,
    float restitution_velocity_threshold,
    float epsilon) noexcept
{
    VelocityImpulseResult result;
    const float first_inverse_mass = inverse_mass(first);
    const float second_inverse_mass = inverse_mass(second);
    const float inverse_mass_sum = first_inverse_mass + second_inverse_mass;
    if (inverse_mass_sum <= 0.0f || normal.is_zero(epsilon))
        return result;

    material = normalized_physics_material(material);
    const float closing_speed = (body_velocity(second) - body_velocity(first)).dot(normal);
    if (closing_speed >= -epsilon)
        return result;

    const float restitution = -closing_speed >= restitution_velocity_threshold
        ? material.restitution
        : 0.0f;
    result.normal = -(1.0f + restitution) * closing_speed / inverse_mass_sum;
    const auto normal_impulse = normal * result.normal;
    apply_impulse(first, -normal_impulse, first_inverse_mass);
    apply_impulse(second, normal_impulse, second_inverse_mass);

    const elysia::core::Vector2 tangent{-normal.y, normal.x};
    const float tangent_speed = (body_velocity(second) - body_velocity(first)).dot(tangent);
    if (std::fabs(tangent_speed) <= epsilon)
        return result;

    const float desired_tangent_impulse = -tangent_speed / inverse_mass_sum;
    const float static_limit = material.static_friction * result.normal;
    if (std::fabs(desired_tangent_impulse) <= static_limit + epsilon)
    {
        result.tangent = desired_tangent_impulse;
    }
    else
    {
        result.tangent = -std::copysign(
            material.dynamic_friction * result.normal,
            tangent_speed);
    }
    const auto tangent_impulse = tangent * result.tangent;
    apply_impulse(first, -tangent_impulse, first_inverse_mass);
    apply_impulse(second, tangent_impulse, second_inverse_mass);
    return result;
}
}

CollisionSystem::CollisionSystem()
    : CollisionSystem(make_default_collision_strategies())
{
}

CollisionSystem::CollisionSystem(CollisionStrategySet strategies)
    : _strategies(std::move(strategies))
{
    if (!_strategies.is_complete())
        throw std::invalid_argument("CollisionSystem requires a complete strategy set.");
}

const IBroadPhaseIndex& CollisionSystem::broad_phase_index() const noexcept
{
    return *_strategies.broad_phase;
}

const ICollisionDetectionStrategy& CollisionSystem::discrete_detection_strategy() const noexcept
{
    return *_strategies.discrete_detection;
}

const ICollisionDetectionStrategy& CollisionSystem::continuous_detection_strategy() const noexcept
{
    return *_strategies.continuous_detection;
}

const ICollisionResponseStrategy& CollisionSystem::response_strategy() const noexcept
{
    return *_strategies.response;
}

void CollisionSystem::evaluate(
    std::span<PhysicsObjectState> object_states,
    std::span<const CollisionShapeView> collider_views,
    const ITileCollisionWorld* tile_world,
    std::span<const CollisionPair> transient_ignored_pairs,
    const PhysicsWorldConfig& config,
    double fixed_delta_seconds,
    CollisionFrame& out_frame,
    PhysicsStepStats& stats,
    PhysicsDebugCapture debug_capture,
    PhysicsDebugSnapshot* debug_snapshot)
{
    out_frame.clear();
    if (debug_snapshot)
        debug_snapshot->clear();
    const bool capture_broad_phase = debug_snapshot
        && captures_physics_debug(debug_capture, PhysicsDebugCapture::BroadPhase);
    const bool capture_shapes = debug_snapshot
        && (captures_physics_debug(debug_capture, PhysicsDebugCapture::Shapes)
            || capture_broad_phase);
    const bool capture_contacts = debug_snapshot
        && captures_physics_debug(debug_capture, PhysicsDebugCapture::Contacts);
    const CollisionDetectionContext detection_context{
        fixed_delta_seconds, config.collision_epsilon};

    std::vector<BroadPhaseProxy> proxies;
    proxies.reserve(collider_views.size());
    std::unordered_map<ColliderId, std::size_t> collider_lookup;
    collider_lookup.reserve(collider_views.size());
    std::vector<CollisionShapeView> all_views(collider_views.begin(), collider_views.end());

    for (std::size_t i = 0; i < collider_views.size(); ++i)
    {
        const auto& view = collider_views[i];
        if (view.target.kind != CollisionTargetKind::Collider)
            continue;
        proxies.push_back(BroadPhaseProxy{
            view.target.collider,
            view.current_bounds,
            view.swept_bounds,
            view.filter,
            true
        });
        collider_lookup.emplace(view.target.collider, i);
        if (capture_shapes)
        {
            debug_snapshot->shapes.push_back(
                PhysicsDebugShape{view.target, view.previous_shape,
                    view.current_shape, view.swept_bounds});
        }
    }
    stats.broad_phase_proxies = proxies.size();
    _strategies.broad_phase->synchronize(proxies);

    std::vector<BroadPhasePair> broad_pairs;
    _strategies.broad_phase->collect_pairs(broad_pairs);
    stats.broad_phase_pairs = broad_pairs.size();
    if (capture_broad_phase)
        debug_snapshot->broad_phase_pairs = broad_pairs;

    const auto is_internal_tile_face = [&](
        const CollisionShapeView& first,
        const CollisionShapeView& second,
        const CollisionHit& hit,
        const CollisionResponseContext& context,
        CollisionResponse response)
    {
        if (response != CollisionResponse::Block || !tile_world
            || first.target.kind != CollisionTargetKind::Collider
            || second.target.kind != CollisionTargetKind::Tile)
            return false;

        const auto normal = hit.manifold.normal;
        TileCoordinate neighbour = second.target.tile;
        bool axis_aligned = false;
        if (std::fabs(normal.x) >= 1.0f - config.collision_epsilon
            && std::fabs(normal.y) <= config.collision_epsilon)
        {
            neighbour.x -= normal.x > 0.0f ? 1 : -1;
            axis_aligned = true;
        }
        else if (std::fabs(normal.y) >= 1.0f - config.collision_epsilon
            && std::fabs(normal.x) <= config.collision_epsilon)
        {
            neighbour.y -= normal.y > 0.0f ? 1 : -1;
            axis_aligned = true;
        }
        if (!axis_aligned)
            return false;

        const TileCollisionCell current_cell = resolved_cell(
            *tile_world, second.target.tile);
        const TileCollisionCell neighbour_cell = resolved_cell(
            *tile_world, neighbour);
        if (!matching_internal_tile_surface(current_cell, neighbour_cell)
            || !collision_filters_allow(first.filter, neighbour_cell.filter))
            return false;

        const CollisionShapeView neighbour_view = make_tile_view(
            *tile_world, neighbour, neighbour_cell);
        const auto neighbour_hit = detect_discrete_shapes(
            first.current_shape,
            neighbour_view.current_shape,
            config.collision_epsilon);
        return neighbour_hit
            && _strategies.response->classify(
                first, neighbour_view, *neighbour_hit, context)
                == CollisionResponse::Block;
    };

    const auto detect_candidate = [&](
        const CollisionShapeView& first,
        const CollisionShapeView& second)
    {
        if (!first.target.is_valid() || !second.target.is_valid()
            || (first.object.is_valid() && first.object == second.object)
            || !collision_filters_allow(first.filter, second.filter))
        {
            return;
        }

        ++stats.narrow_phase_tests;
        const bool continuous = supports_continuous(first, second);
        std::optional<CollisionHit> hit = continuous
            ? _strategies.continuous_detection->detect(first, second, detection_context)
            : _strategies.discrete_detection->detect(first, second, detection_context);
        if (!hit)
            return;
        if (continuous && hit->time_of_impact < 1.0f)
            ++stats.ccd_hits;

        const CollisionPair pair = normalized_collision_pair(first.target, second.target);
        const bool pair_ignored = ignored_pair(pair, transient_ignored_pairs);
        CollisionResponseContext context;
        context.first_displacement = first.current_owner_origin - first.previous_owner_origin;
        context.second_displacement = second.current_owner_origin - second.previous_owner_origin;
        context.epsilon = config.collision_epsilon;
        context.transiently_ignored = pair_ignored;
        const CollisionResponse response = _strategies.response->classify(
            first, second, *hit, context);
        if (response == CollisionResponse::Ignore)
        {
            if (pair_ignored)
                out_frame.ignored_pairs_overlapping.push_back(pair);
            return;
        }

        // A tile face covered by an equivalent solid neighbour is not part of
        // the map boundary. Dropping it before caching/solving prevents a body
        // from catching on seams while preserving each exposed support tile.
        if (is_internal_tile_face(first, second, *hit, context, response))
            return;

        CollisionContact contact;
        contact.pair = pair;
        contact.manifold = hit->manifold;
        contact.response = response;
        contact.time_of_impact = hit->time_of_impact;
        if (pair.first != first.target)
            contact.manifold.normal = -contact.manifold.normal;
        out_frame.contacts.push_back(contact);
    };

    for (const BroadPhasePair& pair : broad_pairs)
    {
        const auto first = collider_lookup.find(pair.first);
        const auto second = collider_lookup.find(pair.second);
        if (first == collider_lookup.end() || second == collider_lookup.end())
            continue;
        detect_candidate(all_views[first->second], all_views[second->second]);
    }

    if (tile_world)
    {
        const auto origin = tile_world->world_origin();
        const auto tile_size = tile_world->tile_size();
        for (const CollisionShapeView& collider : collider_views)
        {
            const auto range = checked_tile_range(
                collider.swept_bounds, origin, tile_size,
                TileRangeBoundary::HalfOpen,
                config.max_tile_candidates_per_operation);
            if (!range)
            {
                ++stats.rejected_tile_candidate_ranges;
                continue;
            }
            for (std::int64_t y = range->min_y; y <= range->max_y; ++y)
            {
                for (std::int64_t x = range->min_x; x <= range->max_x; ++x)
                {
                    ++stats.tile_samples;
                    const TileCoordinate coordinate{
                        static_cast<int>(x), static_cast<int>(y)};
                    const TileCollisionCell cell = resolved_cell(*tile_world, coordinate);
                    if (cell.type == TileCollisionType::Empty)
                        continue;
                    if (capture_broad_phase)
                        debug_snapshot->tile_candidates.push_back(coordinate);
                    CollisionShapeView tile = make_tile_view(*tile_world, coordinate, cell);
                    detect_candidate(collider, tile);
                    all_views.push_back(std::move(tile));
                }
            }
        }
    }

    sort_and_deduplicate_contacts(out_frame.contacts, config.collision_epsilon);
    std::ranges::sort(out_frame.ignored_pairs_overlapping);
    out_frame.ignored_pairs_overlapping.erase(
        std::unique(
            out_frame.ignored_pairs_overlapping.begin(),
            out_frame.ignored_pairs_overlapping.end()),
        out_frame.ignored_pairs_overlapping.end());

    const auto find_view = [&](CollisionTarget target) -> const CollisionShapeView*
    {
        const auto found = std::ranges::find(all_views, target, &CollisionShapeView::target);
        return found == all_views.end() ? nullptr : &*found;
    };

    std::vector<PhysicsObjectHandle> ccd_resolved_objects;
    struct CcdProgress
    {
        PhysicsObjectState* state = nullptr;
        elysia::core::Vector2 sweep_start{};
        float remaining_seconds = 0.0f;
        std::uint32_t iterations = 0;
        std::vector<CollisionPair> hit_pairs;
    };
    std::vector<CcdProgress> ccd_progress;
    std::vector<CollisionContact*> ccd_contacts;
    for (CollisionContact& contact : out_frame.contacts)
        if (contact.response == CollisionResponse::Block
            && contact.time_of_impact < 1.0f - config.collision_epsilon)
            ccd_contacts.push_back(&contact);
    std::ranges::stable_sort(ccd_contacts, [](const auto* first, const auto* second)
    {
        if (first->time_of_impact != second->time_of_impact)
            return first->time_of_impact < second->time_of_impact;
        return first->pair < second->pair;
    });
    for (CollisionContact* contact_pointer : ccd_contacts)
    {
        CollisionContact& contact = *contact_pointer;
        const auto* first_view = find_view(contact.pair.first);
        const auto* second_view = find_view(contact.pair.second);
        if (!first_view || !second_view)
            continue;
        PhysicsObjectState* first_state = find_state(object_states, first_view->object);
        PhysicsObjectState* second_state = find_state(object_states, second_view->object);
        const float first_inverse_mass = inverse_mass(first_state);
        const float second_inverse_mass = inverse_mass(second_state);
        if (first_inverse_mass <= 0.0f && second_inverse_mass <= 0.0f)
            continue;
        const bool first_already_resolved = first_state
            && std::ranges::find(ccd_resolved_objects, first_state->object)
                != ccd_resolved_objects.end();
        const bool second_already_resolved = second_state
            && std::ranges::find(ccd_resolved_objects, second_state->object)
                != ccd_resolved_objects.end();
        if ((first_inverse_mass > 0.0f && first_already_resolved)
            || (second_inverse_mass > 0.0f && second_already_resolved))
            continue;
        const float toi = std::clamp(contact.time_of_impact, 0.0f, 1.0f);
        elysia::core::Vector2 first_impact{};
        elysia::core::Vector2 second_impact{};
        if (first_inverse_mass > 0.0f)
        {
            const auto movement = first_state->current_owner_origin - first_state->previous_owner_origin;
            first_state->current_owner_origin = first_state->previous_owner_origin + movement * toi;
            first_impact = first_state->current_owner_origin;
        }
        if (second_inverse_mass > 0.0f)
        {
            const auto movement = second_state->current_owner_origin - second_state->previous_owner_origin;
            second_state->current_owner_origin = second_state->previous_owner_origin + movement * toi;
            second_impact = second_state->current_owner_origin;
        }
        const auto impulse = solve_velocity_contact(
            first_state,
            second_state,
            contact.manifold.normal,
            combine_physics_materials(first_view->material, second_view->material),
            config.restitution_velocity_threshold,
            config.collision_epsilon);
        contact.normal_impulse += impulse.normal;
        contact.tangent_impulse += impulse.tangent;
        const float remaining = static_cast<float>(fixed_delta_seconds) * (1.0f - toi);
        if (first_inverse_mass > 0.0f && first_state->body)
            first_state->current_owner_origin += first_state->body->velocity * remaining;
        if (second_inverse_mass > 0.0f && second_state->body)
            second_state->current_owner_origin += second_state->body->velocity * remaining;
        if (first_inverse_mass > 0.0f)
        {
            ccd_resolved_objects.push_back(first_state->object);
            ccd_progress.push_back({first_state, first_impact, remaining, 1, {contact.pair}});
        }
        if (second_inverse_mass > 0.0f)
        {
            ccd_resolved_objects.push_back(second_state->object);
            ccd_progress.push_back({second_state, second_impact, remaining, 1, {contact.pair}});
        }
        ++stats.ccd_iterations;
    }

    for (CcdProgress& progress : ccd_progress)
    {
        while (progress.state && progress.state->body
            && progress.iterations < config.max_ccd_iterations
            && progress.remaining_seconds > config.collision_epsilon)
        {
            struct IterationHit
            {
                CollisionShapeView moving;
                CollisionShapeView target;
                CollisionHit hit;
                CollisionResponse response = CollisionResponse::Ignore;
                CollisionPair pair{};
            };
            std::optional<IterationHit> best;
            const auto desired_origin = progress.state->current_owner_origin;
            for (const CollisionShapeView& source : collider_views)
            {
                if (source.object != progress.state->object
                    || !std::holds_alternative<WorldAabb>(source.previous_shape))
                    continue;
                CollisionShapeView moving = source;
                moving.previous_shape = translated_shape(
                    source.previous_shape,
                    progress.sweep_start - source.previous_owner_origin);
                moving.current_shape = translated_shape(
                    source.previous_shape,
                    desired_origin - source.previous_owner_origin);
                moving.previous_owner_origin = progress.sweep_start;
                moving.current_owner_origin = desired_origin;
                moving.current_bounds = shape_bounds(moving.current_shape);
                moving.swept_bounds = swept_shape_bounds(
                    moving.previous_shape, moving.current_shape);

                for (const CollisionShapeView& target_source : all_views)
                {
                    if (target_source.object == progress.state->object
                        || !std::holds_alternative<WorldAabb>(target_source.current_shape)
                        || !collision_filters_allow(moving.filter, target_source.filter))
                        continue;
                    PhysicsObjectState* target_state = find_state(
                        object_states, target_source.object);
                    if (inverse_mass(target_state) > 0.0f)
                        continue;
                    CollisionShapeView target = adjusted_view(
                        target_source, object_states);
                    target.previous_shape = target.current_shape;
                    target.previous_owner_origin = target.current_owner_origin;
                    const CollisionPair pair = normalized_collision_pair(
                        moving.target, target.target);
                    if (std::ranges::find(progress.hit_pairs, pair)
                        != progress.hit_pairs.end())
                        continue;
                    const auto hit = _strategies.continuous_detection->detect(
                        moving, target,
                        CollisionDetectionContext{
                            progress.remaining_seconds,
                            config.collision_epsilon});
                    if (!hit || hit->time_of_impact >= 1.0f - config.collision_epsilon)
                        continue;
                    CollisionResponseContext context;
                    context.first_displacement = desired_origin - progress.sweep_start;
                    context.second_displacement = {};
                    context.epsilon = config.collision_epsilon;
                    context.transiently_ignored = ignored_pair(
                        pair, transient_ignored_pairs);
                    const CollisionResponse response = _strategies.response->classify(
                        moving, target, *hit, context);
                    if (response != CollisionResponse::Block
                        || is_internal_tile_face(
                            moving, target, *hit, context, response))
                        continue;
                    if (!best
                        || hit->time_of_impact + config.collision_epsilon
                            < best->hit.time_of_impact
                        || (std::fabs(hit->time_of_impact - best->hit.time_of_impact)
                                <= config.collision_epsilon
                            && pair < best->pair))
                    {
                        best = IterationHit{
                            moving, target, *hit, response, pair};
                    }
                }
            }
            if (!best)
                break;

            const float toi = std::clamp(best->hit.time_of_impact, 0.0f, 1.0f);
            const float full_step_seconds = static_cast<float>(fixed_delta_seconds);
            const float remaining_fraction = full_step_seconds > 0.0f
                ? progress.remaining_seconds / full_step_seconds : 0.0f;
            const float global_toi = std::clamp(
                1.0f - remaining_fraction + remaining_fraction * toi,
                0.0f,
                1.0f);
            const auto movement = desired_origin - progress.sweep_start;
            const auto impact_origin = progress.sweep_start + movement * toi;
            progress.state->current_owner_origin = impact_origin;
            PhysicsObjectState* target_state = find_state(
                object_states, best->target.object);
            const auto impulse = solve_velocity_contact(
                progress.state,
                target_state,
                best->hit.manifold.normal,
                combine_physics_materials(
                    best->moving.material, best->target.material),
                config.restitution_velocity_threshold,
                config.collision_epsilon);
            progress.remaining_seconds *= 1.0f - toi;
            progress.sweep_start = impact_origin;
            progress.state->current_owner_origin = impact_origin
                + progress.state->body->velocity * progress.remaining_seconds;
            ++progress.iterations;
            ++stats.ccd_iterations;
            ++stats.ccd_hits;
            progress.hit_pairs.push_back(best->pair);

            CollisionContact contact;
            contact.pair = best->pair;
            contact.manifold = best->hit.manifold;
            contact.response = best->response;
            contact.time_of_impact = global_toi;
            contact.normal_impulse = impulse.normal;
            contact.tangent_impulse = impulse.tangent;
            if (contact.pair.first != best->moving.target)
                contact.manifold.normal = -contact.manifold.normal;
            out_frame.contacts.push_back(contact);
        }
    }

    sort_and_deduplicate_contacts(out_frame.contacts, config.collision_epsilon);

    for (std::uint32_t iteration = 0; iteration < config.solver_iterations; ++iteration)
    {
        ++stats.solver_iterations;
        for (CollisionContact& contact : out_frame.contacts)
        {
            if (contact.response != CollisionResponse::Block)
                continue;
            const auto* first_source = find_view(contact.pair.first);
            const auto* second_source = find_view(contact.pair.second);
            if (!first_source || !second_source)
                continue;
            const CollisionShapeView first = adjusted_view(*first_source, object_states);
            const CollisionShapeView second = adjusted_view(*second_source, object_states);
            const auto hit = detect_discrete_shapes(
                first.current_shape,
                second.current_shape,
                config.collision_epsilon);
            if (!hit)
                continue;
            contact.manifold = hit->manifold;
            PhysicsObjectState* first_state = find_state(object_states, first.object);
            PhysicsObjectState* second_state = find_state(object_states, second.object);
            const float first_inverse_mass = inverse_mass(first_state);
            const float second_inverse_mass = inverse_mass(second_state);
            const float inverse_mass_sum = first_inverse_mass + second_inverse_mass;
            if (inverse_mass_sum <= 0.0f)
                continue;
            const float depth = std::max(
                0.0f,
                hit->manifold.penetration - config.penetration_slop);
            const auto correction = hit->manifold.normal
                * (depth * config.position_correction_percent);
            if (first_inverse_mass > 0.0f)
                first_state->current_owner_origin -= correction * (first_inverse_mass / inverse_mass_sum);
            if (second_inverse_mass > 0.0f)
                second_state->current_owner_origin += correction * (second_inverse_mass / inverse_mass_sum);
            const auto impulse = solve_velocity_contact(
                first_state,
                second_state,
                hit->manifold.normal,
                combine_physics_materials(first.material, second.material),
                config.restitution_velocity_threshold,
                config.collision_epsilon);
            contact.normal_impulse += impulse.normal;
            contact.tangent_impulse += impulse.tangent;
        }
    }

    stats.contacts = out_frame.contacts.size();
    if (capture_contacts)
        debug_snapshot->contacts = out_frame.contacts;
    if (capture_broad_phase)
    {
        std::ranges::sort(debug_snapshot->tile_candidates);
        debug_snapshot->tile_candidates.erase(
            std::unique(debug_snapshot->tile_candidates.begin(),
                debug_snapshot->tile_candidates.end()),
            debug_snapshot->tile_candidates.end());
    }
}

void CollisionSystem::query_aabb(
    const elysia::core::Rect& bounds,
    std::vector<ColliderId>& out_candidates) const
{
    _strategies.broad_phase->query_aabb(bounds, out_candidates);
}

void CollisionSystem::clear() noexcept
{
    _strategies.broad_phase->clear();
}
}
