#pragma once

#include "../collision/collision_query.h"

#include <optional>

namespace elysia::physics
{
class ICollisionQueryService
{
public:
    virtual ~ICollisionQueryService() = default;

    [[nodiscard]] virtual std::optional<CollisionQueryHit> raycast(
        const RayCastQuery& query
    ) const = 0;

    [[nodiscard]] virtual std::optional<CollisionQueryHit> segment_cast(
        const SegmentCastQuery& query
    ) const = 0;
};
}
