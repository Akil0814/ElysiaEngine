#include "physics_scenario_internal.h"
#include "../../../engine/gameplay/control/control_command.h"
#include <algorithm>

namespace example::demo::physics
{
namespace
{
using Key=elysia::input::RawInputControl;
struct ScriptInput
{
    elysia::input::InputActionMap map=example::input::make_default_gameplay_input_map();
    elysia::input::RawInputState previous;
    void send(elysia::gameplay::ControlCommandReceiver &player, std::initializer_list<Key> keys)
    {
        elysia::input::RawInputFrame raw;
        for(auto key:keys)raw.state.set_pressed(key,true);
        elysia::input::InputSnapshot snapshot{.sources={{elysia::input::InputSourceId::keyboard_mouse(),previous,raw}}};
        for(int i=1;i<int(Key::Count);++i) {
            auto key=static_cast<Key>(i);
            if(previous.is_pressed(key)!=raw.state.is_pressed(key)) snapshot.events.push_back({.control=key,.type=raw.state.is_pressed(key)?elysia::input::RawInputEventType::ControlPressed:elysia::input::RawInputEventType::ControlReleased,.source=elysia::input::InputSourceId::keyboard_mouse()});
        }
        previous=raw.state;
        auto result = map.resolve(snapshot);
        player.on_control_command({.state = std::move(result.frame), .events = std::move(result.events)},
                                 1.0 / 60.0);
    }
};
}
void PhysicsScenario::Impl::build_combat()
{
    auto input=std::make_shared<ScriptInput>();
    if(descriptor.id=="combat_collider")
    {
        finish_at=200;
        body({0,400},{1000,20},BodyType::Static);
        auto& player=add<PlatformPlayerCharacter>(elysia::core::Rect{100,344,34,56});
        auto& enemy=add<BlockCombatActor>(ActorConfig{{140,348,38,52},{230,80,80},elysia::gameplay::collision::teams::Enemy,50,0,true});
        auto& friend_actor=add<BlockCombatActor>(ActorConfig{{140,348,38,52},{80,200,130},elysia::gameplay::collision::teams::Player,50,0,true});
        // The ally hurtbox shares the attack volume without its body pushing the enemy out.
        world.set_collider_enabled(friend_actor.body_collider_id(),false);
        world.set_gravity_scale(friend_actor.physics_handle(),0);
        truth("combat.bind_player",player.bind_combat(combat));
        truth("combat.bind_enemy",enemy.bind_combat(combat));
        truth("combat.bind_ally",friend_actor.bind_combat(combat));
        auto saw_active=std::make_shared<bool>(false);
        before=[&,input](unsigned n){
            if(n==20||n==80){
                // Restore the attack fixture after knockback, using real physics commands.
                world.teleport_object(player.physics_handle(),{100,344},TeleportVelocityMode::Clear);
                world.teleport_object(enemy.physics_handle(),{140,348},TeleportVelocityMode::Clear);
                input->send(player,{Key::KeyJ});
            }else if(n==130)input->send(player,{Key::KeySpace});
            else input->send(player,{});
        };
        after=[&,saw_active](unsigned n){
            *saw_active|=player.collider_definitions()[2].enabled;
            if(n==10)truth("combat.grounded",combat.is_grounded(player));
            if(n==22)check("attack.windup_no_damage",enemy.health().current(),50);
            if(n==45){truth("attack.active_window",*saw_active);check("attack.one_hit",enemy.health().current(),25);
                check("attack.team_filter",friend_actor.health().current(),50);truth("attack.window_closed",!player.collider_definitions()[2].enabled);}
            if(n==70)check("attack.no_repeat",enemy.health().current(),25);
            if(n==110){truth("attack.death",!enemy.alive());
                truth("death.body_disabled",!enemy.collider_definitions()[0].enabled);truth("death.hit_disabled",!enemy.collider_definitions()[2].enabled);}
            if(n==131)truth("input.jump",player.velocity().y < -400);
            if(n==200)truth("jump.lands",combat.is_grounded(player));
        };
    }
    else if(descriptor.id=="combat_platform")
    {
        finish_at=300;
        auto& map=tile_map(10,6,{100,20},{0,300},TileOutOfBoundsPolicy::Empty);
        map.fill_row(0,0,9,one_way_tile());map.fill_row(5,0,9,block_tile());world.set_tile_world(map);
        auto& player=add<PlatformPlayerCharacter>(elysia::core::Rect{90,344,34,56});
        truth("combat.bind_player",player.bind_combat(combat));
        before=[&,input](unsigned n){
            if(n==20)input->send(player,{Key::KeySpace});
            else if(n==150)input->send(player,{Key::KeyS,Key::KeySpace});
            else if(n==220){input->send(player,{Key::KeySpace});input->send(player,{});}
            else input->send(player,{});
        };
        after=[&](unsigned n){
            if(n==10)truth("platform.floor_grounded",combat.is_grounded(player));
            if(n==45)truth("platform.upward_passage",player.position().y<244);
            if(n==120){truth("platform.seam_grounded",combat.is_grounded(player));check("platform.height",player.position().y,244,3);}
            if(n==210){truth("platform.drop_all_tiles",player.position().y>330);truth("platform.floor_reland",combat.is_grounded(player));}
            if(n==220)truth("input.short_tap_latched",player.velocity().y< -400);
            if(n==221)truth("input.short_tap_consumed",player.velocity().y> -510);
            if(n==300)truth("platform.reland",combat.is_grounded(player));
        };
    }
    else
    {
        finish_at=300;
        auto& map=tile_map(12,10,{50,50});
        map.fill_row(0,0,11,block_tile());map.fill_row(9,0,11,block_tile());
        map.fill_column(0,0,9,block_tile());map.fill_column(11,0,9,block_tile());
        map.fill_column(6,1,8,block_tile());(void)map.set_cell({3,4},hazard_tile());world.set_tile_world(map);
        auto& player=add<TopDownPlayerCharacter>(elysia::core::Rect{100,100,34,40});
        auto& enemy=add<TopDownChaseEnemy>(elysia::core::Rect{450,100,36,42},player);
        truth("combat.bind_player",player.bind_combat(combat));truth("combat.bind_enemy",enemy.bind_combat(combat));
        auto health_outside=std::make_shared<int>(100);
        before=[&,input](unsigned n){
            if(n==31){map.fill_column(6,1,8,{});world.update_tiles({6,1},{6,8});}
            if(n==61){world.set_body_enabled(enemy.physics_handle(),false);enemy.set_active(false);
                world.teleport_object(player.physics_handle(),{100,200},TeleportVelocityMode::Clear);}
            if(n>=61 && n<=100)input->send(player,{Key::KeyD});
            else if(n>=161)input->send(player,{Key::KeyD});
            else input->send(player,{});
        };
        after=[&,health_outside](unsigned n){
            if(n==30){check("sight.blocked_enemy",enemy.position().x,450,.1);
                truth("sight.blocking_query",world.segment_cast({player.center(),enemy.center(),{collision_layers::Body,collision_layers::World,0}}).has_value());}
            if(n==60)truth("sight.enemy_chases",enemy.position().x<420);
            if(n==100){truth("hazard.entry_damage",player.health().current()<100);*health_outside=player.health().current();}
            if(n==160)check("hazard.exit_stops_damage",player.health().current(),*health_outside);
            if(n==300){check("world.wall_position",player.position().x,516,3);
                truth("world.wall_blocks",world.contact_state(player.physics_handle()).wall_right);}
        };
        view={0,0,600,500};
    }
}
}
