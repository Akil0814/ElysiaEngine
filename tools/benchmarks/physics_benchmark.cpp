#include "engine/core/game_object.h"
#include "engine/physics/physics_world.h"
#include <algorithm>
#include <chrono>
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
        collider.shape = AabbShape{{0, 0, 10, 10}};
        BodyDefinition definition;
        definition.mass_policy = MassPolicy::ExplicitMass;
        definition.velocity = {1, 0};
        for (int i = 0; i < count; ++i)
        {
            auto object =
                std::make_unique<elysia::core::GameObject>(elysia::core::DepthLayer::Item);
            object->set_position({float(i % 25) * 30, float(i / 25) * 30});
            world.register_object(*object, definition, {&collider, 1});
            objects.push_back(std::move(object));
        }
        std::vector<double> samples;
        for (int i = 0; i < 1200; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            world.advance(1.0 / 60);
            auto end = std::chrono::steady_clock::now();
            if (i >= 200)
                samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        }
        std::ranges::sort(samples);
        std::cout << count << " bodies median_us=" << samples[500] << " p95_us=" << samples[950]
                  << '\n';
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
            auto object =
                std::make_unique<elysia::core::GameObject>(elysia::core::DepthLayer::Item);
            object->set_position(position);
            BodyDefinition definition;
            definition.type = type;
            definition.fixed_rotation = false;
            Collider collider;
            collider.shape = AabbShape{{-size.x / 2, -size.y / 2, size.x, size.y}};
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
            world.create_distance_joint({anchor, bob, {}, {}, 50, true, 2, 0.7f});
        }
        std::vector<double> samples;
        for (int i = 0; i < 1200; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            world.advance(1.0 / 60);
            auto end = std::chrono::steady_clock::now();
            if (i >= 200)
                samples.push_back(std::chrono::duration<double, std::micro>(end - start).count());
        }
        std::ranges::sort(samples);
        std::cout << "50 rotating boxes + 5 springs sleep=" << sleeping
                  << " median_us=" << samples[500] << " p95_us=" << samples[950] << '\n';
        world.reset();
    }
}
