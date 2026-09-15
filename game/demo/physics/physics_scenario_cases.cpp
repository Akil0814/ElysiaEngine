#include "physics_scenario_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace example::demo::physics
{
void PhysicsScenario::Impl::build_motion()
{
    finish_at=60;
    auto fixed=body({30,50},{20,20},BodyType::Static).physics_handle();
    auto kin=body({30,100},{20,20},BodyType::Kinematic).physics_handle();
    auto falling=body({150,30},{20,20},BodyType::Dynamic,true).physics_handle();
    BodyDefinition d;
    d.gravity_scale=0; d.mass_policy=MassPolicy::ExplicitMass; d.mass=2;
    Collider c; c.shape=AabbShape{{0,0,20,20}};
    auto force=add<ScenarioBody>(Vector2{250,40},d,std::vector<Collider>{c}).physics_handle();
    auto impulse=add<ScenarioBody>(Vector2{250,100},d,std::vector<Collider>{c}).physics_handle();
    d.linear_damping=5;
    auto damped=add<ScenarioBody>(Vector2{400,40},d,std::vector<Collider>{c}).physics_handle();
    d.linear_damping=0; d.fixed_rotation=false;
    auto rotating=add<ScenarioBody>(Vector2{500,50},d,std::vector<Collider>{c}).physics_handle();
    auto torqued=add<ScenarioBody>(Vector2{600,50},d,std::vector<Collider>{c}).physics_handle();
    world.set_velocity(fixed,{60,0}); world.set_velocity(kin,{60,0});
    world.apply_impulse(impulse,{120,0}); world.set_velocity(damped,{120,0});
    world.apply_angular_impulse(rotating,100);
    world.apply_torque(force,100); // fixed rotation must reject rotation, not the command.
    before=[=,this](unsigned){world.apply_force(force,{120,0});world.apply_torque(torqued,100);};
    after=[=,this](unsigned n){if(n!=60)return;
        check("static.position",world.body_state(fixed)->position.x,30,.01);
        check("kinematic.position",world.body_state(kin)->position.x,90,.1);
        check("gravity.velocity",world.body_state(falling)->velocity.y,980,1);
        check("force.mass_response",world.body_state(force)->velocity.x,60,.05);
        check("impulse.mass_response",world.body_state(impulse)->velocity.x,60,.05);
        truth("damping.reduces_speed",world.body_state(damped)->velocity.x<5);
        check("fixed_rotation",world.body_state(force)->angular_velocity,0,.001);
        truth("angular_impulse",world.body_state(rotating)->angular_velocity>0);
        truth("torque",world.body_state(torqued)->angular_velocity>0);
    };
}
void PhysicsScenario::Impl::build_materials()
{
    finish_at=800;
    auto& floor=body({0,500},{1000,20},BodyType::Static);
    floor.colliders[0].material={1,0}; world.update_collider(floor.physics_collider(0),floor.colliders[0]);
    auto& slippery=body({20,479},{20,20},BodyType::Dynamic,true);
    slippery.colliders[0].material={0,0}; world.update_collider(slippery.physics_collider(0),slippery.colliders[0]);
    auto& rough=body({350,479},{20,20},BodyType::Dynamic,true);
    rough.colliders[0].material={1,0}; world.update_collider(rough.physics_collider(0),rough.colliders[0]);
    auto& ball=body({550,280},{24,24},BodyType::Dynamic,true,true);
    ball.colliders[0].material={0,0.8f};world.update_collider(ball.physics_collider(0),ball.colliders[0]);
    std::vector<PhysicsObjectHandle> stack;
    for(int i=0;i<4;++i) stack.push_back(body({650,479.f-i*22},{20,20},BodyType::Dynamic,true).physics_handle());
    body({800,100},{15,150},BodyType::Static);
    auto wall=body({730,150}).physics_handle();world.set_velocity(wall,{180,0});
    body({850,80},{100,15},BodyType::Static);
    auto ceiling=body({880,180}).physics_handle();world.set_velocity(ceiling,{0,-180});
    auto a=slippery.physics_handle(), b=rough.physics_handle(), c=ball.physics_handle();
    before=[=,this](unsigned n){
        if(n==10){world.set_velocity(a,{120,0});world.set_velocity(b,{120,0});}
        if(n==800)world.apply_impulse(stack.back(),{0,-50});
    };
    auto observations=std::make_shared<std::array<bool,4>>();
    after=[=,this](unsigned n){
        (*observations)[0]|=world.contact_state(wall).wall_right;
        (*observations)[1]|=world.contact_state(ceiling).ceiling;
        (*observations)[2]|=world.body_state(c)->velocity.y < -100;
        (*observations)[3]|=world.contact_state(a).grounded;
        if(n==80){
            truth("friction.relative_speed",world.body_state(a)->velocity.x>world.body_state(b)->velocity.x+50);
            truth("wall.contact",(*observations)[0]);truth("ceiling.contact",(*observations)[1]);
            truth("ground.contact",(*observations)[3]);truth("circle.restitution",(*observations)[2]);
        }
        if(n==780){
            for(auto h:stack){truth("stack.rests",world.body_state(h)->position.y<501);truth("stack.sleeps",!world.body_state(h)->awake);}
        }
        if(n==800)truth("impulse.wakes",world.body_state(stack.back())->awake);
    };
}
void PhysicsScenario::Impl::build_ccd()
{
    finish_at=1;
    struct Run{PhysicsObjectHandle h;bool blocks;float origin;};
    auto runs=std::make_shared<std::vector<Run>>();
    for(int i=0;i<8;++i)
    {
        const float y=30.f+i*65;
        body({250,y},{2,40},i==7?BodyType::Static:BodyType::Dynamic);
        BodyDefinition d;d.gravity_scale=0;d.velocity={6000,0};d.bullet=i==4;
        Collider c;c.shape=CircleShape{{},1};
        c.detection_mode=(i==1||i==3||i==4||i==5)?CollisionDetectionMode::Continuous:CollisionDetectionMode::Discrete;
        Collider extra=c;extra.enabled=false;
        extra.detection_mode=i==5?CollisionDetectionMode::Continuous:CollisionDetectionMode::Discrete;
        auto& p=add<ScenarioBody>(Vector2{200,y+10},d,std::vector<Collider>{c,extra});
        auto h=p.physics_handle();
        if(i>=2 && i<=5){
            p.tick=[&,h,i]{auto c=p.colliders[0];c.detection_mode=i==2?CollisionDetectionMode::Continuous:CollisionDetectionMode::Discrete;world.update_collider(world.collider_id(h,0),c);};
        }
        if(i==6){c.detection_mode=CollisionDetectionMode::Continuous;world.update_collider(p.physics_collider(0),c);}
        runs->push_back({h,i!=0&&i!=3,200});
    }
    after=[=,this](unsigned){for(std::size_t i=0;i<runs->size();++i){const auto& r=(*runs)[i];
        auto x=world.body_state(r.h)->position.x-r.origin;
        truth("ccd.variant_"+std::to_string(i),r.blocks?x<60:x>90);
    }};
}
void PhysicsScenario::Impl::build_filters()
{
    finish_at=5;
    std::vector<PhysicsObjectHandle> visitors;
    std::vector<ColliderId> sensors;
    for(int i=0;i<7;++i){
        auto& s=body({40.f+i*125,200},{30,30},BodyType::Static);
        auto& a=body({40.f+i*125,200},{20,20});
        s.colliders[0].response=(i==4||i==6)?CollisionResponse::Block:CollisionResponse::Overlap;
        if(i==2)s.colliders[0].response=CollisionResponse::Ignore;
        if(i==3)s.colliders[0].filter.mask=0;
        if(i==4){s.colliders[0].filter={1,0,1};a.colliders[0].filter={1,0,1};}
        if(i==5){s.colliders[0].filter.group=-1;a.colliders[0].filter.group=-1;}
        world.update_collider(s.physics_collider(0),s.colliders[0]);
        world.update_collider(a.physics_collider(0),a.colliders[0]);
        sensors.push_back(s.physics_collider(0));visitors.push_back(a.physics_handle());
    }
    auto count=[this](ColliderId id,CollisionEventPhase phase){
        return std::ranges::count_if(events,[=](const auto& e){return e.phase==phase &&
            (e.contact.pair.first==CollisionTarget::from_collider(id)||e.contact.pair.second==CollisionTarget::from_collider(id));});
    };
    before=[=,this](unsigned n){if(n==3){world.set_collider_enabled(sensors[0],false);world.unregister_object(visitors[1]);}};
    after=[=,this](unsigned n){
        if(n==2){for(int i=0;i<7;++i){bool expected=i==0||i==1||i==4||i==6;
            check("filter.begin_"+std::to_string(i),count(sensors[i],CollisionEventPhase::Begin),expected?1:0);
            if(expected)truth("filter.stay_"+std::to_string(i),count(sensors[i],CollisionEventPhase::Stay)>0);
        }}
        if(n==5){check("disable.exactly_one_end",count(sensors[0],CollisionEventPhase::End),1);
            check("delete.exactly_one_end",count(sensors[1],CollisionEventPhase::End),1);}
    };
}
void PhysicsScenario::Impl::build_queries()
{
    finish_at=1;
    auto a=body({100,100},{20,20},BodyType::Static,true,true).physics_handle();
    auto b=body({200,100},{20,20},BodyType::Static).physics_handle();
    auto r=body({400,150},{100,10},BodyType::Static).physics_handle();
    world.set_transform(r,{{400,150},1.57079632679f});
    after=[=,this](unsigned){
        std::vector<CollisionQueryHit> hits;world.raycast_all({{0,110},{1,0},300,{}},hits);
        truth("ray.sorted",hits.size()==2&&hits[0].distance<hits[1].distance);
        auto hit=world.segment_cast({{0,110},{300,110},{}});
        truth("segment.first",hit&&hit->target==CollisionTarget::from_collider(world.collider_id(a,0)));
        hit=world.raycast({{110,110},{1,0},200,{}});truth("ray.starts_inside",hit&&hit->fraction==0);
        std::vector<CollisionOverlapQueryHit> overlaps;
        world.overlap_aabb({{420,148,4,4},{}},overlaps);truth("rotated.narrow_overlap",overlaps.empty());
        world.overlap_circle({{110,110},12,{}},overlaps);check("circle.overlap",overlaps.size(),1);
        world.overlap_aabb({{90,90,40,40},{}},overlaps);check("aabb.overlap",overlaps.size(),1);
        hit=world.sweep_aabb({{0,100,10,10},{300,0},{}});truth("sweep.hit",hit&&hit->fraction>0&&hit->fraction<1);
        world.raycast_all({{0,110},{1,0},300,{1,0,0}},hits);truth("query.mask",hits.empty());
        world.teleport_object(a,{600,100});world.teleport_object(b,{700,100});
        truth("query.teleport_immediate",!world.raycast({{0,110},{1,0},300,{}}));
    };
}
void PhysicsScenario::Impl::build_one_way()
{
    finish_at=24;
    struct Run{PhysicsObjectHandle actor;Vector2 start,velocity;bool pass;};
    auto runs=std::make_shared<std::vector<Run>>();
    const PassThroughDirection dirs[]={PassThroughDirection::Up,PassThroughDirection::Down,
        PassThroughDirection::Left,PassThroughDirection::Right,PassThroughDirection::Up|PassThroughDirection::Left,
        PassThroughDirection::Up|PassThroughDirection::Left};
    view={0,0,1000,850};
    for(int i=0;i<12;++i){
        int direction=i/2; bool pass=(i%2)==0;
        Vector2 origin{100.f+(i%5)*190,120.f+(i/5)*280};
        auto& platform=body(origin,{20,20},BodyType::Static);
        platform.colliders[0].one_way=OneWayCollision{dirs[direction],1};
        platform.colliders[0].filter={1u<<i,1u<<i,0};
        world.update_collider(platform.physics_collider(0),platform.colliders[0]);
        Vector2 delta,velocity;
        if(direction==0||direction==4){delta={0,pass?40.f:-40.f};velocity={0,pass?-300.f:300.f};}
        if(direction==1){delta={0,pass?-40.f:40.f};velocity={0,pass?300.f:-300.f};}
        if(direction==2||direction==5){delta={pass?40.f:-40.f,0};velocity={pass?-300.f:300.f,0};}
        if(direction==3){delta={pass?-40.f:40.f,0};velocity={pass?300.f:-300.f,0};}
        auto& actor=body(origin+delta);
        actor.colliders[0].filter=platform.colliders[0].filter;world.update_collider(actor.physics_collider(0),actor.colliders[0]);
        world.set_velocity(actor.physics_handle(),velocity);
        runs->push_back({actor.physics_handle(),origin+delta,velocity,pass});
    }
    after=[=,this](unsigned n){if(n!=24)return;
        for(std::size_t i=0;i<runs->size();++i){const auto& r=(*runs)[i];float distance=(world.body_state(r.actor)->position-r.start).length();
            truth("one_way.direction_"+std::to_string(i),r.pass?distance>100:distance<45);}
    };
}
void PhysicsScenario::Impl::build_tiles()
{
    finish_at=660;
    auto& map=tile_map(10,6,{100,20},{0,300},TileOutOfBoundsPolicy::Empty);
    map.fill_row(0,0,9,one_way_tile());world.set_tile_world(map);
    auto h=body({90,260},{20,20},BodyType::Dynamic,true).physics_handle();
    auto& platform=body({500,230},{120,20},BodyType::Kinematic);
    platform.colliders[0].material={1,0};world.update_collider(platform.physics_collider(0),platform.colliders[0]);
    auto& passenger=body({520,209},{20,20},BodyType::Dynamic,true);
    passenger.colliders[0].material={1,0};world.update_collider(passenger.physics_collider(0),passenger.colliders[0]);
    auto p=passenger.physics_handle(),k=platform.physics_handle();
    before=[=,this](unsigned n){
        if(n==120)world.set_velocity(k,{30,0});
        if(n==300){std::vector<CollisionContact> contacts;auto target=CollisionTarget::from_collider(world.collider_id(h,0));world.collect_contacts(target,contacts);
            check("tile.two_supports",contacts.size(),2);
            for(const auto& c:contacts)truth("tile.drop_request",world.request_pass_through(target.collider,c.pair.first==target?c.pair.second:c.pair.first));}
        if(n==350){world.teleport_object(h,{90,260},TeleportVelocityMode::Clear);}
        if(n==600){(void)tiles->set_cell({0,0},{});(void)tiles->set_cell({1,0},{});world.update_tiles({0,0},{1,0});}
    };
    after=[=,this](unsigned n){
        if(n==299){truth("tile.seam_grounded",world.contact_state(h).grounded);truth("moving_platform.carries",world.body_state(p)->position.x>560);}
        if(n==340)truth("tile.drop_all_supports",world.body_state(h)->position.y>340);
        if(n==580)truth("tile.reland",world.contact_state(h).grounded);
        if(n==660)truth("tile.update_removes_support",world.body_state(h)->position.y>400);
    };
}
void PhysicsScenario::Impl::build_joints()
{
    finish_at=420;
    auto anchor=body({180,120},{12,12},BodyType::Static).physics_handle();
    auto bob=body({180,220},{20,20},BodyType::Dynamic,true,true).physics_handle();
    auto distance=world.create_distance_joint({anchor,bob,{},{},100});joints.push_back(distance);
    auto sa=body({420,120},{12,12},BodyType::Static).physics_handle();
    auto sb=body({420,280},{20,20},BodyType::Dynamic,true,true).physics_handle();
    auto spring=world.create_distance_joint({sa,sb,{},{},100,true,3,.7f});joints.push_back(spring);
    BodyDefinition d;d.gravity_scale=0;d.fixed_rotation=false;
    Collider c;c.shape=AabbShape{{-40,-5,80,10}};
    auto ma=body({700,220},{12,12},BodyType::Static).physics_handle();
    auto mb=add<ScenarioBody>(Vector2{700,220},d,std::vector<Collider>{c}).physics_handle();
    RevoluteJointDefinition motor;motor.first=ma;motor.second=mb;motor.enable_motor=true;motor.motor_speed=1;
    motor.max_motor_torque=200000;motor.enable_limit=true;motor.lower_angle=0;motor.upper_angle=.5f;
    auto mj=world.create_revolute_joint(motor);joints.push_back(mj);
    before=[=,this](unsigned n){if(n==400){truth("joint.destroy",world.destroy_joint(distance));world.unregister_object(sb);}};
    after=[=,this](unsigned n){
        if(n==350){auto j=world.joint_state(distance);check("distance.length",(j->anchor_second-j->anchor_first).length(),100,1);
            j=world.joint_state(spring);check("spring.converges",(j->anchor_second-j->anchor_first).length(),100,8);
            check("motor.limit",world.body_state(mb)->angle,.5,.08);}
        if(n==420){truth("joint.invalid_after_destroy",!world.joint_state(distance));truth("joint.invalid_after_body_removed",!world.joint_state(spring));}
    };
}
void PhysicsScenario::Impl::build_lifecycle()
{
    finish_at=8;
    auto& actor=body({100,100});auto h=actor.physics_handle();
    auto spawned=std::make_shared<PhysicsObjectHandle>();
    actor.tick=[&,h,spawned]{
        switch(result.steps){
        case 1: actor.set_velocity_x(100);actor.set_velocity_y(200);world.set_transform(h,{{100,100},1});world.teleport_object(h,{200,100});break;
        case 2: actor.set_velocity_y(300);actor.set_velocity_x(400);break;
        case 3: actor.set_velocity_x(500);actor.set_velocity({10,20});actor.set_velocity_y(30);break;
        case 4: *spawned=body({500,100}).physics_handle();world.set_transform(*spawned,{{500,100},1});world.teleport_object(*spawned,{600,100},TeleportVelocityMode::Clear);break;
        case 5: world.teleport_object(*spawned,{700,100});world.unregister_object(*spawned);truth("removed.rejects_command",!world.set_velocity_x(*spawned,2));break;
        default:break;
        }
    };
    after=[=,this](unsigned n){
        auto s=world.body_state(h);
        if(n==1){check("velocity.x_then_y.x",s->velocity.x,100,.01);check("velocity.x_then_y.y",s->velocity.y,200,.01);check("teleport.keeps_rotation",s->angle,1,.01);}
        if(n==2){check("velocity.y_then_x.x",s->velocity.x,400,.01);check("velocity.y_then_x.y",s->velocity.y,300,.01);}
        if(n==3){check("velocity.mixed.x",s->velocity.x,10,.01);check("velocity.mixed.y",s->velocity.y,30,.01);}
        if(n==4){check("pending.rotation",world.body_state(*spawned)->angle,1,.01);check("pending.velocity_clear",world.body_state(*spawned)->velocity.length(),0,.01);}
        if(n==5)truth("removed.handle_invalid",!world.body_state(*spawned));
        if(n==6){world.set_transform(h,{{200,100},1});auto angle=world.body_state(h)->angle;for(int i=0;i<100;++i)world.teleport_object(h,{200,100});check("teleport.no_drift",world.body_state(h)->angle,angle,.0001);}
        if(n==8){world.reset();check("reset.objects",world.registered_object_count(),0);truth("reset.invalidates_handles",!world.contains_object(h));}
    };
}
void PhysicsScenario::Impl::build_timing()
{
    finish_at=60;
    auto h=body({100,100}).physics_handle();world.set_velocity(h,{60,0});
    after=[=,this](unsigned n){if(n!=60)return;
        check("fixed_steps.position",world.body_state(h)->position.x,160,.05);
        // Exercise the public clock contract on a separate empty diagnostic world;
        // this does not duplicate the visible moving fixture.
        PhysicsWorld clock;
        check("clock.fractional_step",clock.advance(1.0/120),0);
        check("clock.next_half",clock.advance(1.0/120),1);
        check("clock.large_delta_limit",clock.advance(1.0),8);
        truth("clock.dropped_steps",clock.last_step_stats().dropped_fixed_steps>=51);
        world.advance(1.0/120);
        auto pose=world.render_pose(h);auto state=world.body_state(h);
        truth("render.interpolates",pose&&pose->position.x<state->position.x&&pose->position.x>state->position.x-1.1f);
    };
}
void PhysicsScenario::Impl::build_stress()
{
    finish_at=720;
    if(descriptor.id=="stress_tiles"){
        const int size=std::array{64,128,256}[tier];
        auto& map=tile_map(size,size,{8,8});
        for(int y=0;y<size;++y)for(int x=0;x<size;++x)
            if(y==size-1||x%8==0&&y%8==0)(void)map.set_cell({x,y},block_tile());
        world.set_tile_world(map);view={0,0,float(size*8),float(size*8)};
        for(int i=0;i<100;++i)body({float(i%10)*32+12,float(i/10)*32+12},{4,4});
        before=[=,this](unsigned n){if(n%30==0){int x=int(n/30)%size;(void)tiles->set_cell({x,1},n%60?block_tile():TileCollisionCell{});truth("stress.tile_update",world.update_tiles({x,1},{x,1}));}
            for(int i=0;i<32;++i){std::vector<CollisionOverlapQueryHit> hits;world.overlap_aabb({{float((n+i)%size)*8,0,8,float(size)*8},{}},hits);}
        };
    }else{
        const bool dense=descriptor.id=="stress_contacts";
        int count=dense?std::array{100,500,1000}[tier]:std::array{100,500,2000}[tier];
        const int cols=int(std::ceil(std::sqrt(double(count))));
        const float spacing=dense?10.1f:24.f;
        view={-30,-30,cols*spacing+60,cols*spacing+100};
        if(dense)body({-20,cols*spacing+5},{cols*spacing+40,20},BodyType::Static);
        for(int i=0;i<count;++i){auto h=body({float(i%cols)*spacing,float(i/cols)*spacing},{10,10},BodyType::Dynamic,dense).physics_handle();
            if(!dense)world.set_velocity(h,{3,0});}
    }
    auto contact_seen=std::make_shared<bool>(false);
    after=[=,this](unsigned n){*contact_seen|=world.last_step_stats().contacts>0;
        if(n!=720)return;
        bool finite=true,valid=true;
        for(auto h:handles){auto s=world.body_state(h);valid&=s.has_value();if(s)finite&=std::isfinite(s->position.x)&&std::isfinite(s->position.y)&&std::isfinite(s->velocity.x)&&std::isfinite(s->angle);}
        truth("stress.finite_state",finite);truth("stress.valid_handles",valid);
        check("stress.object_count",world.registered_object_count(),expected_objects);
        check("stress.samples",timings.size(),600);
        if(descriptor.id=="stress_contacts")truth("stress.contacts_observed",*contact_seen);
        // Destroy/recreate cleanup is additionally checked by the shared runner tests.
    };
}
}
