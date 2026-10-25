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
    if (ImGui::CollapsingHeader("Body mass properties"))
    {
        for (const auto &body : snapshot.bodies)
        {
            const auto &state = body.state;
            const auto &properties = body.mass_properties;
            const char *mode = std::holds_alternative<elysia::physics::DensityMass>(properties) ? "Density" :
                std::holds_alternative<elysia::physics::ExplicitMass>(properties) ? "Explicit mass" : "Custom mass data";
            ImGui::Text("Body %llu: %s | mass %.4g kg | inertia %.4g kg*EU^2",
                body.owner.value, mode, state.mass, state.rotational_inertia);
            ImGui::Text("Local COM %.3f, %.3f | world COM %.3f, %.3f",
                state.local_center.x, state.local_center.y, state.world_center.x, state.world_center.y);
        }
    }
    if (ImGui::CollapsingHeader("Joint state", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const auto& joint:snapshot.joints)
        {
            const auto* state=&joint.state;
            ImGui::Text("Joint %llu: %llu / %llu",joint.handle.value,state->first.value,state->second.value);
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
                if constexpr(requires {value.settings;})
                {
                    const auto& s=value.settings;
                    ImGui::Text("Collide connected: %s",s.collide_connected?"yes":"no");
                    if constexpr(requires {s.enable_spring;})ImGui::Text("Spring: %s",s.enable_spring?"on":"off");
                    if constexpr(requires {s.frequency_hz;})ImGui::Text("Frequency %.2f Hz | damping %.2f",s.frequency_hz,s.damping_ratio);
                    if constexpr(requires {s.enable_limit;})ImGui::Text("Limits: %s",s.enable_limit?"on":"off");
                    if constexpr(requires {s.enable_motor;})ImGui::Text("Motor: %s | speed %.2f",s.enable_motor?"on":"off",s.motor_speed);
                    if constexpr(requires {s.max_motor_force;})ImGui::Text("Max motor force %.2f",s.max_motor_force);
                    if constexpr(requires {s.max_motor_torque;})ImGui::Text("Max motor torque %.2f",s.max_motor_torque);
                    if constexpr(requires {s.length;})ImGui::Text("Rest length %.2f | range %.2f / %.2f",s.length,s.min_length,s.max_length);
                    if constexpr(requires {s.target_angle;})ImGui::Text("Target angle %.3f | range %.3f / %.3f",s.target_angle,s.lower_angle,s.upper_angle);
                    if constexpr(requires {s.lower_translation;})ImGui::Text("Travel range %.2f / %.2f",s.lower_translation,s.upper_translation);
                    if constexpr(requires {s.target_translation;})ImGui::Text("Target translation %.2f",s.target_translation);
                    if constexpr(requires {s.linear_frequency_hz;})ImGui::Text("Linear %.2f Hz / damping %.2f | angular %.2f Hz / damping %.2f",s.linear_frequency_hz,s.linear_damping_ratio,s.angular_frequency_hz,s.angular_damping_ratio);
                    if constexpr(requires {s.max_force;})ImGui::Text("Max force %.2f",s.max_force);
                    if constexpr(requires {s.linear_offset;})ImGui::Text("Offset %.2f, %.2f / %.3f | max torque %.2f | correction %.2f",s.linear_offset.x,s.linear_offset.y,s.angular_offset,s.max_torque,s.correction_factor);
                }
            },state->details);
            ImGui::Text("Reaction %.2f, %.2f | torque %.2f",state->reaction_force.x,state->reaction_force.y,state->reaction_torque);
        }
    }
    if(ImGui::CollapsingHeader("Chain terrain"))
        for(const auto& chain:snapshot.chains)
        {
            ImGui::PushID(static_cast<int>(chain.handle.value));
            if(ImGui::TreeNode("Chain","Chain %llu | owner %llu | %zu edges",chain.handle.value,chain.owner.value,chain.colliders.size()))
            {
                ImGui::Text("%s | %zu actual vertices",chain.loop?"Closed":"Open",chain.local_vertices.size());
                if(chain.ghost_before)ImGui::Text("Ghost before %.2f, %.2f",chain.ghost_before->x,chain.ghost_before->y);
                if(chain.ghost_after)ImGui::Text("Ghost after %.2f, %.2f",chain.ghost_after->x,chain.ghost_after->y);
                for(std::size_t n=0;n<chain.colliders.size();++n)ImGui::Text("Edge %zu: Collider %llu",n,chain.colliders[n]);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    if(ImGui::CollapsingHeader("Kinematic targets"))
        for(const auto& target:snapshot.targets)
        {
            ImGui::Text("Owner %llu | %s | dt %.6f",target.owner.value,target.pending?"Pending next step":"Last completed step",target.fixed_delta_seconds);
            ImGui::Text("At command: %.2f, %.2f / %.3f",target.pose_at_command.position.x,target.pose_at_command.position.y,target.pose_at_command.angle);
            ImGui::Text("Requested: %.2f, %.2f / %.3f",target.requested_pose.position.x,target.requested_pose.position.y,target.requested_pose.angle);
            ImGui::Text("Effective: %.2f, %.2f / %.3f",target.effective_pose.position.x,target.effective_pose.position.y,target.effective_pose.angle);
        }
    if (ImGui::CollapsingHeader("Debug Snapshot"))
    {
        ImGui::Text("Shapes: %zu", snapshot.shapes.size());
        ImGui::Text("Contacts: %zu", snapshot.contacts.size());
        ImGui::Text("Velocities: %zu", snapshot.velocities.size());
        ImGui::Text("Chains: %zu | Targets: %zu",snapshot.chains.size(),snapshot.targets.size());
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
            bool selected = (debug_draw->enabled_categories() & category) != elysia::tools::DebugDrawCategory::None;
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
        category_checkbox("Joint details (F2)",elysia::tools::DebugDrawCategory::PhysicsJointDetails);
        category_checkbox("Chain vertices / ghosts (F3)",elysia::tools::DebugDrawCategory::PhysicsChainDetails);
        category_checkbox("Kinematic targets (F4)",elysia::tools::DebugDrawCategory::PhysicsTarget);
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
