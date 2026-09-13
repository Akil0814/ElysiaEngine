#include "physics_test_support.h"
#include "engine/physics/detail/physics_units.h"
#include <limits>
#include <iostream>
int main(){
 for(float scale:{1.f,32.f,100.f,1000.f}){
  elysia::physics::detail::PhysicsUnits u(scale);
  for(float v:{-10000.f,-1.f,0.f,1.f,2000.f}){
   require(near(u.from_length(u.to_length(v)),v,0.002f),"Linear quantities round trip");
   require(near(u.from_squared(u.to_squared(v)),v,0.002f),"Squared quantities round trip");
  }
 }
 for(float scale:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  bool rejected=false;try{PhysicsWorldConfig c;c.units_per_meter=scale;PhysicsWorld w(c);}catch(const std::invalid_argument&){rejected=true;}
  require(rejected,"Invalid scale rejected");
 }
 for(float scale:{50.f,100.f,200.f}){
  Probe o;o.collider.shape=AabbShape{{0,0,scale,scale}};o.definition.fixed_rotation=false;o.definition.mass_policy=MassPolicy::ExplicitMass;o.definition.mass=3;
  PhysicsWorldConfig c;c.units_per_meter=scale;PhysicsWorld w(c);auto h=o.add(w);
  auto before=w.body_state(h);require(before&&near(before->mass,3),"Explicit mass");
  require(near(before->rotational_inertia/(scale*scale),2,0.01f),"Inertia about local origin scales with length squared");
  w.apply_impulse(h,{3*scale,0});require(near(w.body_state(h)->velocity.x/scale,1),"Impulse conversion");
  float inertia=w.body_state(h)->rotational_inertia;
  // Use centered geometry for angular quantities; Box2D exposes inertia about origin.
  w.apply_torque(h,scale*scale);step(w);require(w.body_state(h)->angular_velocity>0,"Positive torque turns clockwise");
  require(inertia>0,"Nonzero inertia");
 }
 std::cout<<"physics units tests passed\n";
}
