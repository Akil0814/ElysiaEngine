#include "engine/localization/localization_service.h"
#include "game/input/local_controls.h"
#include "game/input/command_view.h"
#include "game/showcase/camera/multi_target_camera_scene.h"
#include "game/showcase/shared/colored_block_object.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/tools/logger.h"
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>

namespace example::scene
{
MultiTargetCameraScene::MultiTargetCameraScene()
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .camera = elysia::scene::CameraSceneConfig{
              .initial_slot = elysia::camera::CameraSlot::Main,
              .owned_slots = elysia::camera::CameraSlot::Main,
              .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}})
{}

using namespace elysia::core;
using namespace elysia::camera;
using namespace elysia::ui;
namespace
{
const elysia::input::InputActionId separation_action{"example.camera.separation"};
class CameraActor final : public example::showcase::ColoredBlockObject, public elysia::gameplay::ControlCommandReceiver {
public:
    using ColoredBlockObject::ColoredBlockObject;
    CameraActor* other = nullptr;
    void on_control_command(const elysia::gameplay::ControlCommand& command, double delta) override {
        auto movement=example::input::CommandView(command).move();
        if(movement.length_squared()>1) movement.normalize_in_place();
        set_center(center()+movement*static_cast<float>(320*delta));
        if(other && !other->is_destroyed()) other->set_center(other->center()+Vector2{command.state.axis1d(separation_action)*static_cast<float>(700*delta),0});
    }
    void on_control_cancelled(elysia::gameplay::InputCancelReason) override {}
};
constexpr auto slot = CameraSlot::Main;
const Rect world_bounds{-900, -700, 1800, 1400};
}

void MultiTargetCameraScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* demo = elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!demo || !elysia::scene::SceneKeys::is_supported(demo->return_route.target))
        throw std::logic_error("MultiTargetCameraScene requires a valid demo return route.");
    _return_route = demo->return_route;
    _paused = false;
    if (!_targets[0])
    {
        _targets[0] = create_and_add_object<CameraActor>(
            DepthLayer::Item, Rect{-140, -20, 40, 40}, Color{65, 180, 255});
        _targets[1] = create_and_add_object<CameraActor>(
            DepthLayer::Item, Rect{100, -20, 40, 40}, Color{255, 165, 65});
    }
    if (!_controls) build_controls();
    if (!_overlay) _overlay = create_and_add_object<example::showcase::camera::CameraOverlayView>([this]{
        example::showcase::camera::CameraOverlayData data{.viewport=camera().viewport_size()};
        if(const auto focus=resolve_camera_focus(CameraSlot::Main)){data.focus=camera().world_to_screen(focus->bounds);data.primary=camera().world_to_screen(focus->primary);}
        if(_bounds)data.bounds=camera().world_to_screen(world_bounds);
        return data;
    });
    _overlay->set_visible(true);
    _controls->set_visible(true);
    _controls->set_active(true);
    reset_demo();
    static_cast<CameraActor*>(_targets[0])->other=static_cast<CameraActor*>(_targets[1]);
    static_cast<CameraActor*>(_targets[1])->other=static_cast<CameraActor*>(_targets[0]);
    using namespace elysia::input;
    auto map = example::input::make_gameplay_input_map();
    if (!map.contains(separation_action))
        (void)map.register_action(
            {separation_action, InputActionValueType::Axis1D},
            {{separation_action, ButtonInputBinding{RawInputControl::KeyQ, InputActionComponent::X, -1}},
             {separation_action, ButtonInputBinding{RawInputControl::KeyE, InputActionComponent::X, 1}}});
    example::input::configure_scene_player(*this,_controller,*_targets[_primary],PrimaryLocalPlayer,std::move(map));
}

void MultiTargetCameraScene::install_strategy()
{
    auto strategy = std::make_unique<MultiTargetFollowStrategy>(
        MultiTargetFollowConfig{.dead_zone_enabled = _dead_zone});
    _strategy = strategy.get();
    camera_runtime().set_follow_strategy(slot, std::move(strategy));
}

void MultiTargetCameraScene::request_primary(std::size_t index)
{
    if (!_targets[index]) return;
    _requested_primary = index;
    _primary_request = elysia::gameplay::ControllerService::instance()->bind_target(
        _controller, control_context(), *_targets[index]);
    finish_primary_request();
}
void MultiTargetCameraScene::finish_primary_request()
{
    if (!_primary_request || _primary_request->pending()) return;
    if (_primary_request->succeeded()) _primary = _requested_primary;
    else elysia::tools::Logger::instance()->warn("input", "Camera target switch failed; primary unchanged.");
    _primary_request.reset();
}

