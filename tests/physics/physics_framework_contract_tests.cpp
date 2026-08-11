#include "engine/gameplay/collision/actor_collision_rig.h"
#include "engine/gameplay/collision/gameplay_collision_events.h"
#include "engine/gameplay/collision/gameplay_collision_listener.h"
#include "engine/gameplay/collision/gameplay_collision_service.h"
#include "engine/gameplay/collision/team_relation_resolver.h"
#include "engine/physics/body/physics_body.h"
#include "engine/physics/body/physics_system.h"
#include "engine/physics/collision/collision_event.h"
#include "engine/physics/collision/collider.h"
#include "engine/physics/contracts/broad_phase_index.h"
#include "engine/physics/collision/collision_contact.h"
#include "engine/physics/collision/collision_query.h"
#include "engine/physics/collision/collision_system.h"
#include "engine/physics/contracts/collision_query_service.h"
#include "engine/physics/contracts/collision_strategy.h"
#include "engine/physics/physics_world_config.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

using elysia::tests::require;

namespace
{
class FakeBroadPhaseIndex final : public elysia::physics::IBroadPhaseIndex
{
public:
    void synchronize(std::span<const elysia::physics::BroadPhaseProxy> proxies) override
    {
        (void)proxies;
    }

    void collect_pairs(std::vector<elysia::physics::BroadPhasePair>& out_pairs) const override
    {
        out_pairs.clear();
    }

    void query_aabb(
        const elysia::core::Rect& bounds,
        std::vector<elysia::physics::ColliderId>& out_candidates) const override
    {
        (void)bounds;
        out_candidates.clear();
    }

    void clear() noexcept override {}
};

class FakeDetectionStrategy final : public elysia::physics::ICollisionDetectionStrategy
{
public:
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
};

class FakeResponseStrategy final : public elysia::physics::ICollisionResponseStrategy
{
public:
    [[nodiscard]] elysia::physics::CollisionResponse resolve(
        const elysia::physics::ColliderView& first,
        const elysia::physics::ColliderView& second,
        const elysia::physics::CollisionHit& hit,
        double delta_seconds
    ) const override
    {
        (void)second;
        (void)hit;
        (void)delta_seconds;
        return first.collider
            ? first.collider->response
            : elysia::physics::CollisionResponse::Ignore;
    }
};
}

