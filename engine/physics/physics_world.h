#pragma once

#include "physics_object_handle.h"
#include "physics_world_config.h"
#include "physics_world_stats.h"
#include "body/physics_system.h"
#include "collision/collision_system.h"
#include "collision/contact_cache.h"
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
enum class TeleportVelocityMode : std::uint8_t
{
    Preserve,
    Clear
};

struct PhysicsContactState
{
    bool grounded = false;
    bool ceiling = false;
    bool wall_left = false;
    bool wall_right = false;
};

class PhysicsWorld final : public ICollisionQueryService
{
public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});
    PhysicsWorld(PhysicsWorldConfig config, CollisionStrategySet strategies);
    ~PhysicsWorld() override;

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;
    PhysicsWorld(PhysicsWorld&&) = delete;
    PhysicsWorld& operator=(PhysicsWorld&&) = delete;

    [[nodiscard]] PhysicsObjectHandle register_object(
        elysia::core::GameObject& owner,
        PhysicsBodyProvider* body_provider,
        ColliderProvider* collider_provider);
    [[nodiscard]] bool unregister_object(PhysicsObjectHandle handle);

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

    [[nodiscard]] bool teleport_object(
        PhysicsObjectHandle handle,
        elysia::core::Vector2 position,
        TeleportVelocityMode velocity_mode = TeleportVelocityMode::Preserve);
    [[nodiscard]] bool request_pass_through(
        ColliderId actor,
        CollisionTarget support);
    void collect_contacts(
        CollisionTarget target,
        std::vector<CollisionContact>& out_contacts) const;
    [[nodiscard]] PhysicsContactState contact_state(
        PhysicsObjectHandle object) const noexcept;

    [[nodiscard]] std::uint32_t advance(double frame_delta_seconds);
    void reset() noexcept;

    [[nodiscard]] const PhysicsWorldConfig& config() const noexcept;
    [[nodiscard]] double accumulator_seconds() const noexcept;
    [[nodiscard]] const PhysicsStepStats& last_step_stats() const noexcept;
    [[nodiscard]] const PhysicsDebugSnapshot& debug_snapshot() const noexcept;

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

    struct PendingListenerOperation
    {
        ICollisionListener* listener = nullptr;
        bool add = false;
    };

    struct PendingTeleport
    {
        PhysicsObjectHandle handle{};
        elysia::core::Vector2 position{};
        TeleportVelocityMode velocity_mode = TeleportVelocityMode::Preserve;
    };

    enum class PendingTileOperation : std::uint8_t
    {
        None,
        Set,
        Clear
    };

    void fixed_step(double fixed_delta_seconds);
    void flush_pending_operations();
    [[nodiscard]] std::optional<Registration> prepare_registration(
        elysia::core::GameObject& owner,
        PhysicsBodyProvider* body_provider,
        ColliderProvider* collider_provider);
    void commit_registration(Registration registration);
    [[nodiscard]] bool unregister_immediate(PhysicsObjectHandle handle) noexcept;
    [[nodiscard]] bool teleport_immediate(
        PhysicsObjectHandle handle,
        elysia::core::Vector2 position,
        TeleportVelocityMode velocity_mode) noexcept;
    void remove_cached_target(CollisionTarget target) noexcept;
    [[nodiscard]] std::optional<OneWayCollision> one_way_for_target(
        CollisionTarget target) const noexcept;
    [[nodiscard]] Registration* find_registration(PhysicsObjectHandle handle) noexcept;
    [[nodiscard]] const Registration* find_registration(PhysicsObjectHandle handle) const noexcept;
    [[nodiscard]] const Registration* find_registration(ColliderId collider) const noexcept;

    PhysicsWorldConfig _config{};
    std::vector<Registration> _registrations;
    std::unordered_map<ColliderId, Collider*> _collider_index;
    std::vector<ICollisionListener*> _listeners;
    const ITileCollisionWorld* _tile_world = nullptr;

    PhysicsSystem _physics_system;
    CollisionSystem _collision_system;
    ContactCache _contact_cache;
    CollisionFrame _collision_frame;
    std::vector<CollisionPair> _transient_ignored_pairs;
    PhysicsStepStats _last_step_stats{};
    PhysicsDebugSnapshot _debug_snapshot;

    std::vector<Registration> _pending_registrations;
    std::vector<PhysicsObjectHandle> _pending_unregistrations;
    std::vector<PendingListenerOperation> _pending_listener_operations;
    std::vector<PendingTeleport> _pending_teleports;
    PendingTileOperation _pending_tile_operation = PendingTileOperation::None;
    const ITileCollisionWorld* _pending_tile_world = nullptr;

    std::uint64_t _next_object_handle = 1;
    ColliderId _next_collider_id = 1;
    double _accumulator_seconds = 0.0;
    std::uint64_t _dropped_fixed_steps = 0;
    bool _advancing = false;
};
}
