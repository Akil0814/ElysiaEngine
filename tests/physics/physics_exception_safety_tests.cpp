#define SDL_MAIN_HANDLED
#include "engine/physics/physics_world.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/tools/debug_draw.h"
#include "tests/support/test_assertions.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <functional>

namespace
{
thread_local long fail_after = -1;
}
void* operator new(std::size_t size)
{
    if (fail_after >= 0 && fail_after-- == 0)
    {
        fail_after = -1;
        throw std::bad_alloc{};
    }
    if (auto* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

class OwnedBody final : public elysia::core::GameObject, public elysia::physics::PhysicsParticipant
{
public:
    explicit OwnedBody(int& destroyed) : GameObject(elysia::core::DepthLayer::Character), destroyed(destroyed)
    { for (auto& collider : colliders) collider.shape = elysia::physics::AabbShape{{0,0,20,20}}; }
    ~OwnedBody() override { ++destroyed; }
    std::span<const elysia::physics::Collider> collider_definitions() const override { return colliders; }
private:
    int& destroyed;
    std::array<elysia::physics::Collider,2> colliders{};
};
class OwnershipScene final : public elysia::scene::Scene
{
public:
    using Scene::contains_object_address;
    OwnershipScene() : Scene(elysia::scene::SceneRuntimeFeatures{
        .fixed_step=elysia::scene::FixedStepConfig{},.physics=elysia::physics::PhysicsWorldConfig{}}) {}
    std::size_t registrations() { return physics_world().registered_object_count(); }
protected:
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};
class DeferredBody final : public elysia::core::GameObject,
                           public elysia::physics::PhysicsStepParticipant
{
public:
    DeferredBody() : GameObject(elysia::core::DepthLayer::Character)
    { collider.shape = elysia::physics::AabbShape{{0,0,20,20}}; }
    void fixed_update(double) override { if (tick) tick(); }
    std::function<void()> tick;
    elysia::physics::Collider collider;
};

int main(int argc,char** argv)
{
    std::set_terminate([] { std::fputs("terminate\n", stderr); std::_Exit(86); });
    using elysia::tests::require;
    const char* mode = argc >= 2 ? argv[1] : "shutdown";
    const long index = argc > 2 ? std::strtol(argv[2],nullptr,10) : 0;
    if (!std::strcmp(mode,"registration"))
    {
        elysia::core::GameObject owner(elysia::core::DepthLayer::Character);
        elysia::physics::PhysicsWorld world;
        std::array<elysia::physics::Collider,2> colliders{};
        for (auto& collider : colliders) collider.shape = elysia::physics::AabbShape{{0,0,20,20}};
        fail_after = index;
        bool caught = false;
        try { (void)world.register_object(owner,{},colliders); }
        catch (const std::bad_alloc&) { caught = true; }
        fail_after = -1;
        require(caught && world.registered_object_count() == 0 && world.registered_collider_count() == 0,
            "failed registration leaves no owner or collider record");
        const auto recovered = world.register_object(owner,{},colliders);
        require(recovered.is_valid() && world.registered_object_count() == 1
                && world.registered_collider_count() == colliders.size(),
            "PhysicsWorld accepts a fresh registration after rollback");
        world.step(1.0 / 60.0);
    }
    else if (!std::strcmp(mode,"ownership"))
    {
        int destroyed = 0;
        OwnershipScene scene;
        auto object = std::make_unique<OwnedBody>(destroyed);
        auto* address = object.get();
        fail_after = index;
        bool caught = false;
        try { (void)scene.add_object(std::move(object)); }
        catch (const std::exception&) { caught = true; }
        fail_after = -1;
        require(caught && destroyed == 1 && !scene.contains_object_address(address)
                && scene.registrations() == 0,
            "failed Scene registration releases object and all PhysicsWorld references");
    }
    else if (!std::strcmp(mode,"debug"))
    {
        elysia::core::GameObject owner(elysia::core::DepthLayer::Character);
        elysia::physics::PhysicsWorld world;
        std::array<elysia::physics::Collider,1> colliders{};
        colliders[0].shape = elysia::physics::AabbShape{{0,0,20,20}};
        (void)world.register_object(owner,{},colliders);
        fail_after = 0;
        bool caught = false;
        try { world.set_debug_capture(elysia::physics::PhysicsDebugCapture::Shapes); }
        catch (const std::bad_alloc&) { caught = true; }
        fail_after = -1;
        require(caught && world.debug_capture() == elysia::physics::PhysicsDebugCapture::None
                && world.debug_snapshot().shapes.empty(),"failed capture keeps prior mode and snapshot");
        world.set_debug_capture(elysia::physics::PhysicsDebugCapture::Shapes);
        require(world.debug_snapshot().shapes.size() == 1,"capture succeeds after transient allocation failure");
    }
    else if (!std::strcmp(mode,"deferred") || !std::strcmp(mode,"deferred_removed"))
    {
        const bool remove_source = !std::strcmp(mode,"deferred_removed");
        elysia::physics::PhysicsWorld world;
        DeferredBody source, spawned;
        const auto source_handle = world.register_object(source,{}, {&source.collider,1});
        elysia::physics::PhysicsObjectHandle created;
        source.tick = [&] {
            created = world.register_object(spawned,{}, {&spawned.collider,1});
            if (remove_source) require(world.unregister_object(source_handle),
                "source unregister queues after deferred creation");
            fail_after = index;
        };
        bool caught = false;
        try { world.step(1.0 / 60.0); }
        catch (const std::bad_alloc&) { caught = true; }
        fail_after = -1;
        source.tick = {};
        require(caught && created.is_valid() && world.contains_object(source_handle) == !remove_source
                && !world.contains_object(created) && world.registered_object_count() == (remove_source ? 0u : 1u),
            "deferred native registration failure removes pending and abandoned owners");
        world.step(1.0 / 60.0);
    }
    else if (!std::strcmp(mode,"shutdown"))
    {
        elysia::io::ContentRegistry registry;
        elysia::scene::SceneRuntimeContext context(nullptr,registry,1280,720);
        elysia::scene::SceneManager manager;
        manager.initialize(context);
        elysia::tools::DebugDraw::instance()->clear();
        fail_after = 0;
        const bool closed = manager.shutdown();
        fail_after = -1;
        require(closed && manager.shutdown(),"shutdown is nonallocating and idempotent");
        manager.initialize(context);
        require(manager.local_players().contains(elysia::input::PrimaryLocalPlayer)
                && manager.shutdown(),"reinitialize restores default local player");
    }
    else require(false,"unknown exception safety mode");
}
