#include "../../input/local_controls.h"
#include "../../input/command_view.h"
#include "local_multiplayer_scene.h"
#include "demo_scene_payload.h"
#include <stdexcept>
#include "../example_scene_keys.h"
#include "../../demo/physics/colored_block_object.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/widgets/ui_text_input.h"
#include "../../../engine/core/render/colors.h"
#include <sstream>
namespace example::scene
{
using namespace elysia::input;
using namespace elysia::ui;
using elysia::core::Rect;
namespace
{
class PlayerBlock final : public example::demo::physics::ColoredBlockObject,
                          public elysia::gameplay::ControlCommandReceiver
{
  public:
    PlayerBlock(Rect rect, elysia::core::Color color)
        : ColoredBlockObject(elysia::core::DepthLayer::Character, rect, color)
    {
    }
    void on_control_command(const elysia::gameplay::ControlCommand &command, double dt) override
    {
        auto move = example::input::CommandView(command).move();
        if (move.length_squared() > 1)
            move.normalize_in_place();
        auto p = position() + move * static_cast<float>(220 * dt);
        p.x = std::clamp(p.x, -500.f, 500.f);
        p.y = std::clamp(p.y, -210.f, 210.f);
        set_position(p);
    }
    void on_control_cancelled(elysia::gameplay::InputCancelReason) override
    {
    }
};
} // namespace
void LocalMultiplayerScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    const auto *route = elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (!route || !elysia::scene::SceneKeys::is_supported(route->return_route.target))
        throw std::logic_error("LocalMultiplayerScene requires a valid DemoScenePayload return route.");
    _return_route = route->return_route;
    _assign_to = {};
    if (!local_players().contains(_second_player))
    {
        _second_player = {};
        for (auto player : local_players().players())
            if (player != PrimaryLocalPlayer)
            {
                _second_player = player;
                break;
            }
        if (!_second_player.value)
            _second_player = local_players().create_player();
    }
    _first_controller = example::input::session_player(PrimaryLocalPlayer);
    _second_controller = example::input::session_player(_second_player);
    if (!_saved_devices)
    {
        _saved_devices = local_players().configuration();
        _wasd = *local_players().create_partition(
            "WASD", example::input::keyboard_keys(example::input::KeyboardScheme::Wasd));
        _arrows = *local_players().create_partition(
            "Arrows", example::input::keyboard_keys(example::input::KeyboardScheme::Arrows));
    }
    if (!_first)
    {
        _first = create_and_add_object<PlayerBlock>(Rect{-150, 0, 48, 48}, elysia::core::colors::blue_500);
        _second = create_and_add_object<PlayerBlock>(Rect{150, 0, 48, 48}, elysia::core::colors::red_500);
        _window = create_and_add_object<UiWindow>(Rect{0, 0, 1280, 720}, 100);
        _window->set_style_overrides({.draw_background = false, .draw_border = false});
        auto instructions = std::make_unique<UiLabel>(
            Rect{0, 0, 1248, 28}, 0,
            ui_raw_text("WASD: P1 | Arrows: P2 | Unassigned pad Start: add to P2 | Mouse: independent"));
        _window->add_child(std::move(instructions), {._margin = {16, 12, 0, 0}});
        auto status = std::make_unique<UiLabel>(Rect{0, 0, 1248, 28});
        _status = status.get();
        _window->add_child(std::move(status), {._margin = {16, 44, 0, 0}});
        auto open = std::make_unique<UiButton>(Rect{16, 80, 200, 40});
        open->set_text_content(ui_raw_text("Players / menu"));
        open->set_on_click([this] { open_menu(); });
        _window->add_child(std::move(open), {._margin = {16, 80, 0, 0}});
        auto menu = std::make_unique<UiListContainer>(Rect{0, 0, 600, 540});
        _menu = menu.get();
        _menu->set_item_spacing(6);
        auto add = [&](const char *title, auto callback) {
            auto b = std::make_unique<UiButton>(Rect{0, 0, 580, 38});
            b->set_text_content(ui_raw_text(title));
            b->set_on_click(callback);
            _menu->add_back(std::move(b));
        };
        add("Bind next Start controller to player 1", [this] {
            _assign_to = PrimaryLocalPlayer;
            close_menu();
        });
        add("Rebind player 2 controller", [this] {
            if (!_second_player.value)
                _second_player = local_players().create_player();
            _assign_to = _second_player;
            close_menu();
        });
        add("Release controllers for reassignment", [this] {
            for (auto player : local_players().players())
                for (auto source : local_players().sources(player))
                    if (source.is_gamepad())
                        if (!local_players().unbind_source(source)) throw std::logic_error("Cannot unbind input source");
            close_menu();
        });
        add("Keyboard: WASD P1 / Arrows P2", [this] { configure_keyboard(false); });
        add("Keyboard: Arrows P1 / WASD P2", [this] { configure_keyboard(true); });
        add("Mouse: player 1",
            [this] { if (!local_players().transfer_source(PrimaryLocalPlayer, InputSourceId::mouse())) throw std::logic_error("Cannot transfer mouse"); });
        add("Mouse: player 2",
            [this] { if (!local_players().transfer_source(_second_player, InputSourceId::mouse())) throw std::logic_error("Cannot transfer mouse"); });
        add("UI pad: player 1's gamepad", [this] {
            set_ui_gamepad(local_players().configuration().bindings.at(PrimaryLocalPlayer).gamepad);
        });
        add("UI pad: player 2's gamepad",
            [this] { set_ui_gamepad(local_players().configuration().bindings.at(_second_player).gamepad); });
        auto text = std::make_unique<UiTextInput>(Rect{0, 0, 580, 38});
        text->set_placeholder_content(ui_raw_text("Type here: typing must not move players"));
        _menu->add_back(std::move(text));
        add("Resume", [this] { close_menu(); });
        add("Back to gallery", [this] { request_scene_switch(_return_route); });
        _window->add_child(std::move(menu), {._anchor = elysia::ui::UiLayoutAnchor::Center});
        (void)_window->register_overlay(*_menu, {.open = false, .modal = true});
    }
    if (!local_players().transfer_source(PrimaryLocalPlayer, InputSourceId::mouse()))
        throw std::logic_error("Multiplayer mouse assignment failed");
    configure_keyboard(_swapped);
    _window->set_visible(true);
    _window->set_active(true);
    elysia::camera::CameraManager::instance()->set_center(elysia::camera::CameraSlot::Main, {0, 0});
}
void LocalMultiplayerScene::bind_players()
{
    auto *service = elysia::gameplay::ControllerService::instance();
    for (auto player : {PrimaryLocalPlayer, _second_player})
        if (!service->bind_target((player == PrimaryLocalPlayer ? _first_controller : _second_controller),
                                  control_context(), player == PrimaryLocalPlayer ? *_first : *_second).succeeded())
            throw std::logic_error("Multiplayer controller binding failed");
}
void LocalMultiplayerScene::configure_keyboard(bool swapped)
{
    auto *service = elysia::gameplay::ControllerService::instance();
    for (auto player : {PrimaryLocalPlayer, _second_player})
        if (!service->unbind_target((player == PrimaryLocalPlayer ? _first_controller : _second_controller)).succeeded())
            throw std::logic_error("Multiplayer configuration requires completed unbinding");
    auto next = local_players().configuration();
    for (auto &[player, binding] : next.bindings)
        binding.keyboard = {};
    next.bindings[PrimaryLocalPlayer].keyboard = swapped ? _arrows : _wasd;
    next.bindings[_second_player].keyboard = swapped ? _wasd : _arrows;
    if (!local_players().apply_configuration(std::move(next)))
        throw std::logic_error("Invalid multiplayer partitions");
    _swapped = swapped;
    for (auto player : {PrimaryLocalPlayer, _second_player})
    {
        bool wasd = (player == PrimaryLocalPlayer) != swapped;
        if (!service->replace_input_map(
                (player == PrimaryLocalPlayer ? _first_controller : _second_controller),
                example::input::make_gameplay_input_map(
                    {wasd ? example::input::KeyboardScheme::Wasd : example::input::KeyboardScheme::Arrows,
                     true, true})).succeeded())
            throw std::logic_error("Invalid multiplayer mapping");
    }
    bind_players();
}
void LocalMultiplayerScene::restore_devices()
{
    if (!_saved_devices)
        return;
    auto *service = elysia::gameplay::ControllerService::instance();
    for (auto handle : {_first_controller, _second_controller})
        if (service->get(handle) && !service->unbind_target(handle).succeeded())
            throw std::logic_error("Multiplayer configuration requires completed unbinding");
    auto saved = *_saved_devices;
    std::erase_if(saved.bindings, [&](const auto &entry) { return !local_players().contains(entry.first); });
    for (auto player : local_players().players())
        saved.bindings[player].gamepad = local_players().configuration().bindings.at(player).gamepad;
    if (!local_players().apply_configuration(std::move(saved)))
        throw std::logic_error("Cannot restore input configuration");
    _saved_devices.reset();
    _wasd = {};
    _arrows = {};
}
void LocalMultiplayerScene::on_exit()
{
    close_menu();
    restore_devices();
    if (_window)
    {
        _window->set_visible(false);
        _window->set_active(false);
    }
}
void LocalMultiplayerScene::open_menu()
{
    set_ui_interaction_mode(UiInteractionMode::Navigation);
    _window->open_overlay(*_menu);
    set_all_gameplay_input_blocked(true);
}
void LocalMultiplayerScene::close_menu()
{
    if (_window && _menu)
        _window->close_overlay(*_menu);
    set_all_gameplay_input_blocked(false);
    set_ui_interaction_mode(UiInteractionMode::Pointer);
}
bool LocalMultiplayerScene::on_unassigned_input(const RawInputEvent &event)
{
    if (event.type != RawInputEventType::ControlPressed || event.control != RawInputControl::GamepadStart)
        return false;
    if (!_assign_to.value)
    {
        if (!_second_player.value)
            _second_player = local_players().create_player();
        _assign_to = _second_player;
    }
    const auto player = _assign_to;
    if (!local_players().replace_gamepad(player, event.source))
        return false;
    bind_players();
    _assign_to = {};
    return true;
}
void LocalMultiplayerScene::on_shortcuts(const RawInputFrame &, const std::vector<RawInputEvent> &events)
{
    for (const auto &event : events)
        if (event.type == RawInputEventType::ControlPressed &&
            (event.control == RawInputControl::KeyEscape || event.control == RawInputControl::GamepadStart))
        {
            consume_input(event);
            open_menu();
            return;
        }
}
void LocalMultiplayerScene::on_update(double dt)
{
    if (_menu && !_window->is_overlay_open(*_menu))
    {
        set_all_gameplay_input_blocked(false);
        set_ui_interaction_mode(UiInteractionMode::Pointer);
    }
    GameplayScene::on_update(dt);
    std::ostringstream out;
    for (auto player : {PrimaryLocalPlayer, _second_player})
        if (player.value)
        {
            out << "P" << player.value << (player == PrimaryLocalPlayer ? " -> blue [" : " -> red [");
            const auto &binding = local_players().configuration().bindings.at(player);
            if (binding.keyboard.value)
                out << local_players().configuration().partitions.at(binding.keyboard).name << " ";
            if (binding.mouse)
                out << "mouse ";
            if (binding.gamepad.value)
                out << "pad " << binding.gamepad.value << " ";
            else
                out << "pad: disconnected ";
            out << "] ";
        }
    out << "UI: keyboard + mouse";
    if (ui_gamepad().value)
        out << " + pad " << ui_gamepad().value;
    if (_assign_to.value)
        out << " | Press Start on an unassigned pad for P" << _assign_to.value;
    _status->set_text_content(ui_raw_text(out.str()));
}
} // namespace example::scene
