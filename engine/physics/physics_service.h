#pragma once

#include "collision/collision_system.h"
#include "../tools/singleton.h"

#include <functional>
#include <memory>

#define ELYSIA_PHYSICS (::elysia::physics::PhysicsService::instance())

namespace elysia::physics
{
using BroadPhaseStrategyFactory =std::function<std::unique_ptr<IBroadPhaseStrategy>()>;

using CollisionDetectionStrategyFactory =std::function<std::unique_ptr<ICollisionDetectionStrategy>()>;

using CollisionResponseStrategyFactory =std::function<std::unique_ptr<ICollisionResponseStrategy>()>;

struct CollisionStrategyFactories
{
    BroadPhaseStrategyFactory create_broad_phase;
    CollisionDetectionStrategyFactory create_discrete_detector;
    CollisionDetectionStrategyFactory create_continuous_detector;
    CollisionResponseStrategyFactory create_response_strategy;
};

class PhysicsService final : public elysia::tools::Singleton<PhysicsService>
{
    friend elysia::tools::Singleton<PhysicsService>;

public:
    [[nodiscard]] bool configure(CollisionStrategyFactories factories);
    [[nodiscard]] bool apply_to(CollisionSystem& collision_system) const;

    [[nodiscard]] bool is_configured() const noexcept;
    void shutdown() noexcept;

private:
    PhysicsService() = default;

    CollisionStrategyFactories _factories{};
    bool _configured = false;
};
}
