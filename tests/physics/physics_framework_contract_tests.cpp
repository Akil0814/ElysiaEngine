#include "engine/physics/body/physics_system.h"
#include "engine/physics/collision/default_collision_strategies.h"
#include "engine/physics/collision/world_shape.h"
#include "engine/physics/physics_world_config.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

using elysia::tests::require;

int main()
{
    using namespace elysia::physics;

    static_assert(std::is_abstract_v<IBroadPhaseIndex>);
    static_assert(std::is_abstract_v<ICollisionDetectionStrategy>);
    static_assert(std::is_abstract_v<ICollisionResponseStrategy>);

    Collider collider;
    require(collider.id == InvalidColliderId && collider.enabled,
        "Colliders must start enabled with an invalid world ID");
    require(collider.response == CollisionResponse::Block
            && collider.detection_mode == CollisionDetectionMode::Discrete,
        "Collider response and detection defaults must be usable");
    require(collider.material == PhysicsMaterial{},
        "Colliders must use the stable default physics material");
    const auto normalized_material = normalized_physics_material({
        std::numeric_limits<float>::quiet_NaN(), -2.0f, 4.0f});
    require(normalized_material.static_friction == 0.0f
            && normalized_material.dynamic_friction == 0.0f
            && normalized_material.restitution == 1.0f,
        "Invalid material values must normalize without propagating NaN");
    const auto combined_material = combine_physics_materials(
        {0.9f, 0.4f, 0.2f}, {0.4f, 0.1f, 0.8f});
    require(std::fabs(combined_material.static_friction - 0.6f) < 0.0001f
            && std::fabs(combined_material.dynamic_friction - 0.2f) < 0.0001f
            && combined_material.restitution == 0.8f,
        "Material combination must use geometric-mean friction and max restitution");
    PhysicsBody body;
    require(body.type == BodyType::Dynamic,
        "Bodies must default to Dynamic");

    const auto collider_target = CollisionTarget::from_collider(4);
    const auto tile_target = CollisionTarget::from_tile({-2, 3});
    require(collider_target.is_valid() && tile_target.is_valid()
            && collider_target < tile_target,
        "Structured collision targets must be valid and stably ordered");
    const CollisionPair pair = normalized_collision_pair(tile_target, collider_target);
    require(pair.first == collider_target && pair.second == tile_target,
        "Collision pairs must normalize target order");
    require(CollisionTargetHash{}(collider_target)
            == CollisionTargetHash{}(CollisionTarget::from_collider(4)),
        "Equal targets must hash equally");

    CollisionFilter first{1u, 2u, 0};
    CollisionFilter second{2u, 1u, 0};
    require(collision_filters_allow(first, second),
        "Category/mask matching must be bidirectional");
    second.mask = 0;
    require(!collision_filters_allow(first, second),
        "Either rejected mask must reject the pair");
    first.group = second.group = 9;
    require(collision_filters_allow(first, second),
        "Equal positive groups must force acceptance");
    first.group = second.group = -9;
    require(!collision_filters_allow(first, second),
        "Equal negative groups must force rejection");

    const WorldAabb first_box{{0.0f, 0.0f, 10.0f, 10.0f}};
    const WorldAabb second_box{{10.0f, 2.0f, 8.0f, 8.0f}};
    const auto touching = detect_discrete_shapes(first_box, second_box);
    require(touching && touching->manifold.penetration == 0.0f
            && touching->manifold.normal == elysia::core::Vector2{1.0f, 0.0f},
        "AABB touching must hit with a first-to-second normal");
    const auto circles = detect_discrete_shapes(
        WorldCircle{{3.0f, 3.0f}, 2.0f}, WorldCircle{{3.0f, 3.0f}, 2.0f});
    require(circles && circles->manifold.normal == elysia::core::Vector2{1.0f, 0.0f},
        "Coincident circles must use the stable +X normal");
    const auto contained = detect_discrete_shapes(
        WorldAabb{{0, 0, 10, 10}}, WorldAabb{{4, 4, 2, 2}});
    require(contained && std::fabs(contained->manifold.penetration - 6.0f) < 0.0001f
            && contained->manifold.normal == elysia::core::Vector2{1, 0},
        "AABB containment must report the true minimum translation depth");
    CollisionShapeView first_circle_view;
    CollisionShapeView second_circle_view;
    first_circle_view.current_shape = WorldCircle{{0, 0}, 1};
    second_circle_view.current_shape = WorldCircle{{0, 0.001f}, 1};
    DefaultDiscreteCollisionStrategy discrete_strategy;
    const auto coarse_epsilon_hit = discrete_strategy.detect(
        first_circle_view, second_circle_view, {1.0 / 60.0, 0.01f});
    const auto fine_epsilon_hit = discrete_strategy.detect(
        first_circle_view, second_circle_view, {1.0 / 60.0, 0.0001f});
    require(coarse_epsilon_hit && fine_epsilon_hit
            && coarse_epsilon_hit->manifold.normal
                == elysia::core::Vector2{1, 0}
            && fine_epsilon_hit->manifold.normal.y > 0.99f,
        "Discrete detection must use the epsilon supplied by its context");

    PhysicsWorldConfig config;
    require(config.solver_iterations == 8
            && config.position_correction_percent == 0.8f
            && config.restitution_velocity_threshold == 1.0f
            && config.max_tile_candidates_per_operation == 65536,
        "The default solver configuration must favor stable resting contacts");
    config.gravity = {0.0f, 10.0f};
    PhysicsObjectState dynamic_state{PhysicsObjectHandle{1}, &body, {}, {}};
    body.velocity = {2.0f, 0.0f};
    body.accumulated_force = {0.0f, 10.0f};
    body.mass = 2.0f;
    std::array states{dynamic_state};
    PhysicsSystem{}.integrate(states, config, 0.5);
    require(states[0].current_owner_origin.nearly_equals({1.0f, 3.75f})
            && body.velocity.nearly_equals({2.0f, 7.5f})
            && body.accumulated_force == elysia::core::Vector2{},
        "Dynamic integration must apply gravity/force using semi-implicit Euler");

    std::vector<BroadPhaseProxy> proxies{
        {1, {0, 0, 10, 10}, {0, 0, 10, 10}, {1, 0xffffffffu, 0}, true},
        {2, {10, 0, 10, 10}, {10, 0, 10, 10}, {2, 0xffffffffu, 0}, true},
        {3, {50, 0, 10, 10}, {50, 0, 10, 10}, {4, 0xffffffffu, 0}, true}
    };
    BruteForceBroadPhaseIndex brute;
    SweepAndPruneBroadPhaseIndex sap;
    brute.synchronize(proxies);
    sap.synchronize(proxies);
    std::vector<BroadPhasePair> brute_pairs;
    std::vector<BroadPhasePair> sap_pairs;
    brute.collect_pairs(brute_pairs);
    sap.collect_pairs(sap_pairs);
    require(brute_pairs == sap_pairs
            && brute_pairs == std::vector<BroadPhasePair>{{1, 2}},
        "SAP must match the Brute Force candidate oracle, including touching bounds");
    proxies[1].filter.mask = 0;
    brute.synchronize(proxies);
    sap.synchronize(proxies);
    brute.collect_pairs(brute_pairs);
    sap.collect_pairs(sap_pairs);
    require(brute_pairs.empty() && sap_pairs.empty(),
        "Broad-phase indices must consistently reject filter-incompatible bounds");
    proxies[1].filter.mask = 0xffffffffu;
    std::mt19937 random(0xE1751Au);
    std::uniform_real_distribution<float> position(-200.0f, 200.0f);
    std::uniform_real_distribution<float> size(1.0f, 30.0f);
    for (const ColliderId count : {ColliderId{100}, ColliderId{500}, ColliderId{1000}})
    {
        proxies.clear();
        for (ColliderId id = 1; id <= count; ++id)
        {
            const elysia::core::Rect current{
                position(random), position(random), size(random), size(random)};
            const elysia::core::Rect swept = current.merged(
                current.translated({position(random) * 0.1f, position(random) * 0.1f}));
            proxies.push_back({id, current, swept, {}, true});
        }
        brute.synchronize(proxies);
        sap.synchronize(proxies);
        brute.collect_pairs(brute_pairs);
        sap.collect_pairs(sap_pairs);
        require(brute_pairs == sap_pairs
                && sap_pairs.size() <= static_cast<std::size_t>(count * (count - 1) / 2),
            "SAP 100/500/1000 output must equal the Brute Force candidate oracle");
    }

    CollisionSystem default_system;
    CollisionSystem brute_system(make_brute_force_collision_strategies());
    require(dynamic_cast<const SweepAndPruneBroadPhaseIndex*>(
                &default_system.broad_phase_index()) != nullptr
            && dynamic_cast<const BruteForceBroadPhaseIndex*>(
                &brute_system.broad_phase_index()) != nullptr,
        "Default systems must use SAP while Brute Force remains injectable");

    std::cout << "physics framework contract tests passed\n";
    return 0;
}
