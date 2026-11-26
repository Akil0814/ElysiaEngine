#define SDL_MAIN_HANDLED
#include "particle_test_fixture.h"
#include "effect_request_assertions.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

using namespace elysia;
using namespace elysia::effects;
using elysia::tests::require;

namespace
{
void validation(tests::ParticleFixture& f)
{
    auto d=f.definition();
    require(ParticleEffect::validate(d,f.texture), "valid definition");
    const auto rejected=[&] { auto result=ParticleEffect::validate(d,f.texture); require(!result && !result.error().diagnostic.message.empty(), "invalid definition has diagnostic"); };
    d.effect_key.clear(); rejected(); d=f.definition();
    require(!ParticleEffect::validate(d,nullptr) && ParticleEffect::validate(d,nullptr).error().code==EffectRegistrationError::MissingTexture,"missing texture category");
    d.max_particles=0; rejected(); d=f.definition();
    d.max_particles=std::numeric_limits<std::size_t>::max(); rejected(); d=f.definition();
    d.burst_count=0; rejected(); d=f.definition();
    d.emission_rate=-1; rejected(); d=f.definition();
    d.emission_duration_seconds=0; rejected(); d=f.definition();
    d.lifetime_seconds={0,1}; rejected(); d=f.definition();
    d.velocity_min={1,0}; rejected(); d=f.definition();
    d.acceleration.x=std::numeric_limits<float>::infinity(); rejected(); d=f.definition();
    d.angle_degrees={2,1}; rejected(); d=f.definition();
    d.end_size={-1,1}; rejected(); d=f.definition();
    d.source_rect=core::Rect{6,0,4,4}; rejected(); d=f.definition();
    d.source_rect=core::Rect{-1,0,1,1}; rejected(); d=f.definition();
    d.layer=core::DepthLayer::Count; rejected(); d=f.definition();
    d.blend=static_cast<ParticleBlendMode>(99); rejected();
}
void simulation(tests::ParticleFixture& f)
{
    auto d=f.definition(); d.burst_count=8; d.velocity_min={-10,-20}; d.velocity_max={10,20};
    ParticleEffect a(d,f.texture,{8,4},{0,0},42), b(d,f.texture,{8,4},{0,0},42), c(d,f.texture,{8,4},{0,0},43);
    a.update(0.25); b.update(0.25);
    require(a.particles().size()==8 && a.particles()[0].position==b.particles()[0].position,"seed repeats particle simulation");
    require(a.particles()[0].velocity!=c.particles()[0].velocity,"independent seeds vary particles");
    d=f.definition(); d.velocity_min=d.velocity_max={10,0}; d.acceleration={4,0};
    ParticleEffect ballistic(d,f.texture,{8,4},{0,0},1); ballistic.update(0.5);
    require(std::abs(ballistic.particles()[0].position.x-5.5f)<0.0001f && ballistic.particles()[0].velocity.x==12,"constant acceleration uses elapsed time");
    d=f.definition(); d.burst_count=0; d.emission_rate=4;
    ParticleEffect continuous(d,f.texture,{8,4},{0,0},2);
    continuous.update(0.125); require(continuous.particles().empty(),"fractional emission is retained");
    continuous.update(0.125); require(continuous.particles().size()==1,"fraction combines into one emission");
    continuous.set_position({50,0}); continuous.update(0.25);
    require(continuous.particles().size()==2 && continuous.particles()[0].position.x==0 && continuous.particles()[1].position.x==50,"only new particles move with the emitter");
    require(continuous.stop() && !continuous.stop(),"stop is idempotent"); continuous.update(2);
    require(continuous.finished(),"stopped groups finish after remaining particles expire");
    d.emission_duration_seconds=0.5;
    ParticleEffect duration(d,f.texture,{8,4},{0,0},2); duration.update(1);
    require(duration.particles().size()==2 && std::abs(duration.particles()[0].age-0.75)<0.0001,"new particles receive within-frame age after finite emission");
    duration.update(1.5); require(duration.finished(),"finite emission automatically drains");
    d=f.definition(); d.max_particles=2; d.burst_count=100; d.emission_rate=100; d.lifetime_seconds={1,1};
    ParticleEffect capped(d,f.texture,{8,4},{0,0},3); require(capped.particles().size()==2,"burst obeys capacity");
    capped.update(0.5); capped.update(0.51); require(capped.particles().size()==2 && capped.particles()[0].age<0.02,"capacity drops old emission without catch-up debt");
    d.emission_rate=std::numeric_limits<double>::max();
    ParticleEffect extreme(d,f.texture,{8,4},{0,0},3); extreme.update(std::numeric_limits<double>::max());
    require(extreme.particles().size()<=2,"huge deltas remain bounded by capacity");
    const auto pc=capped.particle_capacity(), vc=capped.vertex_capacity(), ic=capped.index_capacity();
    const camera::Camera camera({0,0},{128,96});
    for(int i=0;i<200;++i){ capped.update(0.016); require(capped.render(f.renderer,camera),"warm render"); }
    require(pc==capped.particle_capacity() && vc==capped.vertex_capacity() && ic==capped.index_capacity(),"steady-state storage capacities remain fixed");
    ParticleRuntime runtime; int scene=0;
    auto h=runtime.create(d,f.texture,{.effect_key=d.effect_key,.seed=2},&scene,4); require(h,"runtime request");
    runtime.update(2,false,4); require(runtime.active(*h),"creation frame does not advance");
    runtime.update(2,true,5); require(runtime.active(*h),"scene pause freezes groups");
    tests::require_effect_failure(runtime.set_position({},{}),EffectRequestError::InvalidHandle);
    tests::require_effect_failure(runtime.set_position(*h,{std::numeric_limits<float>::quiet_NaN(),0}),EffectRequestError::InvalidRequest);
    require(runtime.cancel(*h),"cancel valid handle"); auto next=runtime.create(d,f.texture,{.effect_key=d.effect_key},&scene);
    require(next && next->slot==h->slot && next->generation!=h->generation && !runtime.cancel(*h),"stale handles cannot affect reused slots");
    runtime.unbind(&scene); require(!runtime.active(*next),"scene exit invalidates handle");
}
void benchmark(tests::ParticleFixture& f)
{
    for(const std::size_t count : {1000,10000})
    {
        auto d=f.definition(); d.max_particles=count; d.burst_count=count; d.lifetime_seconds={100,100}; d.start_size=d.end_size={2,2};
        ParticleEffect effect(d,f.texture,{8,4},{0,0},1); camera::Camera camera({0,0},{128,96});
        for(int i=0;i<20;++i){effect.update(0.001); require(effect.render(f.renderer,camera),"benchmark warm-up");}
        using clock=std::chrono::steady_clock;
        double update_ms=0, submit_ms=0;
        for(int i=0;i<100;++i)
        {
            auto start=clock::now(); effect.update(0.001); auto updated=clock::now();
            require(effect.render(f.renderer,camera),"benchmark batch submission"); auto submitted=clock::now();
            update_ms+=std::chrono::duration<double,std::milli>(updated-start).count();
            submit_ms+=std::chrono::duration<double,std::milli>(submitted-updated).count();
            require(SDL_RenderPresent(f.renderer),"benchmark present");
        }
        std::cout<<"particles="<<count<<" update_ms="<<update_ms/100<<" submit_ms="<<submit_ms/100<<" batches=1 renderer=software\n";
    }
}
}
int main(int argc,char** argv)
{
    tests::ParticleFixture fixture; validation(fixture); simulation(fixture);
    if(argc>1 && std::string_view(argv[1])=="--benchmark") benchmark(fixture);
}
