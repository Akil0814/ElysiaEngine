#pragma once

#include "../collision/collider.h"

#include "../../core/geometry/rect.h"

#include <span>
#include <vector>

namespace elysia::physics
{
struct BroadPhasePair
{
    ColliderId first = InvalidColliderId;
    ColliderId second = InvalidColliderId;
};

struct BroadPhaseProxy
{
    ColliderId collider = InvalidColliderId;
    elysia::core::Rect current_bounds{};
    elysia::core::Rect swept_bounds{};
    CollisionFilter filter{};
    bool enabled = true;
};

class IBroadPhaseIndex
{
public:
    virtual ~IBroadPhaseIndex() = default;

    virtual void synchronize(std::span<const BroadPhaseProxy> proxies) = 0;
    virtual void collect_pairs(std::vector<BroadPhasePair>& out_pairs) const = 0;
    virtual void query_aabb(
        const elysia::core::Rect& bounds,
        std::vector<ColliderId>& out_candidates) const = 0;
    virtual void clear() noexcept = 0;
};
}
