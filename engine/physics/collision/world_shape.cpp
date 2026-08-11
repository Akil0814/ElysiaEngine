#include "world_shape.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <limits>

namespace elysia::physics
{
namespace
{
[[nodiscard]] float clamp_non_negative(float value) noexcept
{
    return std::max(0.0f, value);
}

[[nodiscard]] std::optional<CollisionHit> detect_aabb_aabb(
    const WorldAabb& first,
    const WorldAabb& second) noexcept
{
    const auto& a = first.rect;
    const auto& b = second.rect;
    const float overlap_x = std::min(a.right(), b.right()) - std::max(a.left(), b.left());
    const float overlap_y = std::min(a.bottom(), b.bottom()) - std::max(a.top(), b.top());
    if (overlap_x < 0.0f || overlap_y < 0.0f)
        return std::nullopt;

    CollisionHit hit;
    if (overlap_x <= overlap_y)
    {
        const float direction = b.center().x >= a.center().x ? 1.0f : -1.0f;
        hit.manifold.normal = {direction, 0.0f};
        hit.manifold.penetration = clamp_non_negative(overlap_x);
        const float contact_x = direction > 0.0f ? a.right() : a.left();
        const float contact_y = (std::max(a.top(), b.top()) + std::min(a.bottom(), b.bottom())) * 0.5f;
        hit.manifold.contact_points[0] = {contact_x, contact_y};
    }
    else
    {
        const float direction = b.center().y >= a.center().y ? 1.0f : -1.0f;
        hit.manifold.normal = {0.0f, direction};
        hit.manifold.penetration = clamp_non_negative(overlap_y);
        const float contact_x = (std::max(a.left(), b.left()) + std::min(a.right(), b.right())) * 0.5f;
        const float contact_y = direction > 0.0f ? a.bottom() : a.top();
        hit.manifold.contact_points[0] = {contact_x, contact_y};
    }
    hit.manifold.contact_point_count = 1;
    return hit;
}

[[nodiscard]] std::optional<CollisionHit> detect_circle_circle(
    const WorldCircle& first,
    const WorldCircle& second) noexcept
{
    const elysia::core::Vector2 delta = second.center - first.center;
    const float radius_sum = first.radius + second.radius;
    const float distance_squared = delta.length_squared();
    if (distance_squared > radius_sum * radius_sum)
        return std::nullopt;

    const float distance = std::sqrt(std::max(0.0f, distance_squared));
    const elysia::core::Vector2 normal = distance > elysia::core::Vector2::k_epsilon
        ? delta / distance
        : elysia::core::Vector2{1.0f, 0.0f};
    CollisionHit hit;
    hit.manifold.normal = normal;
    hit.manifold.penetration = clamp_non_negative(radius_sum - distance);
    const elysia::core::Vector2 first_surface = first.center + normal * first.radius;
    const elysia::core::Vector2 second_surface = second.center - normal * second.radius;
    hit.manifold.contact_points[0] = (first_surface + second_surface) * 0.5f;
    hit.manifold.contact_point_count = 1;
    return hit;
}

[[nodiscard]] std::optional<CollisionHit> detect_aabb_circle(
    const WorldAabb& first,
    const WorldCircle& second) noexcept
{
    const auto& rect = first.rect;
    const elysia::core::Vector2 closest{
        std::clamp(second.center.x, rect.left(), rect.right()),
        std::clamp(second.center.y, rect.top(), rect.bottom())
    };
    const elysia::core::Vector2 delta = second.center - closest;
    const float distance_squared = delta.length_squared();
    if (distance_squared > second.radius * second.radius)
        return std::nullopt;

    CollisionHit hit;
    if (distance_squared > elysia::core::Vector2::k_epsilon * elysia::core::Vector2::k_epsilon)
    {
        const float distance = std::sqrt(distance_squared);
        hit.manifold.normal = delta / distance;
        hit.manifold.penetration = clamp_non_negative(second.radius - distance);
        hit.manifold.contact_points[0] = closest;
    }
    else
    {
        const float left = second.center.x - rect.left();
        const float right = rect.right() - second.center.x;
        const float top = second.center.y - rect.top();
        const float bottom = rect.bottom() - second.center.y;
        float minimum = left;
        hit.manifold.normal = {-1.0f, 0.0f};
        hit.manifold.contact_points[0] = {rect.left(), second.center.y};
        if (right < minimum)
        {
            minimum = right;
            hit.manifold.normal = {1.0f, 0.0f};
            hit.manifold.contact_points[0] = {rect.right(), second.center.y};
        }
        if (top < minimum)
        {
            minimum = top;
            hit.manifold.normal = {0.0f, -1.0f};
            hit.manifold.contact_points[0] = {second.center.x, rect.top()};
        }
        if (bottom < minimum)
        {
            minimum = bottom;
            hit.manifold.normal = {0.0f, 1.0f};
            hit.manifold.contact_points[0] = {second.center.x, rect.bottom()};
        }
        hit.manifold.penetration = clamp_non_negative(second.radius + minimum);
    }
    hit.manifold.contact_point_count = 1;
    return hit;
}

[[nodiscard]] CollisionHit flipped_hit(CollisionHit hit) noexcept
{
    hit.manifold.normal = -hit.manifold.normal;
    return hit;
}
}

bool finite_vector(const elysia::core::Vector2& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool valid_shape(const ColliderShape& shape) noexcept
{
    return std::visit([](const auto& value) noexcept
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AabbShape>)
        {
            return finite_vector(value.local_rect.position())
                && finite_vector(value.local_rect.size())
                && !value.local_rect.is_empty();
        }
        else
        {
            return finite_vector(value.local_center)
                && std::isfinite(value.radius)
                && value.radius > elysia::core::Vector2::k_epsilon;
        }
    }, shape);
}

