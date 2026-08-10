#pragma once

#include "../contracts/collision_strategy.h"

#include <memory>
#include <vector>

namespace elysia::physics
{
class CollisionSystem
{
public:
    CollisionSystem() = default;

    void set_broad_phase_strategy(
        std::unique_ptr<IBroadPhaseStrategy> strategy
    ) noexcept;

    void set_discrete_detection_strategy(
        std::unique_ptr<ICollisionDetectionStrategy> strategy
    ) noexcept;

    void set_continuous_detection_strategy(
        std::unique_ptr<ICollisionDetectionStrategy> strategy
    ) noexcept;

    void set_response_strategy(
        std::unique_ptr<ICollisionResponseStrategy> strategy
    ) noexcept;

    [[nodiscard]] const IBroadPhaseStrategy* broad_phase_strategy() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy* discrete_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionDetectionStrategy* continuous_detection_strategy() const noexcept;
    [[nodiscard]] const ICollisionResponseStrategy* response_strategy() const noexcept;

    template <typename Entry>
    void dispatch_events(
        const std::vector<Entry>& collider_entries,
        double delta_seconds
    )
    {
        (void)collider_entries;
        (void)delta_seconds;
    }

private:
    std::unique_ptr<IBroadPhaseStrategy> _broad_phase_strategy;
    std::unique_ptr<ICollisionDetectionStrategy> _discrete_detection_strategy;
    std::unique_ptr<ICollisionDetectionStrategy> _continuous_detection_strategy;
    std::unique_ptr<ICollisionResponseStrategy> _response_strategy;
};
}
