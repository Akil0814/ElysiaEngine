#include "physics_world.h"
#include "tile/tile_coordinate_range.h"
#include "tile/tile_geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace elysia::physics
{
namespace
{
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

[[nodiscard]] bool finite_rect(const elysia::core::Rect& rect) noexcept
{
    return finite_vector(rect.position())
        && finite_vector(rect.size());
}

template <typename Registrations, typename Visitor>
void visit_query_shapes(const Registrations& registrations, const CollisionFilter& filter, const Visitor& visitor)
{
    for (const auto& registration : registrations)
    {
        if (!registration.owner || registration.owner->is_destroyed() || !registration.owner->is_active())
            continue;
        for (const Collider* collider : registration.colliders)
        {
            if (!collider || !collider->enabled || collider->response == CollisionResponse::Ignore
                || !collision_filters_allow(filter, collider->filter))
                continue;
            if (const auto shape = make_world_shape(collider->shape, registration.current_owner_origin))
                visitor(CollisionTarget::from_collider(collider->id), collider->response, *shape);
        }
    }
}

template <typename Visitor>
void visit_query_tiles(const ITileCollisionWorld* world, const elysia::core::Rect& bounds,
    const CollisionFilter& filter, std::uint32_t limit, const Visitor& visitor)
{
    if (!world) return;
    const auto range = checked_tile_range(bounds, world->world_origin(), world->tile_size(),
        TileRangeBoundary::InclusiveTouching, limit);
    if (!range) return;
    for (std::int64_t y = range->min_y; y <= range->max_y; ++y)
        for (std::int64_t x = range->min_x; x <= range->max_x; ++x)
        {
            const TileCoordinate coordinate{static_cast<int>(x), static_cast<int>(y)};
            const auto cell = detail::tile_cell(*world, coordinate);
            const auto response = detail::tile_response(cell);
            if (response != CollisionResponse::Ignore && collision_filters_allow(filter, cell.filter))
                visitor(CollisionTarget::from_tile(coordinate), response,
                    WorldColliderShape{WorldAabb{detail::tile_rect(*world, coordinate)}});
        }
}

template <typename Registrations>
void overlap_shape(const Registrations& registrations, const ITileCollisionWorld* tiles,
    const PhysicsWorldConfig& config, const WorldColliderShape& query_shape,
    const CollisionFilter& filter, std::vector<CollisionOverlapQueryHit>& out_hits)
{
    const auto consider = [&](CollisionTarget target, CollisionResponse response, const WorldColliderShape& shape)
    {
        if (const auto hit = detect_discrete_shapes(query_shape, shape, config.collision_epsilon))
            out_hits.push_back({target, hit->manifold, response});
    };
    visit_query_shapes(registrations, filter, consider);
    visit_query_tiles(tiles, shape_bounds(query_shape), filter, config.max_tile_candidates_per_operation, consider);
    std::ranges::sort(out_hits, {}, &CollisionOverlapQueryHit::target);
}

}
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
    visit_query_shapes(_registrations, query.filter,
        [&](CollisionTarget target, CollisionResponse response, const WorldColliderShape& shape)
        {
            std::optional<RayShapeHit> hit;
            if (const auto* box = std::get_if<WorldAabb>(&shape))
                hit = ray_aabb(query.origin, direction, query.max_distance, box->rect, _config.collision_epsilon);
            else
                hit = ray_circle(query.origin, direction, query.max_distance,
                    std::get<WorldCircle>(shape), _config.collision_epsilon);
            if (hit) consider(target, response, *hit);
        });
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
            const auto cell = detail::tile_cell(*_tile_world, value);
            const auto response = detail::tile_response(cell);
            if (response != CollisionResponse::Ignore
                && collision_filters_allow(query.filter, cell.filter))
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
    std::set<CollisionTarget> seen;
    std::vector<CollisionQueryHit> unique;
    unique.reserve(out_hits.size());
    for (const CollisionQueryHit& hit : out_hits)
    {
        if (seen.insert(hit.target).second)
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
    const AabbOverlapQuery& query, std::vector<CollisionOverlapQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_rect(query.bounds) || query.bounds.is_empty()) return;
    overlap_shape(_registrations, _tile_world, _config, WorldAabb{query.bounds}, query.filter, out_hits);
}

void PhysicsWorld::overlap_circle(
    const CircleOverlapQuery& query, std::vector<CollisionOverlapQueryHit>& out_hits) const
{
    out_hits.clear();
    if (!finite_vector(query.center) || !std::isfinite(query.radius) || query.radius <= 0.0f) return;
    overlap_shape(_registrations, _tile_world, _config, WorldCircle{query.center, query.radius}, query.filter, out_hits);
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
                              const WorldAabb& target_box)
    {
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
    const auto visit = [&](CollisionTarget target, CollisionResponse response, const WorldColliderShape& shape)
    {
        if (const auto* box = std::get_if<WorldAabb>(&shape))
            consider(target, response, *box);
    };
    visit_query_shapes(_registrations, query.filter, visit);
    visit_query_tiles(_tile_world, swept_bounds, query.filter, _config.max_tile_candidates_per_operation, visit);
    return best;
}


}