std::optional<WorldColliderShape> make_world_shape(
    const ColliderShape& shape,
    const elysia::core::Vector2& owner_origin) noexcept
{
    if (!finite_vector(owner_origin) || !valid_shape(shape))
        return std::nullopt;
    return std::visit([owner_origin](const auto& value) -> WorldColliderShape
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AabbShape>)
            return WorldAabb{value.local_rect.translated(owner_origin)};
        else
            return WorldCircle{owner_origin + value.local_center, value.radius};
    }, shape);
}

elysia::core::Rect shape_bounds(const WorldColliderShape& shape) noexcept
{
    return std::visit([](const auto& value)
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, WorldAabb>)
            return value.rect;
        else
            return elysia::core::Rect::from_center(
                value.center,
                {value.radius * 2.0f, value.radius * 2.0f});
    }, shape);
}

elysia::core::Rect swept_shape_bounds(
    const WorldColliderShape& previous,
    const WorldColliderShape& current) noexcept
{
    return shape_bounds(previous).merged(shape_bounds(current));
}

WorldColliderShape translated_shape(
    const WorldColliderShape& shape,
    const elysia::core::Vector2& offset) noexcept
{
    return std::visit([offset](const auto& value) -> WorldColliderShape
    {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, WorldAabb>)
            return WorldAabb{value.rect.translated(offset)};
        else
            return WorldCircle{value.center + offset, value.radius};
    }, shape);
}

bool collision_filters_allow(
    const CollisionFilter& first,
    const CollisionFilter& second) noexcept
{
    if (first.group != 0 && first.group == second.group)
        return first.group > 0;
    return (first.mask & second.category) != 0
        && (second.mask & first.category) != 0;
}

CollisionResponse combine_collision_responses(
    CollisionResponse first,
    CollisionResponse second) noexcept
{
    if (first == CollisionResponse::Ignore || second == CollisionResponse::Ignore)
        return CollisionResponse::Ignore;
    if (first == CollisionResponse::Overlap || second == CollisionResponse::Overlap)
        return CollisionResponse::Overlap;
    return CollisionResponse::Block;
}

