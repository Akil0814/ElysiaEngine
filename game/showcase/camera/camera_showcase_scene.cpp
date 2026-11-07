#include "camera_showcase_scene.h"
#include "game/gameplay/control/local_controls.h"
#include "game/gameplay/control/command_view.h"
#include "game/showcase/shared/colored_block_object.h"
#include "engine/localization/localization_service.h"
#include "engine/tools/logger.h"
#include <cmath>
#include <format>
#include <stdexcept>
namespace example::scene {
using namespace elysia::core;
using namespace elysia::camera;
using namespace example::showcase::camera;
namespace {
const elysia::input::InputActionId separation_action{"example.camera.separation"};
const Rect world_bounds{-900,-700,1800,1400};
class CameraActor final : public example::showcase::ColoredBlockObject,public elysia::gameplay::ControlCommandReceiver {
public:
    using ColoredBlockObject::ColoredBlockObject;
    CameraActor* other=nullptr;
    bool frozen=false;
    void on_control_command(const elysia::gameplay::ControlCommand& command,double delta) override {
        if(frozen)return;
        auto movement=example::gameplay::CommandView(command).move();
        if(movement.length_squared()>1)movement.normalize_in_place();
        set_center(center()+movement*static_cast<float>(320*delta));
        if(other && !other->is_destroyed() && !other->frozen)
            other->set_center(other->center()+Vector2{command.state.axis1d(separation_action)*static_cast<float>(700*delta),0});
    }
    void on_control_cancelled(elysia::gameplay::InputCancelReason) override {}
};
std::string tr(const char* key){return std::string(ELYSIA_LOCALIZATION->tr(key));}
}
CameraShowcaseScene::CameraShowcaseScene():GameplayScene(elysia::gameplay::GameplaySceneFeatures{
    .camera=elysia::scene::CameraSceneConfig{.initial_slot=CameraSlot::Main,
    .owned_slots=CameraSlot::Main|CameraSlot::Cinematic,.focus_mode=elysia::scene::CameraFocusMode::ResolveEachFrame}}) {}
void CameraShowcaseScene::on_enter(const elysia::scene::ScenePayload& payload) {
    const auto* enter=elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if(!enter || !elysia::scene::SceneKeys::is_supported(enter->return_route.target))
        throw std::logic_error("CameraShowcaseScene requires a valid showcase return route.");
    try {
        set_ui_interaction_mode(elysia::scene::UiInteractionMode::Navigation);
        _return_route=enter->return_route;_paused=false;
        for(std::size_t i=0;i<2;++i)if(!_targets[i])_targets[i]=create_and_add_object<CameraActor>(DepthLayer::Item,
            Rect{0,0,40,40},i==0?Color{65,180,255}:Color{255,165,65});
        static_cast<CameraActor*>(_targets[0])->other=static_cast<CameraActor*>(_targets[1]);
        static_cast<CameraActor*>(_targets[1])->other=static_cast<CameraActor*>(_targets[0]);
        _state={};reset_page();build_controls();
        _overlay=create_and_add_object<CameraOverlayView>([this] {
            CameraOverlayData data{.camera=ELYSIA_CAMERA->camera().value(),.mode=_state.strategy,.dead_zone=_state.dead_zone};
            if(const auto focus=resolve_camera_focus(CameraSlot::Main)){data.focus=ELYSIA_CAMERA->camera().value().world_to_screen(focus->bounds);data.primary=ELYSIA_CAMERA->camera().value().world_to_screen(focus->primary);}
            if(_state.bounds)data.bounds=ELYSIA_CAMERA->camera().value().world_to_screen(world_bounds);
            for(std::size_t i=0;i<_state.node_count;++i)data.nodes.push_back(ELYSIA_CAMERA->camera().value().world_to_screen(_state.nodes[i]));
            return data;
        });
        using namespace elysia::input;
        auto map=example::input::make_gameplay_input_map();
        (void)map.register_action({separation_action,InputActionValueType::Axis1D},
            {{separation_action,ButtonInputBinding{RawInputControl::KeyQ,InputActionComponent::X,-1}},
             {separation_action,ButtonInputBinding{RawInputControl::KeyE,InputActionComponent::X,1}}});
        example::gameplay::configure_scene_player(*this,_controller,*_targets[_state.primary],PrimaryLocalPlayer,std::move(map));
        refresh_status();
    } catch(...) {on_exit();throw;}
}
void CameraShowcaseScene::freeze(bool value) {
    if(value && !_state.frozen){_state.saved_automatic=_state.automatic;_state.automatic=false;}
    if(!value && _state.frozen)_state.automatic=_state.saved_automatic;
    _state.frozen=value;
    for(auto* target:_targets)if(target)static_cast<CameraActor*>(target)->frozen=value;
}
void CameraShowcaseScene::cleanup_activity() {
    _cinematic.clear();
    if(_state.motion)(void)ELYSIA_CAMERA->cancel_motion(*_state.motion);
    if(_state.blend)(void)ELYSIA_CAMERA->cancel_blend(*_state.blend);
    ELYSIA_CAMERA->clear_effects(CameraSlot::Main).value();ELYSIA_CAMERA->clear_effects(CameraSlot::Cinematic).value();
    _state.motion.reset();_state.blend.reset();_state.paused=false;_state.phase=CinematicPhase::Idle;
    freeze(false);
}
void CameraShowcaseScene::reset_page() {
    cleanup_activity();camera_runtime().reset();_strategy=nullptr;
    const auto page=_state.page;_state={};_state.page=page;
    if(page!=CameraPage::Follow)_state.strategy=FollowMode::Smooth;
    for(std::size_t i=0;i<2;++i)if(_targets[i])_targets[i]->set_center({i==0?-120.0f:120.0f,0});
    if(elysia::gameplay::ControllerService::instance()->get(_controller))request_primary(0);
    install_strategy();refresh_status();
}
void CameraShowcaseScene::select_page(CameraPage page) {
    if(static_cast<std::size_t>(page)>2 || page==_state.page)return;
    const bool automatic=_state.frozen?_state.saved_automatic:_state.automatic;
    _state.page=page;reset_page();
    // Keep the world demonstration running until the cinematic takes over.
    if(page==CameraPage::Cinematic)_state.automatic=automatic;
    refresh_status();
}
void CameraShowcaseScene::install_strategy() {
    _strategy=nullptr;
    std::unique_ptr<IFollowStrategy> strategy;
    switch(_state.strategy) {
    case FollowMode::Hard:strategy=std::make_unique<HardFollowStrategy>();break;
    case FollowMode::Smooth:strategy=std::make_unique<SmoothFollowStrategy>(480);break;
    case FollowMode::DeadZone:strategy=std::make_unique<DeadZoneFollowStrategy>(Rect::from_center(ELYSIA_CAMERA->camera().value().viewport_size()*.5f,ELYSIA_CAMERA->camera().value().viewport_size()*.4f));break;
    case FollowMode::MultiTarget: {
        auto multi=std::make_unique<MultiTargetFollowStrategy>(MultiTargetFollowConfig{.dead_zone_enabled=_state.dead_zone});
        _strategy=multi.get();strategy=std::move(multi);break;
    }}
    ELYSIA_CAMERA->set_follow_strategy(CameraSlot::Main,std::move(strategy)).value();
}
void CameraShowcaseScene::request_primary(std::size_t index) {
    if(!_targets[index])return;
    _requested_primary=index;
    _primary_request=elysia::gameplay::ControllerService::instance()->bind_target(_controller,control_context(),*_targets[index]);
    finish_primary_request();
}
void CameraShowcaseScene::finish_primary_request() {
    if(!_primary_request || _primary_request->pending())return;
    if(_primary_request->succeeded())_state.primary=_requested_primary;
    else elysia::tools::Logger::instance()->warn("input","Camera target switch failed; primary unchanged.");
    _primary_request.reset();
}
void CameraShowcaseScene::on_exit() {
    // Reset synchronously: exit and failed-entry cleanup must not allocate a
    // deferred shake-clear request or obscure the original entry failure.
    _cinematic.clear();camera_runtime().reset();_state.motion.reset();_state.blend.reset();
    _state.paused=false;_state.phase=CinematicPhase::Idle;freeze(false);
    _strategy=nullptr;_primary_request.reset();
    _view.clear();
    if(_controls)_controls->destroy();_controls=nullptr;
    if(_overlay)_overlay->destroy();_overlay=nullptr;
}
void CameraShowcaseScene::on_reset() {
    on_exit();for(auto*& target:_targets){if(target)target->destroy();target=nullptr;}_return_route={};_state={};
}
std::optional<CameraFocus> CameraShowcaseScene::resolve_camera_focus(CameraSlot slot) const {
    if(slot!=CameraSlot::Main || !_targets[_state.primary])return std::nullopt;
    if(_state.strategy==FollowMode::MultiTarget && _targets[0] && _targets[1]) {
        const std::array rects{_targets[0]->render_rect(),_targets[1]->render_rect()};return make_camera_focus(rects,_state.primary);
    }
    const std::array rects{_targets[_state.primary]->render_rect()};return make_camera_focus(rects,0);
}
void CameraShowcaseScene::on_control_target_removing(elysia::core::SceneObject& object) {
    for(auto*& target:_targets)if(target==&object)target=nullptr;
    for(auto* target:_targets)if(target && static_cast<CameraActor*>(target)->other==&object)static_cast<CameraActor*>(target)->other=nullptr;
}
void CameraShowcaseScene::on_game_fixed_update(std::uint64_t,double delta) {
    if(_state.automatic && !_state.frozen && _targets[0] && _targets[1]) {
        _state.time+=delta;
        _targets[1-_state.primary]->set_center(_targets[_state.primary]->center()+Vector2{
            static_cast<float>(240+1900*(1-std::cos(_state.time*.45))),static_cast<float>(180*std::sin(_state.time*.45))});
    }
}
void CameraShowcaseScene::perform(CameraAction action) {
    if(!camera_action_enabled(_state,action))return;
    using A=CameraAction;
    const auto slot=_state.page==CameraPage::Cinematic?ELYSIA_CAMERA->presented_slot().value():CameraSlot::Main;
    switch(action) {
    case A::Reset:reset_page();break;
    case A::Strategy:
        cleanup_activity();_state.strategy=static_cast<FollowMode>((static_cast<int>(_state.strategy)+1)%4);
        ELYSIA_CAMERA->set_zoom(CameraSlot::Main,1).value();install_strategy();break;
    case A::DeadZone:_state.dead_zone=!_state.dead_zone;install_strategy();break;
    case A::Swap:request_primary(1-_state.primary);break;
    case A::Automatic:_state.automatic=!_state.automatic;_state.time=0;break;
    case A::Teleport:if(_targets[0] && _targets[1])_targets[1-_state.primary]->set_center(_targets[_state.primary]->center()+Vector2{3600,900});break;
    case A::Bounds:_state.bounds=!_state.bounds;ELYSIA_CAMERA->set_world_bounds(CameraSlot::Main,_state.bounds?std::optional(world_bounds):std::nullopt).value();break;
    case A::Edge:
        _state.bounds=true;ELYSIA_CAMERA->set_world_bounds(CameraSlot::Main,world_bounds).value();
        if(_targets[0] && _targets[1]){_targets[_state.primary]->set_center({840,640});_targets[1-_state.primary]->set_center({700,520});}break;
    case A::Zoom:
        _state.motion=ELYSIA_CAMERA->move_to(CameraSlot::Main,{.zoom=1.5f},1).value();_state.feedback="showcase.camera.playing";break;
    case A::Move:case A::Path: {
        cleanup_activity();const auto start=ELYSIA_CAMERA->camera().value().center();
        _state.nodes[0]=start;_state.nodes[1]=start+Vector2{400,0};_state.node_count=2;
        if(action==A::Move)_state.motion=ELYSIA_CAMERA->move_to(slot,{.center=_state.nodes[1],.zoom=1.5f},1,_state.easing,_state.end).value();
        else {
            _state.nodes[2]=start+Vector2{400,240};_state.nodes[3]=start;_state.node_count=4;
            _state.motion=ELYSIA_CAMERA->play_path(slot,{{
                {{.center=_state.nodes[1],.zoom=1.2f},1,_state.easing},
                {{.center=_state.nodes[2],.zoom=1.5f},1,_state.easing},
                {{.center=start,.zoom=1.0f},1,_state.easing}},_state.end}).value();
        }
        _state.feedback="showcase.camera.playing";break;
    }
    case A::Easing:_state.easing=static_cast<CameraEasing>((static_cast<int>(_state.easing)+1)%3);break;
    case A::EndBehavior:_state.end=_state.end==CameraMotionEndBehavior::Hold?CameraMotionEndBehavior::ResumeFollow:CameraMotionEndBehavior::Hold;break;
    case A::Pause:
        if(_state.page==CameraPage::Cinematic){_cinematic.pause();_cinematic.sync(_state);}
        else {if(_state.motion)ELYSIA_CAMERA->pause_motion(*_state.motion).value();_state.paused=true;}break;
    case A::Resume:
        if(_state.page==CameraPage::Cinematic){_cinematic.resume();_cinematic.sync(_state);}
        else {if(_state.motion)ELYSIA_CAMERA->resume_motion(*_state.motion).value();_state.paused=false;}break;
    case A::Cancel:case A::Follow:
        if(_state.motion)(void)ELYSIA_CAMERA->cancel_motion(*_state.motion);
        _state.motion.reset();_state.paused=false;_state.feedback="showcase.camera.cancelled";break;
    case A::Shake:ELYSIA_CAMERA->request_shake(slot,{}).value();break;
    case A::ClearShake:ELYSIA_CAMERA->clear_effects(slot).value();break;
    case A::Cut:enter_cinematic(false,true);break;
    case A::Blend:enter_cinematic(false,false);break;
    case A::Play:case A::Replay:enter_cinematic(true,false);break;
    case A::Return:case A::Skip:return_main();break;
    default:break;
    }
    refresh_status();
}
void CameraShowcaseScene::enter_cinematic(bool full,bool cut) {
    cleanup_activity();
    _cinematic.start(_targets[_state.primary]?_targets[_state.primary]->center():Vector2{},full,cut);
    _cinematic.sync(_state);freeze(_cinematic.controls_locked());
}
void CameraShowcaseScene::return_main() {
    _cinematic.return_main();_cinematic.sync(_state);freeze(_cinematic.controls_locked());
}
void CameraShowcaseScene::on_after_update(double delta) {
    finish_primary_request();
    if(_state.page==CameraPage::Cinematic) {
        _cinematic.update(is_paused()?0:delta);
        _cinematic.sync(_state);freeze(_cinematic.controls_locked());
    } else if(_state.motion && _state.motion->reached_target()) {
        const bool holding=_state.motion->status()==CameraOperationStatus::Holding ||
            _state.motion->status()==CameraOperationStatus::Paused;
        _state.feedback=holding?"showcase.camera.holding":"showcase.camera.finished";
        if(!holding)_state.motion.reset();
    } else if(_state.motion && (_state.motion->status()==CameraOperationStatus::Cancelled ||
        _state.motion->status()==CameraOperationStatus::Superseded ||
        _state.motion->status()==CameraOperationStatus::SceneEnded)) {
        _state.motion.reset();_state.paused=false;_state.feedback="showcase.camera.cancelled";
    }
    refresh_status();
}
void CameraShowcaseScene::build_controls() {
    _controls=create_and_add_object<elysia::ui::UiWindow>(Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())});
    _controls->set_style_overrides({.draw_background=false,.draw_border=false});
    _view.build(*_controls,[this](CameraPage page){select_page(page);},[this](CameraAction action){perform(action);},[this]{return_to_caller();});
}
void CameraShowcaseScene::refresh_status() {
    static constexpr const char* modes[]={"showcase.camera.hard","showcase.camera.smooth","showcase.camera.single_dead_zone","showcase.camera.multi"};
    static constexpr const char* phases[]={"showcase.camera.ready","showcase.camera.entering","showcase.camera.touring","showcase.camera.holding","showcase.camera.returning","showcase.camera.manual"};
    static constexpr const char* easing[]={"showcase.camera.linear","showcase.camera.smoothstep","showcase.camera.cubic"};
    const auto pose=ELYSIA_CAMERA->camera().value();
    auto status=std::format("{} | {}: {} | {}: ({:.1f}, {:.1f}) | {}: {:.2f} | {}",
        tr(modes[static_cast<int>(_state.strategy)]),tr("showcase.camera.primary"),tr(_state.primary==0?"showcase.camera.blue":"showcase.camera.orange"),
        tr("showcase.camera.center"),pose.center().x,pose.center().y,tr("showcase.camera.zoom_label"),pose.zoom(),
        tr(_state.page==CameraPage::Cinematic?phases[static_cast<int>(_state.phase)]:_state.feedback));
    std::string detail;
    if(_state.page==CameraPage::Follow)detail=std::format("{} | {}: {} | {}: {} | {}: {} | {}",
        tr("showcase.camera.help_follow"),tr("showcase.camera.dead_zone"),tr(_state.dead_zone?"physics_tests.on":"physics_tests.off"),
        tr("showcase.camera.auto_motion"),tr(_state.automatic?"physics_tests.on":"physics_tests.off"),tr("showcase.camera.bounds"),tr(_state.bounds?"physics_tests.on":"physics_tests.off"),
        tr(!_strategy || _strategy->primary_only()?"showcase.camera.primary_only":"showcase.camera.group"));
    else if(_state.page==CameraPage::Motion)detail=std::format("{} | {} | {} | {}",tr("showcase.camera.help_motion"),tr(easing[static_cast<int>(_state.easing)]),
        tr(_state.end==CameraMotionEndBehavior::Hold?"showcase.camera.end_hold":"showcase.camera.end_follow"),tr(_state.paused?"showcase.camera.paused":"showcase.camera.active"));
    else detail=std::format("{} | {} | {}",tr("showcase.camera.help_cinematic"),
        _state.blend?(std::string(ELYSIA_CAMERA->presented_slot().value()==CameraSlot::Main?"Main":"Cinematic")+" → "+(_state.blend_target==CameraSlot::Main?"Main":"Cinematic")):
        std::string(ELYSIA_CAMERA->presented_slot().value()==CameraSlot::Main?"Main":"Cinematic"),tr(_state.paused?"showcase.camera.paused":"showcase.camera.active"));
    _view.update(_state,elysia::ui::ui_raw_text(std::move(status)),elysia::ui::ui_raw_text(std::move(detail)));
}
void CameraShowcaseScene::on_shortcuts(const elysia::input::RawInputFrame&,const std::vector<elysia::input::RawInputEvent>& events) {
    using C=elysia::input::RawInputControl;
    for(const auto& event:events)if(event.type==elysia::input::RawInputEventType::ControlPressed) {
        if(event.control==C::KeyEscape){consume_input(event);return_to_caller();return;}
        if(event.control==C::KeyR){consume_input(event);perform(CameraAction::Reset);}
        if(event.control==C::KeyF1 || event.control==C::KeyF2 || event.control==C::KeyF3) {
            consume_input(event);select_page(event.control==C::KeyF1?CameraPage::Follow:event.control==C::KeyF2?CameraPage::Motion:CameraPage::Cinematic);
        }
    }
}
void CameraShowcaseScene::return_to_caller(){request_scene_switch(_return_route);}
} // namespace example::scene
