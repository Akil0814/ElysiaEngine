#include "detail/physics_world_impl.h"
namespace elysia::physics {
void PhysicsWorld::Impl::capture_debug(){
 debug.clear();if(capture==PhysicsDebugCapture::None)return;
 auto add=[&](const Shape& s){
  if(B2_IS_NULL(s.native))return;PhysicsDebugShape out;out.target=s.target;auto transform=b2Body_GetTransform(b2Shape_GetBody(s.native));
  if(b2Shape_GetType(s.native)==b2_circleShape){auto c=b2Shape_GetCircle(s.native);out.current=WorldCircle{from(b2TransformPoint(transform,c.center)),units.from_length(c.radius)};}
  else {auto poly=b2Shape_GetPolygon(s.native);WorldPolygon p;for(int i=0;i<4;++i)p.vertices[i]=from(b2TransformPoint(transform,poly.vertices[i]));out.current=p;}
  out.previous=out.current;auto aabb=b2Shape_GetAABB(s.native);out.swept_bounds=elysia::core::Rect::from_points(from(aabb.lowerBound),from(aabb.upperBound));
  debug.shapes.push_back(out);
 };
 if(captures_physics_debug(capture,PhysicsDebugCapture::Shapes)){for(auto& [id,s]:shapes)add(s);for(auto& [id,s]:tile_shapes)add(s);}
 if(captures_physics_debug(capture,PhysicsDebugCapture::Contacts))debug.contacts.assign(cache.contacts().begin(),cache.contacts().end());
 if(captures_physics_debug(capture,PhysicsDebugCapture::Velocities))for(auto& [id,o]:objects)debug.velocities.push_back({{id},o.current.position,from(b2Body_GetLinearVelocity(o.native))});
}
}