std::optional<CollisionHit> detect_discrete_shapes(
    const WorldColliderShape& first,
    const WorldColliderShape& second) noexcept
{
    if (const auto* first_aabb = std::get_if<WorldAabb>(&first))
    {
        if (const auto* second_aabb = std::get_if<WorldAabb>(&second))
            return detect_aabb_aabb(*first_aabb, *second_aabb);
        return detect_aabb_circle(*first_aabb, std::get<WorldCircle>(second));
    }
    if (const auto* second_circle = std::get_if<WorldCircle>(&second))
        return detect_circle_circle(std::get<WorldCircle>(first), *second_circle);
    const auto hit = detect_aabb_circle(
        std::get<WorldAabb>(second),
        std::get<WorldCircle>(first));
    return hit ? std::optional<CollisionHit>(flipped_hit(*hit)) : std::nullopt;
}

std::optional<CollisionHit> detect_swept_aabbs(
    const WorldAabb& first_previous,
    const WorldAabb& first_current,
    const WorldAabb& second_previous,
    const WorldAabb& second_current,
    float epsilon) noexcept
{
    if (const auto overlap = detect_aabb_aabb(first_previous, second_previous))
    {
        CollisionHit initial = *overlap;
        initial.time_of_impact = 0.0f;
        return initial;
    }

    const elysia::core::Vector2 first_move =
        first_current.rect.position() - first_previous.rect.position();
    const elysia::core::Vector2 second_move =
        second_current.rect.position() - second_previous.rect.position();
    const elysia::core::Vector2 relative = first_move - second_move;

    float entry_x = -std::numeric_limits<float>::infinity();
    float exit_x = std::numeric_limits<float>::infinity();
    float entry_y = -std::numeric_limits<float>::infinity();
    float exit_y = std::numeric_limits<float>::infinity();

    const auto axis_times = [epsilon](
        float first_min,
        float first_max,
        float second_min,
        float second_max,
        float velocity,
        float& entry,
        float& exit) noexcept -> bool
    {
        if (std::fabs(velocity) <= epsilon)
            return first_max >= second_min && first_min <= second_max;
        const float first_time = (second_min - first_max) / velocity;
        const float second_time = (second_max - first_min) / velocity;
        entry = std::min(first_time, second_time);
        exit = std::max(first_time, second_time);
        return true;
    };

    if (!axis_times(
            first_previous.rect.left(), first_previous.rect.right(),
            second_previous.rect.left(), second_previous.rect.right(),
            relative.x, entry_x, exit_x)
        || !axis_times(
            first_previous.rect.top(), first_previous.rect.bottom(),
            second_previous.rect.top(), second_previous.rect.bottom(),
            relative.y, entry_y, exit_y))
    {
        return std::nullopt;
    }

    const float entry = std::max(entry_x, entry_y);
    const float exit = std::min(exit_x, exit_y);
    if (entry > exit || exit < 0.0f || entry < 0.0f || entry > 1.0f)
        return std::nullopt;

    CollisionHit hit;
    hit.time_of_impact = entry;
    if (entry_x >= entry_y)
        hit.manifold.normal = relative.x > 0.0f
            ? elysia::core::Vector2{1.0f, 0.0f}
            : elysia::core::Vector2{-1.0f, 0.0f};
    else
        hit.manifold.normal = relative.y > 0.0f
            ? elysia::core::Vector2{0.0f, 1.0f}
            : elysia::core::Vector2{0.0f, -1.0f};
    const elysia::core::Rect first_at_impact = first_previous.rect.translated(first_move * entry);
    const elysia::core::Rect second_at_impact = second_previous.rect.translated(second_move * entry);
    hit.manifold.contact_points[0] =
        (first_at_impact.center() + second_at_impact.center()) * 0.5f;
    hit.manifold.contact_point_count = 1;
    return hit;
}
}