int main()
{
    using namespace elysia::physics;
    using namespace elysia::gameplay::collision;

    static_assert(!std::is_abstract_v<GameplayCollisionService>);
    static_assert(std::is_final_v<GameplayCollisionService>);
    static_assert(std::is_abstract_v<IGameplayCollisionRuntime>);
    static_assert(std::is_abstract_v<TeamRelationResolver>);
    static_assert(std::is_abstract_v<IBroadPhaseIndex>);
    static_assert(std::is_abstract_v<ICollisionDetectionStrategy>);
    static_assert(std::is_abstract_v<ICollisionResponseStrategy>);
    static_assert(std::is_abstract_v<ICollisionQueryService>);

    Collider collider;
    require(collider.id == InvalidColliderId, "Collider IDs must default to invalid");
    require(collider.filter.category == 0, "Collider categories must default to empty");
    require(collider.filter.mask == 0xffffffffu, "Collider masks must accept all categories by default");
    require(collider.response == CollisionResponse::Block, "Colliders must default to blocking intent");
    require(collider.detection_mode == CollisionDetectionMode::Discrete,
        "Colliders must default to discrete detection");
    require(collider.enabled, "Colliders must default to enabled");
    require(std::holds_alternative<AabbShape>(collider.shape),
        "Colliders must default to an AABB shape");
    require(!collider.one_way.has_value(),
        "Colliders must default to ordinary non-one-way behavior");

    PassThroughDirection pass_through = PassThroughDirection::Up;
    pass_through |= PassThroughDirection::Left;
    require(has_pass_through_direction(pass_through, PassThroughDirection::Up)
            && has_pass_through_direction(pass_through, PassThroughDirection::Left)
            && !has_pass_through_direction(pass_through, PassThroughDirection::Down),
        "Pass-through directions must support flag composition and queries");
    require(!has_pass_through_direction(pass_through, PassThroughDirection::None),
        "None must not be reported as an enabled pass-through direction");

    collider.one_way = OneWayCollision{pass_through};
    require(collider.one_way->pass_through == pass_through,
        "One-way collision settings must preserve pass-through directions");
    require(collider.one_way->tolerance == 0.01f,
        "One-way collision settings must default to the documented tolerance");

    collider.shape = CircleShape{elysia::core::Vector2(3.0f, 4.0f), 5.0f};
    const CircleShape* circle = std::get_if<CircleShape>(&collider.shape);
    require(circle && circle->local_center == elysia::core::Vector2(3.0f, 4.0f),
        "Collider shapes must preserve circle-local centers");
    require(circle && circle->radius == 5.0f,
        "Collider shapes must preserve circle radii");

    collider.response = CollisionResponse::Overlap;
    require(collider.response == CollisionResponse::Overlap,
        "Overlap must be the canonical non-blocking collision response");

    CollisionContact contact;
    require(!contact.pair.first.is_valid() && !contact.pair.second.is_valid(),
        "Collision contacts must default to invalid targets");
    require(contact.response == CollisionResponse::Ignore,
        "Collision contacts must default to an ignored response");
    require(contact.manifold.normal == elysia::core::Vector2::zero(),
        "Collision manifolds must default to a zero normal");

    BroadPhasePair broad_phase_pair;
    BroadPhaseProxy broad_phase_proxy;
    require(broad_phase_pair.first == InvalidColliderId
            && broad_phase_pair.second == InvalidColliderId,
        "Broad-phase pairs must default to invalid collider IDs");
    require(broad_phase_proxy.collider == InvalidColliderId
            && broad_phase_proxy.enabled,
        "Broad-phase proxies must default to an invalid enabled snapshot");

    CollisionEvent collision_event;
    require(collision_event.phase == CollisionEventPhase::Begin
            && !collision_event.contact.pair.first.is_valid(),
        "Collision events must default to an invalid Begin event");
    require(contact.manifold.penetration == 0.0f && contact.manifold.contact_point_count == 0,
        "Collision manifolds must default to no penetration or contact points");

    CollisionHit hit;
    require(hit.time_of_impact == 1.0f,
        "Collision hits must default to the end of the normalized frame interval");

    RayCastQuery ray_query;
    SegmentCastQuery segment_query;
    CollisionQueryHit query_hit;
    require(ray_query.max_distance == 0.0f,
        "Ray queries must default to zero distance");
    require(segment_query.start == segment_query.end,
        "Segment queries must default to an empty segment");
    require(!query_hit.target.is_valid(),
        "Collision query hits must default to an invalid target");

    ActorCollisionRig rig;
    require(rig.owner == InvalidActorId, "Actor rigs must default to an invalid owner");
    require(rig.body == InvalidColliderId, "Actor body references must default to invalid");
    require(rig.push_box == InvalidColliderId, "Actor push-box references must default to invalid");
    require(teams::Player != teams::Enemy, "Player and enemy presets must be distinct");
    require(teams::Neutral != teams::Player, "Neutral and player presets must be distinct");

    PhysicsBody body;
    body.velocity = elysia::core::Vector2(12.0f, -4.0f);
    require(body.type == BodyType::Dynamic,
        "Physics bodies must default to dynamic behavior");

    PhysicsSystem physics_system;
    PhysicsBodyView body_view{
        PhysicsObjectHandle{1},
        &body,
        {},
        {}
    };
    std::array<PhysicsBodyView, 1> body_views{body_view};
    physics_system.integrate(body_views, PhysicsWorldConfig{}, 1.0 / 60.0);
    require(body.velocity == elysia::core::Vector2(12.0f, -4.0f),
        "The physics scaffold must not mutate bodies");

    CollisionSystem collision_system;
    require(!collision_system.broad_phase_index()
            && !collision_system.discrete_detection_strategy()
            && !collision_system.continuous_detection_strategy()
            && !collision_system.response_strategy(),
        "Collision systems must allow an unconfigured strategy set");

    auto broad_phase = std::make_unique<FakeBroadPhaseIndex>();
    const FakeBroadPhaseIndex* broad_phase_ptr = broad_phase.get();
    collision_system.set_broad_phase_index(std::move(broad_phase));
    require(collision_system.broad_phase_index() == broad_phase_ptr,
        "Collision systems must own the configured broad-phase index");

    auto discrete = std::make_unique<FakeDetectionStrategy>();
    const FakeDetectionStrategy* discrete_ptr = discrete.get();
    collision_system.set_discrete_detection_strategy(std::move(discrete));
    require(collision_system.discrete_detection_strategy() == discrete_ptr,
        "Collision systems must own the configured discrete detector");

    auto continuous = std::make_unique<FakeDetectionStrategy>();
    const FakeDetectionStrategy* continuous_ptr = continuous.get();
    collision_system.set_continuous_detection_strategy(std::move(continuous));
    require(collision_system.continuous_detection_strategy() == continuous_ptr,
        "Collision systems must own the configured continuous detector");

    auto response = std::make_unique<FakeResponseStrategy>();
    const FakeResponseStrategy* response_ptr = response.get();
    collision_system.set_response_strategy(std::move(response));
    require(collision_system.response_strategy() == response_ptr,
        "Collision systems must own the configured response strategy");

    const ColliderView collider_view{
        PhysicsObjectHandle{1},
        &collider,
        {},
        {}
    };
    require(response_ptr->resolve(collider_view, ColliderView{}, hit, 1.0 / 60.0)
            == collider.response,
        "Response strategies must receive collider views, hit data and frame delta");

    collision_system.set_broad_phase_index(nullptr);
    collision_system.set_response_strategy(nullptr);
    require(!collision_system.broad_phase_index(),
        "Collision index slots must support explicit clearing");
    require(!collision_system.response_strategy(),
        "Response strategy slots must support explicit clearing");

    CollisionFrame frame;
    frame.contacts.push_back(CollisionContact{});
    collision_system.evaluate({}, nullptr, 1.0 / 60.0, frame);
    require(frame.contacts.empty() && frame.events.empty(),
        "The collision scaffold must clear its output frame");

    GameplayCollisionListener listener;
    listener.on_body_contact(BodyContactEvent{});
    listener.on_push_box_overlap(PushBoxOverlapEvent{});
    listener.on_hit_overlap(HitOverlapEvent{});

    std::cout << "physics framework contract tests passed\n";
    return 0;
}
