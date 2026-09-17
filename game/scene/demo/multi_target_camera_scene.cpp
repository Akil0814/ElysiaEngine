#include "../../input/local_controls.h"
#include "../../input/command_view.h"
#include "multi_target_camera_scene.h"
#include "../../demo/physics/colored_block_object.h"
#include "../../../engine/ui/window/ui_window.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include <cmath>
#include <format>
#include <stdexcept>

namespace example::scene
{
using namespace elysia::core;
using namespace elysia::camera;
using namespace elysia::ui;
namespace
{
const elysia::input::InputActionId separation_action{"example.camera.separation"};
class CameraActor final : public example::demo::physics::ColoredBlockObject, public elysia::gameplay::ControlCommandReceiver {
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
void outline(SDL_Renderer* renderer, const Rect& rect, Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const SDL_FRect draw{rect.x(), rect.y(), rect.width(), rect.height()};
    SDL_RenderRect(renderer, &draw);
}
}

void MultiTargetCameraScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* demo = elysia::scene::try_scene_payload<DemoScenePayload>(payload);
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
    _controls->set_visible(true);
    _controls->set_active(true);
    reset_demo();
    static_cast<CameraActor*>(_targets[0])->other=static_cast<CameraActor*>(_targets[1]);
    static_cast<CameraActor*>(_targets[1])->other=static_cast<CameraActor*>(_targets[0]);
    using namespace elysia::input;
    auto map = example::input::make_default_gameplay_input_map();
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
    CameraManager::instance()->set_follow_strategy(slot, std::move(strategy));
}

void MultiTargetCameraScene::reset_demo()
{
    _primary = 0;
    if(elysia::gameplay::ControllerService::instance()->get(_controller))
        (void)elysia::gameplay::ControllerService::instance()->bind_target(_controller,control_context(),*_targets[0]);
    _time = 0;
    _automatic = _bounds = false;
    _dead_zone = true;
    _targets[0]->set_center({-120, 0});
    _targets[1]->set_center({120, 0});
    auto* cameras = CameraManager::instance();
    cameras->set_world_bounds(slot, std::nullopt);
    cameras->set_center(slot, {});
    cameras->set_zoom(slot, 1);
    install_strategy();
    refresh_status();
}

void MultiTargetCameraScene::toggle_bounds()
{
    _bounds = !_bounds;
    CameraManager::instance()->set_world_bounds(slot,
        _bounds ? std::optional(world_bounds) : std::nullopt);
}

void MultiTargetCameraScene::on_exit()
{
    _strategy = nullptr;
    if (_controls) { _controls->set_active(false); _controls->set_visible(false); }
}

void MultiTargetCameraScene::reset()
{
    on_exit();
    reset_input_routing();

    for (auto*& target : _targets) { if (target) target->destroy(); target = nullptr; }
    if (_controls) _controls->destroy();
    _controls = nullptr;
    _status = nullptr;
    _return_route = {};
}

std::optional<CameraFocus> MultiTargetCameraScene::resolve_camera_focus() const
{
    if (!_targets[0] || !_targets[1]) return std::nullopt;
    const std::array rects{_targets[0]->render_rect(), _targets[1]->render_rect()};
    return make_camera_focus(rects, _primary);
}

void MultiTargetCameraScene::on_update(double delta)
{
    Scene::on_update(delta);
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
    if (_status && _strategy)
        _status->set_text_content(ui_raw_text(std::format(
            "Primary: {} | Zoom: {:.2f} | {} | DeadZone: {} | Auto: {} | Bounds: {}",
            _primary == 0 ? "Blue" : "Orange", camera().zoom(),
            _strategy->primary_only() ? "Primary only" : "Group",
            _dead_zone ? "ON" : "OFF", _automatic ? "ON" : "OFF", _bounds ? "ON" : "OFF")));
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
    const float width = static_cast<float>(runtime_context().logical_width());
    _controls = create_and_add_object<UiWindow>(Rect{0, 0, width, 102});
    auto label = std::make_unique<UiLabel>(Rect{16, 4, width - 32, 30}, 0,
        ui_raw_text("Multi-target Camera | WASD: move primary | Q/E: move other | Esc: back"));
    _controls->add_child(std::move(label), {._margin = {16, 4, 0, 0}});
    auto status = std::make_unique<UiLabel>(Rect{16, 72, width - 32, 28});
    _status = status.get();
    _controls->add_child(std::move(status), {._margin = {16, 72, 0, 0}});
    float x = 16;
    auto button = [&](const char* text, auto action) {
        auto control = std::make_unique<UiButton>(Rect{x, 36, 132, 32});
        control->set_text_content(ui_raw_text(text));
        control->set_padding(4);
        control->set_on_click(action);
        _controls->add_child(std::move(control), {._margin = {x, 36, 0, 0}});
        x += 140;
    };
    button("DeadZone", [this] { _dead_zone = !_dead_zone; install_strategy(); });
    button("Swap primary", [this] { _primary = 1 - _primary; (void)elysia::gameplay::ControllerService::instance()->bind_target(_controller,control_context(),*_targets[_primary]); });
    button("Auto motion", [this] { _automatic = !_automatic; _time = 0; });
    button("Teleport", [this] { _targets[1 - _primary]->set_center(_targets[_primary]->center() + Vector2{3600, 900}); });
    button("Zoom to 1.5", [] { CameraManager::instance()->request_zoom_to(slot, 1.5f, 1.0); });
    button("World bounds", [this] { toggle_bounds(); });
    button("Reset", [this] { reset_demo(); });
    button("Back", [this] { return_to_caller(); });
}

void MultiTargetCameraScene::on_render(SDL_Renderer* renderer)
{
    Scene::on_render(renderer);
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    const auto viewport = camera().viewport_size();
    outline(renderer, Rect::from_center(viewport * 0.5f, viewport * 0.70f), {65, 180, 255});
    outline(renderer, Rect::from_center(viewport * 0.5f, viewport * 0.55f), {100, 210, 130});
    if (const auto focus = resolve_camera_focus())
    {
        outline(renderer, camera().world_to_screen(focus->bounds), {180, 180, 180});
        auto primary = camera().world_to_screen(focus->primary);
        primary = Rect::from_center(primary.center(), primary.size() + Vector2{8, 8});
        outline(renderer, primary, {255, 255, 255});
    }
    if (_bounds) outline(renderer, camera().world_to_screen(world_bounds), {240, 90, 90});
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}
}
