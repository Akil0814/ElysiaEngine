#pragma once
#include "../../../engine/physics/physics_world.h"
#include "../../../engine/core/render/render_command.h"
#include "../../../engine/scene/routing/scene_key.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace example::demo::physics
{
enum class ScenarioCategory { Physics, Combat, Stress };
enum class ScenarioMode { Verify, FreePlay };
enum class ScenarioStatus { Ready, Running, Passed, Failed, FreePlay };
struct ScenarioDescriptor
{
    std::string_view id;
    ScenarioCategory category;
    elysia::scene::SceneKey scene;
    std::uint32_t max_steps;
    std::string title_key() const;
    std::string purpose_key() const;
    std::string expected_key() const;
};
struct ScenarioCheck
{
    std::string id;
    bool passed;
    double actual, expected, tolerance;
};
struct ScenarioResult
{
    ScenarioStatus status = ScenarioStatus::Ready;
    std::uint32_t steps = 0;
    std::vector<ScenarioCheck> checks;
    std::string failure;
    double median_ms = 0, p95_ms = 0, max_ms = 0;
    std::size_t samples = 0;
    bool debug_geometry = false;
    bool mixed_debug_samples = false;
    elysia::physics::PhysicsStepStats stats{};
};
std::span<const ScenarioDescriptor> physics_scenarios();
const ScenarioDescriptor* find_physics_scenario(std::string_view id);
std::string_view default_physics_scenario(elysia::scene::SceneKey scene);
std::string scenario_status_key(ScenarioStatus);
void remember_scenario_result(std::string_view id, int tier, const ScenarioResult& result);
const ScenarioResult* recent_scenario_result(std::string_view id, int tier = 0);

// Owns the exact world, actors, scripts and checks used both by the UI and CTest.
// No renderer, scene manager, wall clock or random device is needed to execute a case.
class PhysicsScenario final
{
public:
    explicit PhysicsScenario(std::string_view id, int tier = 0);
    ~PhysicsScenario();
    PhysicsScenario(const PhysicsScenario&) = delete;
    PhysicsScenario& operator=(const PhysicsScenario&) = delete;
    void start();
    void advance(double display_delta);
    void single_step();
    void set_paused(bool paused);
    bool paused() const;
    void set_debug_geometry(bool enabled);
    void finish_timeout();
    const ScenarioDescriptor& descriptor() const;
    const ScenarioResult& result() const;
    const elysia::physics::PhysicsWorld& world() const;
    elysia::core::Rect bounds() const;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out) const;
private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
}
