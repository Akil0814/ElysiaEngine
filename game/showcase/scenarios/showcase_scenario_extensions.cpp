#include "game/showcase/shared/collider_render.h"
#include "showcase_scenario_internal.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <type_traits>
namespace example::showcase::scenarios
{
void ShowcaseScenario::Impl::build_edges()
{
    finish_at = 180;
    BodyDefinition fixed;
    fixed.type = BodyType::Static;
    Collider edge;
    edge.material.friction = 0;
    edge.shape = SegmentShape{{60, 220}, {260, 220}};
    add<ScenarioBody>(Vector2{}, fixed, std::vector<Collider>{edge});
    edge.shape = SegmentShape{{260, 220}, {400, 250}};
    add<ScenarioBody>(Vector2{}, fixed, std::vector<Collider>{edge});
    auto owner = add<ScenarioBody>(Vector2{}, fixed, std::vector<Collider>{}).physics_handle();
    ChainDefinition d;
    d.local_vertices = {{60, 440}, {260, 440}, {400, 470}};
    d.material.friction = 0;
    auto chain = world.create_chain(owner, d);
    for (size_t i = 1; i < d.local_vertices.size(); ++i)
        query_commands.push_back(elysia::core::make_world_draw_line_command(d.local_vertices[i - 1],
                                                                            d.local_vertices[i], {100, 160, 200}, 2));
    d.loop = true;
    d.local_vertices = {{620, 180}, {900, 180}, {900, 450}, {620, 450}};
    world.create_chain(owner, d);
    for (size_t i = 0; i < d.local_vertices.size(); ++i)
        query_commands.push_back(elysia::core::make_world_draw_line_command(
            d.local_vertices[i], d.local_vertices[(i + 1) % d.local_vertices.size()], {100, 160, 200}, 2));
    auto first = body({100, 150}, {24, 24}, BodyType::Dynamic, true, true).physics_handle();
    auto second = body({100, 370}, {24, 24}, BodyType::Dynamic, true, true).physics_handle();
    world.set_velocity(first, {100, 0});
    world.set_velocity(second, {100, 0});
    after = [=, this](unsigned n) {
        if (n == 1)
        {
            truth("chain.front_ray", world.segment_cast({{180, 380}, {180, 480}}).has_value());
            truth("chain.back_ray_misses", !world.segment_cast({{180, 480}, {180, 450}}));
            truth("chain.back_shape_cast_is_geometric",
                  world.shape_cast({CircleShape{{}, 10}, {{180, 480}, 0}, {0, -60}}).has_value());
            truth("chain.edge_identity", world.chain_segment_info(world.chain_collider(chain, 1)).has_value());
        }
        if (n == 180)
        {
            truth("chain.slides_across_join", world.body_state(second)->position.x > 270);
            truth("segment.slide_finite", std::isfinite(world.body_state(first)->position.x));
        }
    };
}
void ShowcaseScenario::Impl::build_target_pose()
{
    finish_at = 180;
    BodyDefinition d;
    d.type = BodyType::Kinematic;
    d.fixed_rotation = false;
    Collider c;
    c.shape = BoxShape{{}, {110, 12}, 0};
    auto platform = add<ScenarioBody>(Vector2{450, 370}, d, std::vector<Collider>{c}).physics_handle();
    body({430, 290}, {26, 26}, BodyType::Dynamic, true, true);
    before = [=, this](unsigned n) {
        float t = n / 60.f;
        world.set_target_transform(platform, {{450 + 80 * std::sin(t), 370 + 20 * std::sin(t * 2)}, .15f * std::sin(t)},
                                   1. / 60);
    };
    after = [=, this](unsigned n) {
        if (n == 180)
        {
            auto state = world.body_state(platform);
            check("target.platform_position", state->position.x, 450 + 80 * std::sin(3.f), .5);
            check("target.platform_angle", state->angle, .15f * std::sin(3.f), .01);
            truth("target.no_persistent_controller", state->velocity.length() > 0);
        }
    };
}
void ShowcaseScenario::Impl::build_joint_extensions()
{
    finish_at = 180;
    BodyDefinition anchor;
    anchor.type = BodyType::Static;
    anchor.fixed_rotation = false;
    joint_anchor = add<ScenarioBody>(Vector2{450, 260}, anchor, std::vector<Collider>{}).physics_handle();
    BodyDefinition dynamic;
    dynamic.fixed_rotation = false;
    dynamic.gravity_scale = 0;
    dynamic.mass_policy = MassPolicy::ExplicitMass;
    dynamic.mass = 1;
    Collider c;
    c.shape = BoxShape{{}, {30, 20}, 0};
    joint_body = add<ScenarioBody>(Vector2{450, 260}, dynamic, std::vector<Collider>{c}).physics_handle();
    auto a = joint_anchor, b = joint_body;
    const auto id = descriptor.id;
    if (id == "joint_weld")
    {
        WeldJointDefinition d{a, b};
        joints.push_back(world.create_weld_joint(d));
        world.set_velocity(b, {100, 30});
        world.set_angular_velocity(b, 2);
    }
    if (id == "joint_prismatic")
    {
        PrismaticJointDefinition d{a, b};
        d.settings.enable_motor = true;
        d.settings.motor_speed = 50;
        d.settings.max_motor_force = 10000;
        d.settings.enable_limit = true;
        d.settings.lower_translation = -120;
        d.settings.upper_translation = 120;
        joints.push_back(world.create_prismatic_joint(d));
    }
    if (id == "joint_wheel")
    {
        WheelJointDefinition d{a, b};
        d.settings.enable_motor = true;
        d.settings.motor_speed = 2;
        d.settings.max_motor_torque = 30000;
        d.settings.enable_limit = true;
        d.settings.lower_translation = -70;
        d.settings.upper_translation = 70;
        joints.push_back(world.create_wheel_joint(d));
        world.apply_impulse(b, {0, 120});
    }
    if (id == "joint_mouse")
    {
        MouseJointDefinition d{a, b};
        d.settings.target = {450, 260};
        d.settings.max_force = 10000;
        joints.push_back(world.create_mouse_joint(d));
    }
    if (id == "joint_motor")
    {
        MotorJointDefinition d{a, b};
        d.settings.linear_offset = {100, 0};
        d.settings.angular_offset = .4f;
        d.settings.max_force = 100000;
        d.settings.max_torque = 100000;
        d.settings.correction_factor = .05f;
        joints.push_back(world.create_motor_joint(d));
    }
    if (id == "joint_filter")
    {
        // The pair occupies the same space; a Filter suppresses response without fixing their poses.
        Collider terrain;
        terrain.shape = BoxShape{{}, {80, 20}, 0};
        BodyDefinition s;
        s.type = BodyType::Static;
        joint_anchor = add<ScenarioBody>(Vector2{400, 260}, s, std::vector<Collider>{terrain}).physics_handle();
        a = joint_anchor;
        joints.push_back(world.create_filter_joint({a, b}));
    }
    before = [=, this](unsigned n) {
        if (manual_joint_control)
            return;
        if (id == "joint_mouse")
        {
            auto s = std::get<MouseJointState>(world.joint_state(joints[0])->details).settings;
            s.target = {450 + 70 * std::sin(n / 60.f), 260 + 40 * std::cos(n / 60.f)};
            world.update_mouse_joint(joints[0], s);
        }
        if (n == 90)
            toggle_extension_joint();
    };
    after = [=, this](unsigned n) {
        if (manual_joint_control || n != 180)
            return;
        auto state = world.body_state(b);
        truth("joint.finite_pose", std::isfinite(state->position.x) && std::isfinite(state->angle));
        if (id == "joint_filter")
            truth("filter.destroy_restores_contact", std::ranges::any_of(events, [](const auto &event) {
                      return event.phase == CollisionEventPhase::Begin;
                  }));
        else
        {
            truth("joint.typed_state", world.joint_state(joints[0]).has_value());
            auto j = world.joint_state(joints[0]);
            if (id == "joint_prismatic")
                truth("prismatic.reversed", std::get<PrismaticJointState>(j->details).speed < 0);
            if (id == "joint_wheel")
                truth("wheel.reversed", std::get<WheelJointState>(j->details).angular_velocity < 0);
            if (id == "joint_motor")
                truth("motor.runtime_target", std::get<MotorJointState>(j->details).linear_error.length() < 3);
            if (id == "joint_weld")
                truth("weld.attached", std::get<WeldJointState>(j->details).linear_error.length() < 3);
        }
    };
}
void ShowcaseScenario::Impl::toggle_extension_joint()
{
    extension_toggle = !extension_toggle;
    const auto h = joints.front();
    auto state = world.joint_state(h);
    if (descriptor.id == "joint_filter")
    {
        if (state)
            world.destroy_joint(h);
        else
            joints.front() = world.create_filter_joint({joint_anchor, joint_body});
        world.set_transform(joint_body, {{450, 260}, 0}, TeleportVelocityMode::Clear);
        return;
    }
    if (!state)
        return;
    std::visit(
        [&](const auto &detail) {
            using T = std::decay_t<decltype(detail)>;
            if constexpr (std::is_same_v<T, WeldJointState>)
            {
                auto s = detail.settings;
                s.linear_frequency_hz = extension_toggle ? 2.f : 0.f;
                s.angular_frequency_hz = s.linear_frequency_hz;
                world.update_weld_joint(h, s);
                world.apply_impulse(joint_body, {80, 20});
            }
            else if constexpr (std::is_same_v<T, PrismaticJointState>)
            {
                auto s = detail.settings;
                s.motor_speed = -s.motor_speed;
                world.update_prismatic_joint(h, s);
            }
            else if constexpr (std::is_same_v<T, WheelJointState>)
            {
                auto s = detail.settings;
                s.motor_speed = -s.motor_speed;
                world.update_wheel_joint(h, s);
            }
            else if constexpr (std::is_same_v<T, MouseJointState>)
            {
                auto s = detail.settings;
                s.target = extension_toggle ? Vector2{550, 300} : Vector2{350, 220};
                world.update_mouse_joint(h, s);
            }
            else if constexpr (std::is_same_v<T, MotorJointState>)
            {
                auto s = detail.settings;
                s.linear_offset.x = -s.linear_offset.x;
                s.angular_offset = -s.angular_offset;
                world.update_motor_joint(h, s);
            }
        },
        state->details);
}
void ShowcaseScenario::drag_joint_target(Vector2 point)
{
    if (_impl->descriptor.id != "joint_mouse" || !std::isfinite(point.x) || !std::isfinite(point.y))
        return;
    if (!_impl->manual_joint_control)
    {
        if (_impl->result.status == ScenarioStatus::Passed || _impl->result.status == ScenarioStatus::Failed)
            _impl = std::make_unique<Impl>(_impl->descriptor, _impl->tier);
        _impl->manual_joint_control = true;
    }
    auto &p = *_impl;
    p.paused = false;
    p.result.status = ScenarioStatus::Running;
    auto h = p.joints.front();
    auto state = p.world.joint_state(h);
    auto s = std::get<MouseJointState>(state->details).settings;
    s.target = point;
    p.world.update_mouse_joint(h, s);
}
std::string ShowcaseScenario::joint_status() const
{
    if (_impl->joints.empty())
        return {};
    auto state = _impl->world.joint_state(_impl->joints.front());
    if (!state)
        return "Filter: collision restored";
    std::ostringstream text;
    text << std::fixed << std::setprecision(1);
    std::visit(
        [&](const auto &d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, DistanceJointState>)
                text << "Length " << d.current_length << " | F " << d.motor_force;
            else if constexpr (std::is_same_v<T, RevoluteJointState>)
                text << "Angle " << d.angle << " | torque " << d.motor_torque;
            else if constexpr (std::is_same_v<T, WeldJointState>)
                text << "Weld error " << d.linear_error.length() << " / " << d.angular_error;
            else if constexpr (std::is_same_v<T, PrismaticJointState>)
                text << "Travel " << d.translation << " | speed " << d.speed << " | F " << d.motor_force;
            else if constexpr (std::is_same_v<T, WheelJointState>)
                text << "Travel " << d.translation << " | omega " << d.angular_velocity << " | T " << d.motor_torque;
            else if constexpr (std::is_same_v<T, MouseJointState>)
                text << "Target " << d.settings.target.x << ", " << d.settings.target.y << " | error "
                     << d.error.length();
            else if constexpr (std::is_same_v<T, MotorJointState>)
                text << "Pose error " << d.linear_error.length() << " / " << d.angular_error;
            else
                text << "Filter: pair collision disabled";
        },
        state->details);
    return text.str();
}
} // namespace example::showcase::scenarios
