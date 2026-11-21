#pragma once

#include "engine/physics/contracts/collision_listener.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <utility>

namespace elysia::tests
{
class CollisionEffectScene final : public scene::Scene, private physics::ICollisionListener
{
public:
    CollisionEffectScene()
        : Scene({.fixed_step = scene::FixedStepConfig{}, .physics = physics::PhysicsWorldConfig{}})
    {
        instance = this;
        require(physics_world().add_listener(*this), "register collision listener");
    }
    ~CollisionEffectScene() override
    {
        (void)physics_world().remove_listener(*this);
        instance = nullptr;
    }
    inline static CollisionEffectScene* instance = nullptr;

    void verify_collision_spawn(scene::SceneManager& manager, std::function<void()> request)
    {
        create_and_add_object<Body>();
        create_and_add_object<Body>();
        const auto before = registrations;
        collision = [&] {
            request();
            require(registrations == before, "world effect must remain pending during contact");
        };
        manager.on_update(1.0 / 60.0);
        require(!collision && registrations == before + 1, "collision effect must register in the same frame");
    }

private:
    class Body final : public core::GameObject, public physics::PhysicsParticipant
    {
    public:
        Body() : GameObject(core::DepthLayer::Item)
        {
            collider.shape = physics::BoxShape::from_rect({0, 0, 10, 10});
            collider.response = physics::CollisionResponse::Overlap;
        }
        physics::BodyDefinition body_definition() const override { return {}; }
        std::span<const physics::Collider> collider_definitions() const override { return {&collider, 1}; }
        physics::Collider collider;
    };
    void on_collision_event(const physics::CollisionEvent&) override
    {
        if (auto callback = std::exchange(collision, {})) callback();
    }
    void on_scene_object_registered(core::SceneObject&) override { ++registrations; }
    void on_enter(const scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}

    std::function<void()> collision;
    unsigned registrations = 0;
};
}
