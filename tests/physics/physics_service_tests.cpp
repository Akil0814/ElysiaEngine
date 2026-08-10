#include "engine/physics/physics_service.h"
#include "tests/support/test_assertions.h"

#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

using elysia::tests::require;

namespace
{
class FakeBroadPhaseStrategy final : public elysia::physics::IBroadPhaseStrategy
{
public:
    explicit FakeBroadPhaseStrategy(int identity) noexcept
        : identity(identity)
    {
    }

    void collect_pairs(
        std::span<const elysia::physics::ColliderView> colliders,
        std::vector<elysia::physics::CollisionPair>& out_pairs
    ) const override
    {
        (void)colliders;
        (void)out_pairs;
    }

    int identity = 0;
};

class FakeDetectionStrategy final : public elysia::physics::ICollisionDetectionStrategy
{
public:
    explicit FakeDetectionStrategy(int identity) noexcept
        : identity(identity)
    {
    }

    [[nodiscard]] std::optional<elysia::physics::CollisionHit> detect(
        const elysia::physics::ColliderView& first,
        const elysia::physics::ColliderView& second,
        double delta_seconds
    ) const override
    {
        (void)first;
        (void)second;
        (void)delta_seconds;
        return std::nullopt;
    }

    int identity = 0;
};

class FakeResponseStrategy final : public elysia::physics::ICollisionResponseStrategy
{
public:
    explicit FakeResponseStrategy(int identity) noexcept
        : identity(identity)
    {
    }

    [[nodiscard]] elysia::physics::CollisionResponse resolve(
        const elysia::physics::ColliderView& first,
        const elysia::physics::ColliderView& second,
        const elysia::physics::CollisionHit& hit,
        double delta_seconds
    ) const override
    {
        (void)first;
        (void)second;
        (void)hit;
        (void)delta_seconds;
        return elysia::physics::CollisionResponse::Ignore;
    }

    int identity = 0;
};

elysia::physics::CollisionStrategyFactories complete_factories(
    int& broad_phase_count,
    int& discrete_count,
    int& continuous_count,
    int& response_count)
{
    return {
        [&broad_phase_count]
        {
            return std::make_unique<FakeBroadPhaseStrategy>(++broad_phase_count);
        },
        [&discrete_count]
        {
            return std::make_unique<FakeDetectionStrategy>(++discrete_count);
        },
        [&continuous_count]
        {
            return std::make_unique<FakeDetectionStrategy>(++continuous_count);
        },
        [&response_count]
        {
            return std::make_unique<FakeResponseStrategy>(++response_count);
        }
    };
}
}

