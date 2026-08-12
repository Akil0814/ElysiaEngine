#pragma once

#include "collision_event.h"
#include "../body/physics_system.h"
#include "../contracts/broad_phase_index.h"
#include "../contracts/collision_strategy.h"
#include "../physics_world_config.h"
#include "../physics_world_stats.h"

#include <memory>
#include <span>
#include <vector>

namespace elysia::physics
{
class ITileCollisionWorld;

struct CollisionStrategySet
{
    std::unique_ptr<IBroadPhaseIndex> broad_phase;
    std::unique_ptr<ICollisionDetectionStrategy> discrete_detection;
    std::unique_ptr<ICollisionDetectionStrategy> continuous_detection;
    std::unique_ptr<ICollisionResponseStrategy> response;

    [[nodiscard]] bool is_complete() const noexcept
    {
        return broad_phase && discrete_detection
            && continuous_detection && response;
    }
};

class CollisionSystem
{
public:
    CollisionSystem();
    explicit CollisionSystem(CollisionStrategySet strategies);

    CollisionSystem(const CollisionSystem&) = delete;
    CollisionSystem& operator=(const CollisionSystem&) = delete;
    CollisionSystem(CollisionSystem&&) noexcept = default;
    CollisionSystem& operator=(CollisionSystem&&) noexcept = default;

    [[nodiscard]] const IBroadPhaseIndex& broad_phase_index() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy& discrete_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy& continuous_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionResponseStrategy& response_strategy() const noexcept;

    void evaluate(
        std::span<PhysicsObjectState> object_states,
        std::span<const CollisionShapeView> collider_views,
        const ITileCollisionWorld* tile_world,
        std::span<const CollisionPair> transient_ignored_pairs,
        const PhysicsWorldConfig& config,
        double fixed_delta_seconds,
        CollisionFrame& out_frame,
        PhysicsStepStats& stats,
        PhysicsDebugCapture debug_capture,
        PhysicsDebugSnapshot* debug_snapshot);

    void query_aabb(
        const elysia::core::Rect& bounds,
        std::vector<ColliderId>& out_candidates) const;

    void clear() noexcept;

private:
    CollisionStrategySet _strategies;
};
}
