#include "physics_test_support.h"
#include "engine/physics/physics_debug_draw.h"
#include "engine/tools/debug_draw.h"
#include <array>
#include <iostream>
int main()
{
    std::array<float, 5> reference{};
    for (float scale : {50.f, 100.f, 200.f})
    {
        Probe body;
        body.collider.shape = AabbShape{{-scale / 2, -scale / 2, scale, scale}};
        body.collider.density = {2};
        body.definition.fixed_rotation = false;
        PhysicsWorldConfig c;
        c.units_per_meter = scale;
        PhysicsWorld world(c);
        auto h = body.add(world);
        auto state = world.body_state(h);
        require(state && near(state->mass, 2),
                "Density uses square meters under each spatial scale");
        world.apply_force(h, {2 * scale, 0});
        world.apply_torque(h, scale * scale);
        step(world);
        state = world.body_state(h);
        require(near(state->velocity.x / scale, 1.f / 60, 0.001f), "Force conversion matches mass");
        require(near(state->angular_velocity, 0.05f, 0.001f),
                "Torque conversion uses squared scale");
        world.apply_angular_impulse(h, scale * scale / 3);
        require(near(world.body_state(h)->angular_velocity, 1.05f, 0.001f),
                "Angular impulse conversion");
        world.set_angular_velocity(h, 0);
        world.apply_impulse(h, {0, 2 * scale}, Vector2{scale, 0});
        require(world.body_state(h)->angular_velocity > 0,
                "Positive x crossed with positive y produces clockwise spin");
        world.set_transform(h, {{}, 0}, TeleportVelocityMode::Clear);
        world.set_velocity(h, {scale, 0});
        world.set_angular_velocity(h, 1);
        step(world);
        world.advance(1.0 / 120);
        auto visual = world.render_pose(h);
        state = world.body_state(h);
        require(visual &&
                    near(visual->position.x / scale, state->position.x / scale * 0.5f, 0.001f),
                "Position interpolation");
        require(near(visual->angle, state->angle * 0.5f, 0.001f), "Angle interpolation");
        world.set_debug_capture(PhysicsDebugCapture::Shapes);
        auto *draw = elysia::tools::DebugDraw::instance();
        draw->set_enabled(true);
        draw->set_enabled_categories(elysia::tools::DebugDrawCategory::PhysicsCollider);
        draw->clear();
        submit_physics_debug_snapshot(world.debug_snapshot(), *draw);
        require(draw->commands().size() == 4, "Rotated box draws four actual edges");
        for (const auto &command : draw->commands())
        {
            const auto &edge = std::get<elysia::tools::DebugDrawLine>(command.primitive);
            auto offset = edge.start - visual->position;
            const float x = std::cos(visual->angle) * offset.x + std::sin(visual->angle) * offset.y;
            const float y = -std::sin(visual->angle) * offset.x + std::cos(visual->angle) * offset.y;
            require(near(std::abs(x), scale / 2, 0.001f) && near(std::abs(y), scale / 2, 0.001f),
                    "Debug vertices follow rigid render interpolation at every scale");
            require(near((edge.end - edge.start).length(), scale, 0.001f),
                    "Interpolating rotation preserves collider edge length");
        }
        draw->set_enabled(false);
        world.set_debug_capture(PhysicsDebugCapture::None);
        Probe fixed;
        fixed.definition.type = BodyType::Static;
        auto anchor = fixed.add(world);
        world.set_transform(h, {{scale, 0}, 0}, TeleportVelocityMode::Clear);
        auto spring = world.create_distance_joint({anchor, h, {}, {}, scale, true, 2, 0.7f});
        world.apply_impulse(h, {scale, 0});
        step(world, 20);
        auto joint = world.joint_state(spring);
        state = world.body_state(h);
        std::array<float, 5> values{state->position.x / scale, state->velocity.x / scale,
                                    state->mass, state->rotational_inertia / (scale * scale),
                                    joint->reaction_force.x / scale};
        if (scale == 50)
            reference = values;
        else
            for (int i = 0; i < 5; ++i)
                require(near(values[i], reference[i], 0.005f),
                        "Equivalent physical worlds produce equivalent normalized state and joint "
                        "force");
    }
    std::cout << "scale invariance tests passed\n";
}
