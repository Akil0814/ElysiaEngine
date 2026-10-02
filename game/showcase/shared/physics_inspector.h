#pragma once
#include "engine/tools/development_overlay.h"
#include "engine/physics/physics_world.h"
#include "engine/scene/runtime/fixed_step_runtime.h"
#include <functional>
#include <string>
namespace example::showcase
{
class PhysicsInspector final
{
public:
    ~PhysicsInspector() { detach(); }
    void attach(elysia::tools::IDevelopmentPanelRegistry*,std::string,
        std::function<const elysia::physics::PhysicsWorld&()>,std::function<elysia::scene::FixedStepStats()>,
        std::optional<elysia::scene::FixedStepConfig> config);
    void detach() noexcept;
private:
    void draw();
    elysia::tools::IDevelopmentPanelRegistry* _registry=nullptr;
    elysia::tools::DevelopmentPanelHandle _handle{};
    std::string _name;
    std::function<const elysia::physics::PhysicsWorld&()> _world;
    std::function<elysia::scene::FixedStepStats()> _stats;
    std::optional<elysia::scene::FixedStepConfig> _config;
};
}
