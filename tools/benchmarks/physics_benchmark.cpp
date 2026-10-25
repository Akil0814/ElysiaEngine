#include "engine/core/game_object.h"
#include "engine/physics/physics_world.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
int main()
{
    using namespace elysia::physics;
    for (int count : {100, 500})
    {
        std::vector<std::unique_ptr<elysia::core::GameObject>> objects;
        PhysicsWorldConfig config;
        config.enable_sleep = false;
        PhysicsWorld world(config);
        Collider collider;
        collider.shape = BoxShape::from_rect({0, 0, 10, 10});
        BodyDefinition definition;
        definition.mass_properties = ExplicitMass{1};
        definition.velocity = {1, 0};
        for (int i = 0; i < count; ++i)
        {
            auto object = std::make_unique<elysia::core::GameObject>(elysia::core::DepthLayer::Item);
            object->set_position({float(i % 25) * 30, float(i / 25) * 30});
            world.register_object(*object, definition, {&collider, 1});
            objects.push_back(std::move(object));
        }
        std::vector<double> samples;
        for (int i = 0; i < 1200; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            world.step(1.0 / 60);
            world.finalize_frame(0);
            auto end = std::chrono::steady_clock::now();
            if (i >= 200)
                samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        }
        std::ranges::sort(samples);
        std::cout << count << " bodies median_us=" << samples[500] << " p95_us=" << samples[950] << '\n';
        world.reset();
    }
    // New capabilities are measured separately, without claiming a native baseline.
    for (bool sleeping : {false, true})
    {
        std::vector<std::unique_ptr<elysia::core::GameObject>> objects;
        PhysicsWorldConfig config;
        config.gravity = {0, 980};
        config.enable_sleep = sleeping;
        PhysicsWorld world(config);
        auto add = [&](elysia::core::Vector2 position, elysia::core::Vector2 size, BodyType type) {
            auto object = std::make_unique<elysia::core::GameObject>(elysia::core::DepthLayer::Item);
            object->set_position(position);
            BodyDefinition definition;
            definition.type = type;
            definition.fixed_rotation = false;
            Collider collider;
            collider.shape = BoxShape::from_rect({-size.x / 2, -size.y / 2, size.x, size.y});
            auto handle = world.register_object(*object, definition, {&collider, 1});
            objects.push_back(std::move(object));
            return handle;
        };
        add({250, 500}, {600, 20}, BodyType::Static);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 5; ++x)
                add({100.f + x * 25, 478.f - y * 25}, {24, 24}, BodyType::Dynamic);
        for (int i = 0; i < 5; ++i)
        {
            auto anchor = add({350.f + i * 35, 300}, {5, 5}, BodyType::Static);
            auto bob = add({350.f + i * 35, 350}, {20, 20}, BodyType::Dynamic);
            world.create_distance_joint(
                {anchor, bob, {}, {}, {.length = 50, .enable_spring = true, .frequency_hz = 2, .damping_ratio = 0.7f}});
        }
        std::vector<double> samples;
        for (int i = 0; i < 1200; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            world.step(1.0 / 60);
            world.finalize_frame(0);
            auto end = std::chrono::steady_clock::now();
            if (i >= 200)
                samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        }
        std::ranges::sort(samples);
        std::cout << "50 rotating boxes + 5 springs sleep=" << sleeping << " median_us=" << samples[500]
                  << " p95_us=" << samples[950] << '\n';
        world.reset();
    }
    {
        std::vector<std::unique_ptr<elysia::core::GameObject>> owners;
        PhysicsWorld world({.gravity = {0, 200}, .enable_sleep = false});
        auto add = [&](elysia::core::Vector2 position, BodyType type) {
            auto owner = std::make_unique<elysia::core::GameObject>(elysia::core::DepthLayer::Item);
            owner->set_position(position);
            BodyDefinition body;
            body.type = type;
            body.fixed_rotation = false;
            body.gravity_scale = 0;
            if (type == BodyType::Dynamic) body.mass_properties = ExplicitMass{1};
            Collider shape;
            shape.shape = CapsuleShape{{0, -5}, {0, 5}, 5};
            auto h = world.register_object(*owner, body, {&shape, 1});
            owners.push_back(std::move(owner));
            return h;
        };
        auto terrain = add({0, 500}, BodyType::Static);
        ChainDefinition chain;
        for (int i = 0; i <= 100; ++i)
            chain.local_vertices.push_back({float(i * 20), float(i % 2) * 5});
        world.create_chain(terrain, chain);
        auto platform = add({900, 400}, BodyType::Kinematic);
        for (int i = 0; i < 24; ++i)
        {
            auto a = add({float(i * 60), 100}, BodyType::Static), b = add({float(i * 60), 150}, BodyType::Dynamic);
            switch (i % 6)
            {
            case 0:
                world.create_weld_joint({a, b, {0, 50}});
                break;
            case 1:
                world.create_prismatic_joint({a, b});
                break;
            case 2:
                world.create_wheel_joint({a, b});
                break;
            case 3: {
                MouseJointDefinition d{a, b};
                d.settings.target = {float(i * 60), 150};
                world.create_mouse_joint(d);
                break;
            }
            case 4: {
                MotorJointDefinition d{a, b};
                d.settings.linear_offset = {0, 50};
                world.create_motor_joint(d);
                break;
            }
            case 5:
                world.create_filter_joint({a, b});
                break;
            }
        }
        std::vector<double> samples;
        for (int i = 0; i < 1200; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            world.set_target_transform(platform, {{900 + 50 * std::sin(i / 60.f), 400}, .1f * std::sin(i / 60.f)},
                                       1. / 60);
            for (int q = 0; q < 8; ++q)
                (void)world.shape_cast({CapsuleShape{{0, -5}, {0, 5}, 5}, {{float(q * 150), 350}, 0}, {0, 200}});
            world.step(1. / 60);
            world.finalize_frame(0);
            if (i >= 200)
                samples.push_back(
                    std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count());
        }
        std::ranges::sort(samples);
        std::cout << "100 chain edges + target + 24 mixed joints + 8 capsule casts median_us=" << samples[500]
                  << " p95_us=" << samples[950] << '\n';
        world.reset();
    }
}
