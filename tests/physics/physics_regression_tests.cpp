#include "physics_test_support.h"
#include <iostream>
struct Tiles:ITileCollisionWorld {
 TileCollisionCell cell{TileCollisionType::Block};
 Vector2 world_origin()const noexcept override{return {0,100};} Vector2 tile_size()const noexcept override{return {100,20};}
 int columns()const noexcept override{return 1;} int rows()const noexcept override{return 1;}
 TileOutOfBoundsPolicy out_of_bounds_policy()const noexcept override{return TileOutOfBoundsPolicy::Empty;}
 TileCollisionCell cell_at(TileCoordinate)const noexcept override{return cell;}
};
int main(){
 for(int fps:{30,60,120,144}){
  Probe o;o.definition.mass_policy=MassPolicy::ExplicitMass;o.definition.mass=1;PhysicsWorld w;auto h=o.add(w);int ticks=0;
  o.tick=[&]{++ticks;w.apply_force(h,{60,0});};for(int i=0;i<fps;++i)w.advance(1.0/fps);
  require(ticks==60&&near(w.body_state(h)->velocity.x,60,0.01f),"Force and callback count independent of render cadence");
 }
 Tiles tiles;Probe o;o.set_position({20,0});PhysicsWorldConfig c;c.gravity={0,1000};PhysicsWorld w(c);auto h=o.add(w);w.set_tile_world(tiles);step(w,120);
 require(w.contact_state(h).grounded,"Tile grounds actor");tiles.cell.type=TileCollisionType::Empty;w.update_tiles({0,0},{0,0});step(w,30);require(o.position().y>150,"Dirty tile update removes collision");
 Tiles platform;platform.cell.type=TileCollisionType::OneWay;platform.cell.one_way=OneWayCollision{PassThroughDirection::Up,1};
 Probe jumper;jumper.set_position({20,140});jumper.definition.velocity={0,-500};PhysicsWorld pw(c);auto j=jumper.add(pw);pw.set_tile_world(platform);step(pw,20);
 require(jumper.position().y<100,"Jump passes upward through one-way");step(pw,100);require(pw.contact_state(j).grounded,"Fall lands on one-way");
 require(pw.request_pass_through(pw.collider_id(j,0),CollisionTarget::from_tile({0,0})),"Drop request accepted");step(pw,45);require(jumper.position().y>140,"Drop passes platform");
 // Sensor semantics are discrete; a cast supplies high-speed gameplay detection.
 Probe sensor,moving;sensor.collider.response=CollisionResponse::Overlap;sensor.set_position({100,0});moving.definition.velocity={12000,0};
 PhysicsWorld sweeps;auto s=sensor.add(sweeps);moving.add(sweeps);auto hit=sweeps.sweep_aabb({{20,0,10,10},{200,0},{}});require(hit&&hit->target==CollisionTarget::from_collider(sweeps.collider_id(s,0)),"Explicit cast catches thin sensor");
 std::cout<<"physics regression tests passed\n";
}
