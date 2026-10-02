#include "game/showcase/gameplay/gameplay_demo_scene_base.h"
namespace example::scene {
void GameplayDemoSceneBase::build_controls() {
    _hud_view.build_controls(*_hud,{[this]{toggle_pause();},[this]{single_step();},[this]{request_restart();},[this]{toggle_world_ui();},[this]{show_speech();},[this]{return_to_caller();}});
}
void GameplayDemoSceneBase::toggle_pause() { _demo_paused=!_demo_paused; if(_demo_paused)pause();else resume(); }
void GameplayDemoSceneBase::single_step() { if(!_demo_paused)toggle_pause();_single_step=true; }
void GameplayDemoSceneBase::toggle_world_ui() { _world_ui_visible=!_world_ui_visible;for(auto* actor:_actors)if(actor && !actor->is_destroyed())actor->set_world_ui_visible(_world_ui_visible); }
void GameplayDemoSceneBase::show_speech() { if(_player && _player->alive())_player->show_speech(); }
}
