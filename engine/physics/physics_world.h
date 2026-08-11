#pragma once

#include "physics_object_handle.h"
#include "physics_world_config.h"
#include "body/physics_system.h"
#include "collision/collision_system.h"
#include "contracts/collider_provider.h"
#include "contracts/collision_listener.h"
#include "contracts/collision_query_service.h"
#include "contracts/physics_body_provider.h"
#include "tile/tile_collision_world.h"

#include "../core/game_object.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace elysia::physics
{
class PhysicsService;

class PhysicsWorld final : public ICollisionQueryService
{
public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});
    ~PhysicsWorld() override;

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    [[nodiscard]] bool configure_strategies(const PhysicsService& service);

    [[nodiscard]] PhysicsObjectHandle register_object(
        elysia::core::GameObject& owner,
        PhysicsBodyProvider* body_provider,
        ColliderProvider* collider_provider);
    [[nodiscard]] bool unregister_object(PhysicsObjectHandle handle) noexcept;

    [[nodiscard]] bool contains_object(PhysicsObjectHandle handle) const noexcept;
    [[nodiscard]] bool contains_object(const elysia::core::GameObject& owner) const noexcept;
    [[nodiscard]] bool contains_collider(ColliderId collider) const noexcept;
    [[nodiscard]] std::size_t registered_object_count() const noexcept;
    [[nodiscard]] std::size_t registered_collider_count() const noexcept;

    [[nodiscard]] bool set_tile_world(const ITileCollisionWorld& world) noexcept;
    [[nodiscard]] bool clear_tile_world(const ITileCollisionWorld& world) noexcept;
    [[nodiscard]] const ITileCollisionWorld* tile_world() const noexcept;

    [[nodiscard]] bool add_listener(ICollisionListener& listener) noexcept;
    [[nodiscard]] bool remove_listener(const ICollisionListener& listener) noexcept;

    [[nodiscard]] std::uint32_t advance(double frame_delta_seconds);
    void reset() noexcept;

    [[nodiscard]] const PhysicsWorldConfig& config() const noexcept;
    [[nodiscard]] double accumulator_seconds() const noexcept;

    [[nodiscard]] std::optional<CollisionQueryHit> raycast(
        const RayCastQuery& query) const override;
    [[nodiscard]] std::optional<CollisionQueryHit> segment_cast(
        const SegmentCastQuery& query) const override;

private:
    struct Registration
    {
        PhysicsObjectHandle handle{};
        elysia::core::GameObject* owner = nullptr;
        PhysicsBodyProvider* body_provider = nullptr;
        ColliderProvider* collider_provider = nullptr;
        PhysicsBody* body = nullptr;
        std::vector<Collider*> colliders;
        elysia::core::Vector2 previous_owner_origin{};
        elysia::core::Vector2 current_owner_origin{};
    };

    void fixed_step(double fixed_delta_seconds);
    [[nodiscard]] Registration* find_registration(PhysicsObjectHandle handle) noexcept;
    [[nodiscard]] const Registration* find_registration(PhysicsObjectHandle handle) const noexcept;

    PhysicsWorldConfig _config{};
    std::vector<Registration> _registrations;
    std::unordered_map<ColliderId, Collider*> _collider_index;
    std::vector<ICollisionListener*> _listeners;
    const ITileCollisionWorld* _tile_world = nullptr;

    PhysicsSystem _physics_system;
    CollisionSystem _collision_system;
    CollisionFrame _collision_frame;

    std::uint64_t _next_object_handle = 1;
    ColliderId _next_collider_id = 1;
    double _accumulator_seconds = 0.0;
    bool _advancing = false;
};
}
