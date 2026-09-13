#pragma once
#include "engine/physics/physics_world.h"
#include "engine/core/game_object.h"
#include "engine/physics/contracts/physics_step_participant.h"
#include "tests/support/test_assertions.h"
#include <cmath>
#include <functional>
using namespace elysia::physics;
using elysia::tests::require;
using elysia::core::Vector2;
struct Probe:elysia::core::GameObject,PhysicsStepParticipant {
 Probe():GameObject(elysia::core::DepthLayer::Item){collider.shape=AabbShape{{0,0,20,20}};}
 BodyDefinition definition;
 Collider collider;
 std::function<void()> tick;
 void fixed_update(double) override {if(tick)tick();}
 PhysicsObjectHandle add(PhysicsWorld& w){return w.register_object(*this,definition,{&collider,1});}
};
inline bool near(float a,float b,float e=0.02f){return std::fabs(a-b)<=e;}
inline void step(PhysicsWorld& w,int n=1){for(int i=0;i<n;++i)w.advance(1.0/60);}
struct Events:ICollisionListener {
 std::vector<CollisionEvent> events;
 std::function<void(const CollisionEvent&)> callback;
 void on_collision_event(const CollisionEvent& e)override{events.push_back(e);if(callback)callback(e);}
 int count(CollisionEventPhase p) const {int n=0;for(auto& e:events)n+=e.phase==p;return n;}
};
