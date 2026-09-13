#include "engine/physics/physics_world.h"
#include "engine/physics/collision/default_collision_strategies.h"
#include "engine/gameplay/collision/gameplay_collision_runtime.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <iostream>

using namespace elysia::physics;
using elysia::tests::require;

namespace
{
struct Probe : elysia::core::GameObject, PhysicsBodyProvider, ColliderProvider
{
    Probe() : GameObject(elysia::core::DepthLayer::Character)
    {
        set_world_rect({0, 0, 10, 10});
        collider.shape = AabbShape{{0, 0, 10, 10}};
        collider.response = CollisionResponse::Overlap;
        body.gravity_scale = 0;
    }
    PhysicsBody* physics_body() noexcept override { return &body; }
    const PhysicsBody* physics_body() const noexcept override { return &body; }
    std::span<Collider> colliders() noexcept override { return {&collider, 1}; }
    std::span<const Collider> colliders() const noexcept override { return {&collider, 1}; }
    PhysicsBody body;
    Collider collider;
};

struct Listener : ICollisionListener
{
    std::function<void(const CollisionEvent&)> callback;
    void on_collision_event(const CollisionEvent& event) override { callback(event); }
};

struct ForceProbe : Probe, PhysicsStepParticipant
{
    int steps = 0;
    void fixed_update(double) override
    {
        ++steps;
        body.accumulated_force += elysia::core::Vector2{60, 0};
    }
};

struct TileWorld : ITileCollisionWorld
{
    elysia::core::Vector2 world_origin() const noexcept override { return {}; }
    elysia::core::Vector2 tile_size() const noexcept override { return {20, 20}; }
    int columns() const noexcept override { return 1; }
    int rows() const noexcept override { return 1; }
    TileOutOfBoundsPolicy out_of_bounds_policy() const noexcept override { return TileOutOfBoundsPolicy::Empty; }
    TileCollisionCell cell_at(TileCoordinate) const noexcept override { return {TileCollisionType::Overlap}; }
};

void tile_replacement_invalidates_the_old_world()
{
    Probe object;
    TileWorld old_tiles, new_tiles;
    PhysicsWorld world;
    Listener listener;
    std::vector<CollisionEventPhase> phases;
    listener.callback = [&](const CollisionEvent& event)
    {
        phases.push_back(event.phase);
        if (phases.size() == 1)
        {
            require(world.clear_tile_world(old_tiles), "Old tiles clear during callback");
            require(world.set_tile_world(new_tiles), "New tiles bind in the same callback");
        }
    };
    require(world.register_object(object, &object, &object).is_valid(), "Tile probe registers");
    require(world.set_tile_world(old_tiles) && world.add_listener(listener), "Tiles and listener attach");
    (void)world.advance(1.0 / 60);
    std::vector<CollisionContact> contacts;
    world.collect_contacts(CollisionTarget::from_tile({0, 0}), contacts);
    require(world.tile_world() == &new_tiles && contacts.empty(), "Tile replacement must clear old cached contacts");
    (void)world.advance(1.0 / 60);
    require(phases == std::vector{CollisionEventPhase::Begin, CollisionEventPhase::End, CollisionEventPhase::Begin},
        "Same coordinates in a new map must end the old contact before beginning the new one");
    require(world.remove_listener(listener), "Listener detaches");
}

void invalidation_ends_contacts_once()
{
    for (bool teleport : {false, true})
    {
        Probe first, second;
        PhysicsWorld world;
        Listener listener;
        std::vector<CollisionEventPhase> phases;
        listener.callback = [&](const CollisionEvent& event) { phases.push_back(event.phase); };
        const auto handle = world.register_object(first, &first, &first);
        require(world.register_object(second, &second, &second).is_valid(), "Other probe registers");
        require(world.add_listener(listener), "Listener attaches");
        (void)world.advance(1.0 / 60);
        if (teleport) require(world.teleport_object(handle, {100, 0}), "Teleport invalidates contact");
        else require(world.unregister_object(handle), "Unregister invalidates contact");
        (void)world.advance(1.0 / 60);
        (void)world.advance(1.0 / 60);
        require(phases == std::vector{CollisionEventPhase::Begin, CollisionEventPhase::End},
            "Invalidating a contact emits exactly one End without requiring a live object");
        require(world.remove_listener(listener), "Listener detaches");
    }
}

void gameplay_end_survives_actor_unbinding()
{
    using namespace elysia::gameplay::collision;
    struct SensorListener : GameplayCollisionListener
    {
        std::vector<CollisionEventPhase> phases;
        void on_sensor_overlap(const SensorOverlapEvent& event) override
        {
            require(event.body.owner == 2, "End preserves the retired actor identity");
            phases.push_back(event.phase);
        }
    } listener;
    Probe sensor, body;
    PhysicsWorld world;
    require(world.register_object(sensor, &sensor, &sensor).is_valid(), "Sensor registers");
    const auto handle = world.register_object(body, &body, &body);
    GameplayCollisionRuntime runtime(world);
    require(runtime.bind_collider({sensor.collider.id, 1, teams::Neutral, ColliderRole::Sensor}), "Sensor binds");
    require(runtime.bind_collider({body.collider.id, 2, teams::Player, ColliderRole::Body}), "Body binds");
    require(runtime.add_listener(listener), "Gameplay listener attaches");
    (void)world.advance(1.0 / 60);
    require(runtime.unbind_actor(2) && world.unregister_object(handle), "Actor unbinds and unregisters");
    (void)world.advance(1.0 / 60);
    require(listener.phases == std::vector{CollisionEventPhase::Begin, CollisionEventPhase::End},
        "Sensor occupancy receives End even after actor metadata was unbound");
}

void fixed_step_controls_are_frame_rate_independent()
{
    for (const int fps : {30, 60, 120, 144})
    {
        ForceProbe object;
        PhysicsWorld world;
        require(world.register_object(object, &object, nullptr).is_valid(), "Force probe registers");
        for (int frame = 0; frame < fps; ++frame)
            (void)world.advance(1.0 / fps);
        require(object.steps == 60, "Control runs once per physical step at every display rate");
        require(object.body.velocity.nearly_equals({60, 0}), "Constant force must not depend on display rate");
        require(object.position().nearly_equals({30.5f, 0}), "Integrated displacement must not depend on display rate");
    }
}

void fixed_step_callbacks_respect_lifecycle_boundaries()
{
    struct Mutator : Probe, PhysicsStepParticipant
    {
        std::function<void()> callback;
        void fixed_update(double) override { callback(); }
    } mutator;
    ForceProbe victim, spawned;
    PhysicsWorld world;
    require(world.register_object(mutator, &mutator, nullptr).is_valid(), "Mutator registers");
    const auto victim_handle = world.register_object(victim, &victim, nullptr);
    bool first = true;
    mutator.callback = [&]
    {
        if (!first) return;
        first = false;
        require(world.unregister_object(victim_handle), "Fixed callback can queue removal");
        require(world.register_object(spawned, &spawned, nullptr).is_valid(), "Fixed callback can register another object");
    };
    (void)world.advance(1.0 / 60);
    require(victim.steps == 0 && spawned.steps == 0, "Removed objects skip control; newly registered objects start callbacks next step");
    (void)world.advance(1.0 / 60);
    require(spawned.steps == 1, "Spawned participant runs on the next physical step");
    spawned.set_active(false);
    (void)world.advance(1.0 / 60);
    require(spawned.steps == 1, "Inactive objects do not run fixed control");
    spawned.set_active(true);
    mutator.callback = [&] { world.reset(); };
    require(world.advance(3.0 / 60) == 1 && world.registered_object_count() == 0,
        "Reset during fixed control stops later callbacks and catch-up steps");
    require(spawned.steps == 1 && spawned.body.accumulated_force.is_zero(),
        "Reset must not run or accumulate force on later participants");
}

void callback_commands_preserve_order()
{
    Probe first, second, spawned;
    PhysicsWorld world;
    Listener listener;
    bool ran = false;
    listener.callback = [&](const CollisionEvent&)
    {
        if (ran) return;
        ran = true;
        const auto handle = world.register_object(spawned, &spawned, &spawned);
        require(handle.is_valid(), "Callback registration must reserve a handle");
        require(world.teleport_object(handle, {100, 0}), "Reserved objects accept teleport");
        require(world.teleport_object(handle, {200, 0}, TeleportVelocityMode::Clear), "Later teleport replaces earlier location");
    };
    require(world.register_object(first, &first, &first).is_valid(), "First probe registers");
    require(world.register_object(second, &second, &second).is_valid(), "Second probe registers");
    require(world.add_listener(listener), "Listener attaches");
    (void)world.advance(1.0 / 60);
    require(ran && spawned.position().nearly_equals({200, 0}), "Reserved object must receive queued teleports in order");
    require(spawned.render_rect().position() == spawned.position(), "Spawn teleport must not interpolate from old origin");
    require(world.remove_listener(listener), "Listener detaches");
}

void removed_objects_reject_later_commands()
{
    Probe first, second;
    PhysicsWorld world;
    Listener listener;
    const auto handle = world.register_object(first, &first, &first);
    require(world.register_object(second, &second, &second).is_valid(), "Second probe registers");
    listener.callback = [&](const CollisionEvent&)
    {
        require(world.teleport_object(handle, {100, 0}), "Teleport before removal accepted");
        require(world.unregister_object(handle), "Removal accepted");
        require(!world.teleport_object(handle, {200, 0}), "Teleport after queued removal rejected");
        require(!world.register_object(first, &first, &first).is_valid(), "Re-register during pending removal must not return dying handle");
    };
    require(world.add_listener(listener), "Listener attaches");
    (void)world.advance(1.0 / 60);
    require(first.position().nearly_equals({100, 0}) && !world.contains_object(handle), "Teleport executes before removal");
    require(first.collider.id == InvalidColliderId, "Removal clears provider ID");
    require(world.remove_listener(listener), "Listener detaches");
}

void ccd_events_follow_the_accepted_path()
{
    for (float sensor_x : {5.0f, 60.0f})
    {
        Probe moving, wall, sensor;
        moving.collider.response = CollisionResponse::Block;
        moving.collider.detection_mode = CollisionDetectionMode::Continuous;
        moving.body.velocity = {6000, 0};
        wall.set_position({20, 0});
        wall.body.type = BodyType::Static;
        wall.collider.response = CollisionResponse::Block;
        sensor.set_position({sensor_x, 0});
        sensor.body.type = BodyType::Static;
        PhysicsWorld world;
        Listener listener;
        int hits = 0;
        require(world.register_object(moving, &moving, &moving).is_valid(), "Mover registers");
        require(world.register_object(wall, &wall, &wall).is_valid(), "Wall registers");
        require(world.register_object(sensor, &sensor, &sensor).is_valid(), "Sensor registers");
        listener.callback = [&](const CollisionEvent& event)
        {
            if (event.phase == CollisionEventPhase::Begin
                && (event.contact.pair.first == CollisionTarget::from_collider(sensor.collider.id)
                    || event.contact.pair.second == CollisionTarget::from_collider(sensor.collider.id))) ++hits;
        };
        require(world.add_listener(listener), "Listener attaches");
        (void)world.advance(1.0 / 60);
        require(moving.position().nearly_equals({10, 0}), "CCD stops before the wall");
        require(hits == (sensor_x < 20 ? 1 : 0), "Only sensors on the accepted path may receive Begin");
        require(world.remove_listener(listener), "Listener detaches");
    }
}

void ccd_bounces_rebuild_candidates_and_preserve_crossed_sensors()
{
    for (bool reverse_registration : {false, true})
    {
        Probe moving, right_wall, left_sensor;
        moving.collider.response = CollisionResponse::Block;
        moving.collider.detection_mode = CollisionDetectionMode::Continuous;
        moving.body.velocity = {6000, 0};
        right_wall.set_position({30, 0});
        right_wall.body.type = BodyType::Static;
        right_wall.collider.response = CollisionResponse::Block;
        right_wall.collider.material.restitution = 1;
        left_sensor.set_position({-30, 0});
        left_sensor.body.type = BodyType::Static;
        PhysicsWorld world;
        Listener listener;
        int sensor_entries = 0;
        if (reverse_registration)
            require(world.register_object(right_wall, &right_wall, &right_wall).is_valid(), "Wall registers first");
        require(world.register_object(moving, &moving, &moving).is_valid(), "Mover registers");
        if (!reverse_registration)
            require(world.register_object(right_wall, &right_wall, &right_wall).is_valid(), "Wall registers last");
        require(world.register_object(left_sensor, &left_sensor, &left_sensor).is_valid(), "Sensor registers");
        listener.callback = [&](const CollisionEvent& event)
        {
            const auto target = CollisionTarget::from_collider(left_sensor.collider.id);
            if (event.phase == CollisionEventPhase::Begin
                && (event.contact.pair.first == target || event.contact.pair.second == target)) ++sensor_entries;
        };
        require(world.add_listener(listener), "Listener attaches");
        (void)world.advance(1.0 / 60);
        require(moving.position().nearly_equals({-60, 0}, 0.001f), "Bounce integrates the remaining physical time");
        require(sensor_entries == 1, "Bounce must detect a crossed sensor outside the original sweep even when final positions do not overlap");
        require(world.remove_listener(listener), "Listener detaches");
    }
}

void ccd_budget_stops_at_a_safe_impact()
{
    Probe moving, wall;
    moving.collider.response = CollisionResponse::Block;
    moving.collider.detection_mode = CollisionDetectionMode::Continuous;
    moving.body.velocity = {6000, 0};
    wall.set_position({30, 0});
    wall.body.type = BodyType::Static;
    wall.collider.response = CollisionResponse::Block;
    wall.collider.material.restitution = 1;
    PhysicsWorldConfig config;
    config.max_ccd_iterations = 1;
    PhysicsWorld world(config);
    require(world.register_object(moving, &moving, &moving).is_valid(), "Mover registers");
    require(world.register_object(wall, &wall, &wall).is_valid(), "Wall registers");
    (void)world.advance(1.0 / 60);
    require(moving.position().nearly_equals({20, 0}), "Exhausting the CCD budget must not integrate an unchecked remainder");
}

void solver_uses_the_injected_detection_contract()
{
    struct MarginDetection final : ICollisionDetectionStrategy
    {
        std::optional<CollisionHit> detect(const CollisionShapeView& first,
            const CollisionShapeView& second, const CollisionDetectionContext& context) const noexcept override
        {
            return detect_discrete_shapes(first.current_shape,
                translated_shape(second.current_shape, {-1, 0}), context.epsilon);
        }
    };
    Probe moving, wall;
    moving.collider.response = CollisionResponse::Block;
    wall.collider.response = CollisionResponse::Block;
    wall.body.type = BodyType::Static;
    wall.set_position({9, 0});
    PhysicsWorldConfig config;
    config.penetration_slop = 0;
    config.position_correction_percent = 1;
    auto strategies = make_default_collision_strategies();
    strategies.discrete_detection = std::make_unique<MarginDetection>();
    PhysicsWorld world(config, std::move(strategies));
    require(world.register_object(moving, &moving, &moving).is_valid(), "Mover registers");
    require(world.register_object(wall, &wall, &wall).is_valid(), "Wall registers");
    (void)world.advance(1.0 / 60);
    require(moving.position().nearly_equals({-2, 0}), "Solver must honor the injected contact margin, not revert to built-in geometry");
}

void queries_follow_committed_lifecycle_changes()
{
    Probe removed, survivor;
    removed.set_position({50, 0});
    survivor.set_position({100, 0});
    PhysicsWorld world;
    const auto first = world.register_object(removed, &removed, &removed);
    const auto second = world.register_object(survivor, &survivor, &survivor);
    require(first.is_valid() && second.is_valid(), "Query probes register");
    require(world.unregister_object(first), "Earlier registration removes without corrupting later index");
    require(world.teleport_object(second, {20, 0}), "Surviving handle still resolves after erase");
    std::vector<CollisionOverlapQueryHit> hits;
    world.overlap_aabb({{20, 0, 10, 10}, {}}, hits);
    require(hits.size() == 1 && hits[0].target == CollisionTarget::from_collider(survivor.collider.id), "AABB query sees teleport before another step");
    world.overlap_circle({{25, 5}, 1, {}}, hits);
    require(hits.size() == 1, "Circle query uses the same committed shapes");
    const auto ray = world.raycast({{0, 5}, {1, 0}, 100, {}});
    require(ray && ray->distance == 20, "Ray query sees committed teleport");
    const auto sweep = world.sweep_aabb({{0, 0, 10, 10}, {20, 0}, {}});
    require(sweep && sweep->fraction == 0.5f, "Sweep query sees committed teleport");
    survivor.collider.enabled = false;
    world.overlap_circle({{25, 5}, 1, {}}, hits);
    require(hits.empty() && !world.raycast({{0, 5}, {1, 0}, 100, {}}), "Queries consistently skip disabled colliders");
    world.reset();
    require(!world.contains_object(second), "Reset clears handle indexes");
    require(world.register_object(survivor, &survivor, &survivor).is_valid(), "Registration succeeds after reset");
}
}

int main()
{
    fixed_step_controls_are_frame_rate_independent();
    fixed_step_callbacks_respect_lifecycle_boundaries();
    callback_commands_preserve_order();
    removed_objects_reject_later_commands();
    ccd_events_follow_the_accepted_path();
    tile_replacement_invalidates_the_old_world();
    invalidation_ends_contacts_once();
    gameplay_end_survives_actor_unbinding();
    ccd_bounces_rebuild_candidates_and_preserve_crossed_sensors();
    ccd_budget_stops_at_a_safe_impact();
    solver_uses_the_injected_detection_contract();
    queries_follow_committed_lifecycle_changes();
    std::cout << "physics regression tests passed\n";
}
