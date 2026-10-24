#include "game/showcase/shared/physics_inspector.h"
#include "engine/tools/debug_draw.h"
#include <type_traits>
#if ELYSIA_ENABLE_IMGUI
#include <imgui.h>
#endif
namespace example::showcase {
void PhysicsInspector::attach(elysia::tools::IDevelopmentPanelRegistry* registry,std::string name,
    std::function<const elysia::physics::PhysicsWorld&()> world,std::function<elysia::scene::FixedStepStats()> stats,
    std::optional<elysia::scene::FixedStepConfig> config) {
    detach();_name=std::move(name);_world=std::move(world);_stats=std::move(stats);_config=config;
#if ELYSIA_ENABLE_IMGUI
    if (!registry) return;
    _registry=registry;_handle=registry->register_panel("physics_demo.inspector",[this]{draw();});
    if (!_handle.is_valid()) _registry=nullptr;
#endif
}
void PhysicsInspector::detach() noexcept {
    if (_registry && _handle.is_valid()) (void)_registry->unregister_panel(_handle);
    _registry=nullptr;_handle={};_world={};_stats={};
}
#if ELYSIA_ENABLE_IMGUI
void PhysicsInspector::draw()
{
    if (!ImGui::Begin("Physics Inspector###physics_demo.inspector"))
    {
        ImGui::End();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextUnformatted(_name.c_str());
    ImGui::Text("Frame %.3f ms (%.1f FPS)",
        io.DeltaTime * 1000.0f, io.Framerate);

    const auto& inspected = _world();
    const auto& config = inspected.config();
    if (ImGui::CollapsingHeader(
            "World Configuration", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto* fixed = (_config ? &*_config : nullptr);
        ImGui::Text("Fixed step: %.6f s", fixed ? fixed->delta_seconds : 0.0);
        ImGui::Text("Gravity: (%.2f, %.2f)",
            config.gravity.x, config.gravity.y);
        ImGui::Text("Max catch-up steps: %u",
            fixed ? fixed->max_steps_per_frame : 0u);
        ImGui::Text("Sub-steps: %u", config.sub_steps);
    }

    const auto& stats = inspected.last_step_stats();
    if (ImGui::CollapsingHeader(
            "Last Fixed Step", ImGuiTreeNodeFlags_DefaultOpen)
        && ImGui::BeginTable("physics_step_stats", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        const auto row = [](const char* label, unsigned long long value)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%llu", value);
        };
        row("Registered objects", stats.registered_objects);
        row("Registered colliders", stats.registered_colliders);
        row("Contacts", stats.contacts);
        row("Awake bodies", stats.awake_bodies);
        row("Joints", stats.joints);
        row("Chains",stats.chains);
        ImGui::Text("Step: %.3f ms", stats.step_milliseconds);
        const auto dropped = _stats().dropped_steps;
        row("Dropped fixed steps", dropped);
        ImGui::EndTable();
    }

    const auto& snapshot = inspected.debug_snapshot();
    if (ImGui::CollapsingHeader("Joint state", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const auto& joint:snapshot.joints) if (auto state=inspected.joint_state(joint.handle))
        {
            std::visit([&](const auto& value){
                using T=std::decay_t<decltype(value)>;
                if constexpr(std::is_same_v<T,elysia::physics::DistanceJointState>)ImGui::Text("Length %.2f | speed %.2f | force %.2f",value.current_length,value.speed,value.motor_force);
                else if constexpr(std::is_same_v<T,elysia::physics::RevoluteJointState>)ImGui::Text("Angle %.3f | speed %.3f | torque %.2f",value.angle,value.angular_velocity,value.motor_torque);
                else if constexpr(std::is_same_v<T,elysia::physics::WeldJointState>)ImGui::Text("Weld error %.2f / %.3f",value.linear_error.length(),value.angular_error);
                else if constexpr(std::is_same_v<T,elysia::physics::PrismaticJointState>)ImGui::Text("Translation %.2f | speed %.2f | force %.2f",value.translation,value.speed,value.motor_force);
                else if constexpr(std::is_same_v<T,elysia::physics::WheelJointState>)ImGui::Text("Suspension %.2f / %.2f | angular %.2f | torque %.2f",value.translation,value.speed,value.angular_velocity,value.motor_torque);
                else if constexpr(std::is_same_v<T,elysia::physics::MouseJointState>)ImGui::Text("Mouse target %.1f, %.1f | error %.2f",value.settings.target.x,value.settings.target.y,value.error.length());
                else if constexpr(std::is_same_v<T,elysia::physics::MotorJointState>)ImGui::Text("Motor pose error %.2f / %.3f",value.linear_error.length(),value.angular_error);
                else ImGui::TextUnformatted("Filter: pair collision disabled (no mechanical constraint)");
            },state->details);
            ImGui::Text("Reaction %.2f, %.2f | torque %.2f",state->reaction_force.x,state->reaction_force.y,state->reaction_torque);
        }
    }
    if (ImGui::CollapsingHeader("Debug Snapshot"))
    {
        ImGui::Text("Shapes: %zu", snapshot.shapes.size());
        ImGui::Text("Contacts: %zu", snapshot.contacts.size());
        ImGui::Text("Velocities: %zu", snapshot.velocities.size());
    }

    if (ImGui::CollapsingHeader(
            "Debug Draw", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto* debug_draw = elysia::tools::DebugDraw::instance();
        bool enabled = debug_draw->enabled();
        if (ImGui::Checkbox("Enabled", &enabled))
            debug_draw->set_enabled(enabled);

        const auto category_checkbox = [debug_draw](
            const char* label,
            elysia::tools::DebugDrawCategory category)
        {
            bool selected = debug_draw->is_enabled(category);
            if (!ImGui::Checkbox(label, &selected))
                return;
            auto categories = debug_draw->enabled_categories();
            const auto bits = static_cast<std::uint32_t>(categories);
            const auto category_bits = static_cast<std::uint32_t>(category);
            categories = static_cast<elysia::tools::DebugDrawCategory>(
                selected ? bits | category_bits : bits & ~category_bits);
            debug_draw->set_enabled_categories(categories);
        };
        category_checkbox("Collider (render pose)",
            elysia::tools::DebugDrawCategory::PhysicsCollider);
        category_checkbox("Contact",
            elysia::tools::DebugDrawCategory::PhysicsContact);
        category_checkbox("Contact normal",
            elysia::tools::DebugDrawCategory::PhysicsContactNormal);
        category_checkbox("Native AABB (physics step)",
            elysia::tools::DebugDrawCategory::PhysicsBroadPhase);
        category_checkbox("Previous / current physics poses",
            elysia::tools::DebugDrawCategory::PhysicsPoseHistory);
        category_checkbox("Velocity",
            elysia::tools::DebugDrawCategory::PhysicsVelocity);
        category_checkbox("Joint anchors",
            elysia::tools::DebugDrawCategory::PhysicsJoint);
        ImGui::TextWrapped("Green: awake | Blue: asleep/static | Purple: sensor. Contacts and AABBs show the latest physics step.");
        category_checkbox("Gameplay",
            elysia::tools::DebugDrawCategory::Gameplay);
    }

    ImGui::End();
}
#else
void PhysicsInspector::draw() {}
#endif
}