int main()
{
    using namespace elysia::physics;

    PhysicsService* service = PhysicsService::instance();
    service->shutdown();

    require(!service->is_configured(),
        "PhysicsService must start unconfigured after shutdown");
    require(!service->configure(CollisionStrategyFactories{
            .create_broad_phase = []
            {
                return std::make_unique<FakeBroadPhaseStrategy>(1);
            }
        }),
        "PhysicsService must reject incomplete strategy factories");
    require(!service->is_configured(),
        "Rejected factories must not initialize PhysicsService");

    int broad_phase_count = 0;
    int discrete_count = 0;
    int continuous_count = 0;
    int response_count = 0;
    CollisionStrategyFactories factories = complete_factories(
        broad_phase_count,
        discrete_count,
        continuous_count,
        response_count
    );

    require(service->configure(factories),
        "PhysicsService must accept a complete strategy configuration");
    require(service->is_configured(),
        "Accepted factories must initialize PhysicsService");
    require(!service->configure(factories),
        "PhysicsService must reject reconfiguration before shutdown");

    CollisionSystem first_system;
    CollisionSystem second_system;
    require(service->apply_to(first_system),
        "PhysicsService must apply complete strategies to a collision system");
    require(service->apply_to(second_system),
        "PhysicsService must apply strategies to multiple collision systems");
    require(first_system.broad_phase_strategy()
            && first_system.discrete_detection_strategy()
            && first_system.continuous_detection_strategy()
            && first_system.response_strategy(),
        "Applied collision systems must receive every strategy");
    require(first_system.broad_phase_strategy() != second_system.broad_phase_strategy()
            && first_system.discrete_detection_strategy() != second_system.discrete_detection_strategy()
            && first_system.continuous_detection_strategy() != second_system.continuous_detection_strategy()
            && first_system.response_strategy() != second_system.response_strategy(),
        "Each collision system must receive independent strategy instances");
    require(broad_phase_count == 2 && discrete_count == 2 && continuous_count == 2
            && response_count == 2,
        "Each strategy factory must run once per applied collision system");

    service->shutdown();
    require(!service->is_configured(),
        "PhysicsService shutdown must clear its configuration");

    CollisionStrategyFactories null_factories{
        []
        {
            return std::make_unique<FakeBroadPhaseStrategy>(1);
        },
        []
        {
            return std::make_unique<FakeDetectionStrategy>(2);
        },
        []
        {
            return std::make_unique<FakeDetectionStrategy>(3);
        },
        []() -> std::unique_ptr<ICollisionResponseStrategy>
        {
            return nullptr;
        }
    };
    require(service->configure(std::move(null_factories)),
        "Complete callable slots must be accepted before their products are evaluated");

    CollisionSystem unchanged_system;
    auto existing_broad_phase = std::make_unique<FakeBroadPhaseStrategy>(10);
    auto existing_discrete = std::make_unique<FakeDetectionStrategy>(20);
    auto existing_continuous = std::make_unique<FakeDetectionStrategy>(30);
    auto existing_response = std::make_unique<FakeResponseStrategy>(40);
    const auto* existing_broad_phase_ptr = existing_broad_phase.get();
    const auto* existing_discrete_ptr = existing_discrete.get();
    const auto* existing_continuous_ptr = existing_continuous.get();
    const auto* existing_response_ptr = existing_response.get();
    unchanged_system.set_broad_phase_strategy(std::move(existing_broad_phase));
    unchanged_system.set_discrete_detection_strategy(std::move(existing_discrete));
    unchanged_system.set_continuous_detection_strategy(std::move(existing_continuous));
    unchanged_system.set_response_strategy(std::move(existing_response));

    require(!service->apply_to(unchanged_system),
        "PhysicsService must reject a strategy set containing a null product");
    require(unchanged_system.broad_phase_strategy() == existing_broad_phase_ptr
            && unchanged_system.discrete_detection_strategy() == existing_discrete_ptr
            && unchanged_system.continuous_detection_strategy() == existing_continuous_ptr
            && unchanged_system.response_strategy() == existing_response_ptr,
        "A failed apply must leave every existing collision strategy unchanged");

    service->shutdown();
    CollisionStrategyFactories throwing_factories{
        []() -> std::unique_ptr<IBroadPhaseStrategy>
        {
            throw std::runtime_error("factory failure");
        },
        []
        {
            return std::make_unique<FakeDetectionStrategy>(1);
        },
        []
        {
            return std::make_unique<FakeDetectionStrategy>(2);
        },
        []
        {
            return std::make_unique<FakeResponseStrategy>(3);
        }
    };
    require(service->configure(std::move(throwing_factories)),
        "PhysicsService must accept complete throwing factory callables");

    bool exception_propagated = false;
    try
    {
        (void)service->apply_to(unchanged_system);
    }
    catch (const std::runtime_error&)
    {
        exception_propagated = true;
    }
    require(exception_propagated,
        "PhysicsService must propagate strategy factory exceptions");
    require(unchanged_system.broad_phase_strategy() == existing_broad_phase_ptr
            && unchanged_system.discrete_detection_strategy() == existing_discrete_ptr
            && unchanged_system.continuous_detection_strategy() == existing_continuous_ptr
            && unchanged_system.response_strategy() == existing_response_ptr,
        "A throwing factory must leave the target collision system unchanged");

    service->shutdown();
    require(service->configure(complete_factories(
            broad_phase_count,
            discrete_count,
            continuous_count,
            response_count)),
        "PhysicsService must allow configuration again after shutdown");
    service->shutdown();

    std::cout << "physics service tests passed\n";
    return 0;
}
