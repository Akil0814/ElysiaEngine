#pragma once

#include "collision_event.h"
#include "../contracts/broad_phase_index.h"
#include "../contracts/collision_strategy.h"

#include <memory>
#include <span>

namespace elysia::physics
{
class ITileCollisionWorld;

class CollisionSystem
{
public:
    CollisionSystem() = default;

    void set_broad_phase_index(
        std::unique_ptr<IBroadPhaseIndex> index) noexcept;
    void set_discrete_detection_strategy(
        std::unique_ptr<ICollisionDetectionStrategy> strategy) noexcept;
    void set_continuous_detection_strategy(
        std::unique_ptr<ICollisionDetectionStrategy> strategy) noexcept;
    void set_response_strategy(
        std::unique_ptr<ICollisionResponseStrategy> strategy) noexcept;

    [[nodiscard]] const IBroadPhaseIndex* broad_phase_index() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy* discrete_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy* continuous_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionResponseStrategy* response_strategy() const noexcept;

    void evaluate(
        std::span<const ColliderView> collider_views,
        const ITileCollisionWorld* tile_world,
        double fixed_delta_seconds,
        CollisionFrame& out_frame) noexcept;

private:
    std::unique_ptr<IBroadPhaseIndex> _broad_phase_index;
    std::unique_ptr<ICollisionDetectionStrategy> _discrete_detection_strategy;
    std::unique_ptr<ICollisionDetectionStrategy> _continuous_detection_strategy;
    std::unique_ptr<ICollisionResponseStrategy> _response_strategy;
};
}