void MultiTargetCameraScene::reset_demo()
{
    if (elysia::gameplay::ControllerService::instance()->get(_controller)) request_primary(0);
    else _primary = 0;
    _time = 0;
    _automatic = _bounds = false;
    _dead_zone = true;
    _targets[0]->set_center({-120, 0});
    _targets[1]->set_center({120, 0});
    camera_runtime().set_world_bounds(slot, std::nullopt);
    camera_runtime().set_center(slot, {});
    camera_runtime().set_zoom(slot, 1);
    install_strategy();
    refresh_status();
}

void MultiTargetCameraScene::toggle_bounds()
{
    _bounds = !_bounds;
    camera_runtime().set_world_bounds(slot,
        _bounds ? std::optional(world_bounds) : std::nullopt);
}

void MultiTargetCameraScene::on_exit()
{
    if (_overlay) _overlay->set_visible(false);
    _strategy = nullptr;
    if (_controls) { _controls->set_active(false); _controls->set_visible(false); }
}

void MultiTargetCameraScene::on_reset()
{
    on_exit();

    for (auto*& target : _targets) { if (target) target->destroy(); target = nullptr; }
    if (_controls) _controls->destroy();
    _controls = nullptr;
    if (_overlay) _overlay->destroy();
    _overlay = nullptr;
    _view.clear();
    _return_route = {};
}

std::optional<CameraFocus> MultiTargetCameraScene::resolve_camera_focus(CameraSlot slot) const
{
    if (slot != CameraSlot::Main)
        return std::nullopt;
    if (!_targets[0] || !_targets[1]) return std::nullopt;
    const std::array rects{_targets[0]->render_rect(), _targets[1]->render_rect()};
    return make_camera_focus(rects, _primary);
}

void MultiTargetCameraScene::on_after_update(double delta)
{
    (void)delta;
    finish_primary_request();
    refresh_status();
}
void MultiTargetCameraScene::on_control_target_removing(elysia::core::SceneObject& object) {
    for(auto*& target:_targets) if(target==&object) target=nullptr;
    for(auto* target:_targets) if(target && static_cast<CameraActor*>(target)->other==&object) static_cast<CameraActor*>(target)->other=nullptr;
}
void MultiTargetCameraScene::on_game_fixed_update(std::uint64_t, double delta) {
    if(_automatic && _targets[0] && _targets[1]) {
        _time+=delta;
        _targets[1-_primary]->set_center(_targets[_primary]->center()+Vector2{
            static_cast<float>(240+1900*(1-std::cos(_time*0.45))),static_cast<float>(180*std::sin(_time*0.45))});
    }
}

void MultiTargetCameraScene::refresh_status()
{
    if (!_strategy) return;
    auto tr=[](const char* key){return std::string(ELYSIA_LOCALIZATION->tr(key));};
    auto state=[&](bool enabled){return tr(enabled?"physics_tests.on":"physics_tests.off");};
    _view.status(ui_raw_text(std::format("{}: {} | {}: {:.2f} | {} | {}: {} | {}: {} | {}: {}",
        tr("showcase.camera.primary"),tr(_primary==0?"showcase.camera.blue":"showcase.camera.orange"),tr("showcase.camera.zoom_label"),camera().zoom(),
        tr(_strategy->primary_only()?"showcase.camera.primary_only":"showcase.camera.group"),tr("showcase.camera.dead_zone"),state(_dead_zone),
        tr("showcase.camera.auto_motion"),state(_automatic),tr("showcase.camera.bounds"),state(_bounds))));
}

void MultiTargetCameraScene::on_shortcuts(const elysia::input::RawInputFrame &input,
                                          const std::vector<elysia::input::RawInputEvent> &events)
{

    using C = elysia::input::RawInputControl;
    for (const auto& event : events)
        if (event.control == C::KeyEscape && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            consume_input(event);
            return_to_caller();
        }
}

void MultiTargetCameraScene::return_to_caller() { request_scene_switch(_return_route); }

void MultiTargetCameraScene::build_controls()
{
    _controls=create_and_add_object<UiWindow>(Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())});
    _controls->set_style_overrides({.draw_background=false,.draw_border=false});
    _view.build(*_controls,{[this]{_dead_zone=!_dead_zone;install_strategy();},[this]{request_primary(1-_primary);},[this]{_automatic=!_automatic;_time=0;},
        [this]{_targets[1-_primary]->set_center(_targets[_primary]->center()+Vector2{3600,900});},
        [this]{(void)camera_runtime().move_to(slot,{.zoom=1.5f},1.0);},[this]{toggle_bounds();},[this]{reset_demo();}},[this]{return_to_caller();});
}
}
