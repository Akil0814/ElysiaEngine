#pragma once
#include "physics_scenario.h"
#include "block_actor.h"
#include "demo_tile_map.h"
#include "../../input/gameplay_input_map.h"
#include <functional>
#include <stdexcept>

namespace example::demo::physics
{
using namespace elysia::physics;
using elysia::core::Vector2;
class ScenarioBody final : public elysia::core::GameObject,
                           public PhysicsParticipant, public PhysicsStepParticipant
{
public:
    ScenarioBody(Vector2 position, BodyDefinition definition, std::vector<Collider> shapes);
    BodyDefinition body_definition() const override { return definition; }
    std::span<const Collider> collider_definitions() const override { return colliders; }
    void fixed_update(double) override { if (tick) tick(); }
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const override;
    BodyDefinition definition;
    std::vector<Collider> colliders;
    std::function<void()> tick;
};
struct PhysicsScenario::Impl : ICollisionListener
{
    const ScenarioDescriptor& descriptor;
    int tier;
    PhysicsWorld world;
    elysia::gameplay::collision::GameplayCollisionRuntime runtime;
    DemoCombatSession combat;
    std::vector<std::unique_ptr<elysia::core::GameObject>> objects;
    std::vector<PhysicsObjectHandle> handles;
    std::vector<JointHandle> joints;
    DemoTileMap* tiles = nullptr;
    ScenarioResult result;
    std::vector<CollisionEvent> events;
    std::vector<double> timings;
    std::function<void(unsigned)> before, after;
    elysia::core::Rect view{0, 0, 1000, 600};
    double accumulator = 0;
    bool paused = false;
    unsigned finish_at = 120;
    std::size_t expected_objects = 0;
    explicit Impl(const ScenarioDescriptor&, int);
    ~Impl();
    template<class T, class... Args> T& add(Args&&... args)
    {
        auto object = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *object;
        if (auto* p = dynamic_cast<PhysicsParticipant*>(object.get()))
        {
            auto h = world.register_object(ref, p->body_definition(), p->collider_definitions());
            if (!h.is_valid()) throw std::runtime_error("Scenario registration failed");
            p->bind_physics(world, h);
            handles.push_back(h);
        }
        objects.push_back(std::move(object));
        return ref;
    }
    ScenarioBody& body(Vector2 position, Vector2 size = {20,20}, BodyType type = BodyType::Dynamic,
                       bool gravity = false, bool circle = false);
    DemoTileMap& tile_map(int columns, int rows, Vector2 size = {32,32},
                         Vector2 origin = {0,0}, TileOutOfBoundsPolicy policy = TileOutOfBoundsPolicy::Block);
    void check(std::string id, double actual, double expected, double tolerance = 0);
    void truth(std::string id, bool passed) { check(std::move(id), passed ? 1 : 0, 1); }
    void step();
    void complete();
    void build();
    void build_motion();
    void build_materials();
    void build_ccd();
    void build_filters();
    void build_queries();
    void build_one_way();
    void build_tiles();
    void build_joints();
    void build_lifecycle();
    void build_timing();
    void build_combat();
    void build_stress();
    void on_collision_event(const CollisionEvent& e) override { events.push_back(e); }
};
}
