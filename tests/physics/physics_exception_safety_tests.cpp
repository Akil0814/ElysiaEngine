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
#include <cmath>
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
    { for (auto& collider : colliders) collider.shape = elysia::physics::BoxShape::from_rect({0,0,20,20}); }
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
    { collider.shape = elysia::physics::BoxShape::from_rect({0,0,20,20}); }
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
        bool completed=false;int failures=0;
        for(long budget=0;budget<64&&!completed;++budget)
        {
            elysia::core::GameObject owner(elysia::core::DepthLayer::Character);
            elysia::physics::PhysicsWorld world;
            std::array<elysia::physics::Collider,2> colliders{};
            for(auto& collider:colliders)collider.shape=elysia::physics::BoxShape::from_rect({0,0,20,20});
            fail_after=budget;bool caught=false;
            try{(void)world.register_object(owner,{},colliders);}
            catch(const std::bad_alloc&){caught=true;}
            fail_after=-1;
            if(caught)
            {
                ++failures;
                require(world.registered_object_count()==0&&world.registered_collider_count()==0,
                    "every failed registration allocation leaves no owner or collider record");
                require(world.register_object(owner,{},colliders).is_valid(),
                    "registration succeeds after rollback at every allocation point");
            }
            else completed=true;
            require(world.registered_object_count()==1&&world.registered_collider_count()==2,
                "successful or recovered registration owns the complete collider set");
            world.step(1.0/60);
        }
        require(completed&&failures>0,"registration sweep covers allocation failures through first success");
    }
    else if (!std::strcmp(mode,"ownership"))
    {
        bool completed=false;int failures=0;
        for(long budget=0;budget<64&&!completed;++budget)
        {
            int destroyed=0;
            {
                OwnershipScene scene;auto object=std::make_unique<OwnedBody>(destroyed);auto* address=object.get();
                fail_after=budget;bool caught=false;
                try{(void)scene.add_object(std::move(object));}
                catch(const std::exception&){caught=true;}
                fail_after=-1;
                if(caught)
                {
                    ++failures;
                    require(destroyed==1&&!scene.contains_object_address(address)&&scene.registrations()==0,
                        "every failed Scene allocation releases the object and physics references");
                }
                else
                {
                    completed=true;
                    require(scene.contains_object_address(address)&&scene.registrations()==1,
                        "successful Scene registration retains object ownership");
                }
            }
            require(destroyed==1,"each ownership-sweep fixture destroys its object exactly once");
        }
        require(completed&&failures>0,"ownership sweep covers allocation failures through first success");
    }
    else if (!std::strcmp(mode,"debug"))
    {
        elysia::core::GameObject owner(elysia::core::DepthLayer::Character);
        elysia::physics::PhysicsWorld world;
        std::array<elysia::physics::Collider,1> colliders{};
        colliders[0].shape = elysia::physics::BoxShape::from_rect({0,0,20,20});
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
    else if (!std::strcmp(mode,"debug_extended"))
    {
        using namespace elysia::physics;
        bool completed=false;int failures=0;
        for(long budget=0;budget<512&&!completed;++budget)
        {
            std::array<DeferredBody,4> owners;PhysicsWorld world;
            std::array<PhysicsObjectHandle,4> handles;
            for(std::size_t n=0;n<owners.size();++n){owners[n].set_position({float(n*300),0});handles[n]=world.register_object(owners[n],{.type=n==0?BodyType::Dynamic:BodyType::Kinematic},{&owners[n].collider,1});}
            for(std::size_t n=1;n<owners.size();++n){ChainDefinition d;d.local_vertices={{0,100},{100,100},{200,120}};world.create_chain(handles[n],d);world.set_target_transform(handles[n],{{float(n*300+1),0},.1f},1./60);}
            world.create_weld_joint({handles[1],handles[0]});
            world.set_debug_capture(PhysicsDebugCapture::Shapes);const auto previous=world.debug_snapshot();
            const auto state=world.body_state(handles[1]);
            fail_after=budget;bool caught=false;
            try{world.set_debug_capture(PhysicsDebugCapture::All);}catch(const std::bad_alloc&){caught=true;}
            fail_after=-1;
            if(caught){++failures;require(world.debug_capture()==PhysicsDebugCapture::Shapes&&world.debug_snapshot().chains.empty()&&world.debug_snapshot().targets.empty()&&world.debug_snapshot().joints.empty()&&world.debug_snapshot().shapes.size()==previous.shapes.size()&&world.debug_snapshot().bodies.size()==previous.bodies.size()&&world.debug_snapshot().bodies.front().state.mass==previous.bodies.front().state.mass,"Every extended capture allocation failure preserves old snapshot and mode");require(world.body_state(handles[1])->velocity==state->velocity,"Capture failure cannot change native motion");world.set_debug_capture(PhysicsDebugCapture::All);}
            else completed=true;
            require(world.debug_snapshot().chains.size()==3&&world.debug_snapshot().targets.size()==3&&world.debug_snapshot().joints.size()==1,"Extended capture succeeds after every failed allocation");
            world.set_debug_capture(PhysicsDebugCapture::None);require(world.debug_snapshot().chains.empty()&&world.debug_snapshot().targets.empty()&&world.debug_snapshot().bodies.empty(),"Capture shutdown clears nested data without stale records");
        }
        require(completed&&failures>0,"Extended capture sweeps all allocations through first success");
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
    else if (!std::strcmp(mode,"shape_update"))
    {
        using namespace elysia::physics;
        int failures=0;
        for(long budget=0;budget<4;++budget)
        {
            DeferredBody owner;PhysicsWorld world;
            auto h=world.register_object(owner,{}, {&owner.collider,1});auto id=world.collider_id(h,0);
            auto replacement=owner.collider;replacement.shape=CapsuleShape{{100,0},{100,20},8};
            fail_after=budget;bool caught=false;
            try{world.update_collider(id,replacement);}catch(const std::bad_alloc&){caught=true;}
            fail_after=-1;
            if(caught)
            {
                ++failures;
                require(world.contains_collider(id)&&world.raycast({{-10,10},{1,0},40}),
                    "allocation failure during replacement preserves old collider and native mapping");
                require(!world.raycast({{80,10},{1,0},40}),"failed replacement leaves no orphan shape");
            }
            world.step(1.0/60);
        }
        require(failures>0,"shape replacement fault injection reaches an allocation");
    }
    else if (!std::strcmp(mode,"mass_properties"))
    {
        using namespace elysia::physics;
        for (bool deferred : {false, true})
        {
            bool completed = false;
            int failures = 0;
            for (long budget = 0; budget < 64 && !completed; ++budget)
            {
                DeferredBody owner;
                PhysicsWorld world;
                BodyDefinition definition;
                definition.fixed_rotation = false;
                definition.mass_properties = BodyMassData{2, {2, 3}, 400};
                PhysicsObjectHandle h;
                bool caught = false;
                if (deferred)
                {
                    h = world.register_object(owner, definition, {&owner.collider, 1});
                    owner.tick = [&] {
                        fail_after = budget;
                        try { (void)world.set_mass_properties(h, BodyMassData{3, {4, 5}, 600}); }
                        catch (const std::bad_alloc &) { caught = true; }
                        fail_after = -1;
                        require(std::get<BodyMassData>(*world.mass_properties(h)).mass == 2,
                            "queue allocation does not commit requested mass properties early");
                    };
                    world.step(1. / 60);
                    owner.tick = {};
                    require(std::get<BodyMassData>(*world.mass_properties(h)).mass == (caught ? 2 : 3),
                        "failed mass command keeps original config and successful command commits at FIFO flush");
                    require(std::abs(world.body_state(h)->mass - (caught ? 2 : 3)) < .001f,
                        "native mass agrees with committed config after queue allocation failure");
                }
                else
                {
                    fail_after = budget;
                    try { h = world.register_object(owner, definition, {&owner.collider, 1}); }
                    catch (const std::bad_alloc &) { caught = true; }
                    fail_after = -1;
                    if (caught)
                    {
                        require(world.registered_object_count() == 0 && world.registered_collider_count() == 0,
                            "failed custom-mass registration retires every partial resource");
                        h = world.register_object(owner, definition, {&owner.collider, 1});
                    }
                    require(h.is_valid() && world.body_state(h)->mass == 2,
                        "custom-mass registration succeeds after each rollback");
                }
                if (caught) ++failures;
                else completed = true;
            }
            require(completed && failures > 0, "mass registration and command sweep covers each allocation to success");
        }
    }
    else if (!std::strcmp(mode,"joint_queue"))
    {
        using namespace elysia::physics;
        for(int action=0;action<3;++action)
        {
            DeferredBody anchor,bob;PhysicsWorld world;
            auto a=world.register_object(anchor,{.type=BodyType::Static},{&anchor.collider,1});
            auto b=world.register_object(bob,{}, {&bob.collider,1});
            auto h=world.create_distance_joint({a,b,{}, {},{.length=30}});
            bool caught=false;
            bob.tick=[&] {
                fail_after=0;
                try
                {
                    if(action==0)(void)world.create_distance_joint({a,b});
                    if(action==1)(void)world.update_distance_joint(h,{.length=60});
                    if(action==2)(void)world.destroy_joint(h);
                }
                catch(const std::bad_alloc&){caught=true;}
                fail_after=-1;
            };
            world.step(1.0/60);bob.tick={};
            require(caught&&world.joint_state(h)&&world.last_step_stats().joints==1,
                "failed joint command allocation leaves the existing joint usable and no pending record");
            require(std::abs(std::get<DistanceJointState>(world.joint_state(h)->details).settings.length-30)<.001f,
                "failed joint settings allocation preserves original settings");
            require(world.destroy_joint(h),"joint can be destroyed after failed queue operation");
        }
    }
    else if (!std::strcmp(mode,"chain") || !std::strcmp(mode,"chain_deferred"))
    {
        using namespace elysia::physics;
        const bool deferred=!std::strcmp(mode,"chain_deferred");bool completed=false;int failures=0;
        for(long budget=0;budget<256&&!completed;++budget)
        {
            DeferredBody owner;PhysicsWorld world;auto a=world.register_object(owner,{.type=BodyType::Static},{&owner.collider,1});
            ChainDefinition d;d.local_vertices={{0,100},{100,100},{200,100}};d.tag=std::string(80,'t');ChainHandle h;
            bool caught=false;
            auto create=[&]{h=world.create_chain(a,d);};
            if(deferred)owner.tick=[&]{fail_after=budget;create();};else fail_after=budget;
            try{if(deferred)world.step(1./60);else create();}catch(const std::bad_alloc&){caught=true;}
            fail_after=-1;owner.tick={};
            if(caught&&!world.chain_state(h)){++failures;require(world.registered_collider_count()==1,"Every chain allocation failure retires pending/native children and mappings");
                require(!world.segment_cast({{50,50},{50,150}}),"Failed chain leaves no query proxy");h=world.create_chain(a,d);}
            else completed=true;
            require(world.chain_state(h)&&world.registered_collider_count()==3,"Chain succeeds after rollback");
            world.destroy_chain(h);world.step(1./60);require(world.last_step_stats().chains==0,"No orphan native chain remains after destruction");
        }
        require(completed&&failures>0,"Chain sweep covers every allocation through first success");
    }
    else if (!std::strcmp(mode,"chain_destroy"))
    {
        using namespace elysia::physics;
        bool completed=false;int failures=0;
        for(long budget=0;budget<64&&!completed;++budget)
        {
            DeferredBody owner,sensor;PhysicsWorld world;auto a=world.register_object(owner,{.type=BodyType::Static},{&owner.collider,1});
            ChainDefinition d;d.local_vertices={{0,100},{100,100},{200,100}};auto h=world.create_chain(a,d);
            sensor.set_position({50,90});sensor.collider.response=CollisionResponse::Overlap;auto b=world.register_object(sensor,{}, {&sensor.collider,1});world.step(1./60);
            auto child=world.chain_collider(h,0);fail_after=budget;bool caught=false;
            try{world.destroy_chain(h);}catch(const std::bad_alloc&){caught=true;}fail_after=-1;
            if(caught){++failures;require(world.chain_state(h)&&world.contains_collider(child),"Failed chain destruction keeps the entire group usable");require(world.destroy_chain(h),"Destruction retries after allocation failure");}
            else completed=true;
            world.step(1./60);require(!world.chain_state(h)&&world.registered_collider_count()==2,"No children survive chain destruction");
        }
        require(completed&&failures>0,"Chain destruction sweeps invalidation allocations");
    }
    else if (!std::strcmp(mode,"stage2_joints"))
    {
        using namespace elysia::physics;
        for(int type=0;type<6;++type)for(int action=0;action<3;++action)
        {
            bool completed=false;int failures=0;
            for(long budget=0;budget<64&&!completed;++budget)
            {
                DeferredBody anchor,bob;PhysicsWorld world;auto a=world.register_object(anchor,{.type=BodyType::Static},{&anchor.collider,1});auto b=world.register_object(bob,{}, {&bob.collider,1});JointHandle h;
                auto create=[&]{switch(type){case 0:return world.create_weld_joint({a,b});case 1:return world.create_prismatic_joint({a,b});case 2:return world.create_wheel_joint({a,b});case 3:return world.create_mouse_joint({a,b});case 4:return world.create_motor_joint({a,b});default:return world.create_filter_joint({a,b});}};
                auto update=[&]{switch(type){case 0:return world.update_weld_joint(h,{.linear_frequency_hz=5});case 1:return world.update_prismatic_joint(h,{.target_translation=50});case 2:return world.update_wheel_joint(h,{.frequency_hz=5});case 3:return world.update_mouse_joint(h,{{50,0}});case 4:return world.update_motor_joint(h,{{50,0}});default:return world.destroy_joint(h);}};
                if(action!=0)h=create();bool caught=false;
                bob.tick=[&]{fail_after=budget;try{if(action==0)h=create();else if(action==1)update();else world.destroy_joint(h);}catch(const std::bad_alloc&){caught=true;}fail_after=-1;};
                world.step(1./60);bob.tick={};
                if(caught){++failures;require(world.joint_state(h).has_value()==(action!=0),"Failed joint allocation leaves no pending record and preserves existing joint");}
                else completed=true;
                world.reset();
            }
            require(completed&&failures>0,"Each new joint command sweeps allocation failures");
        }
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
