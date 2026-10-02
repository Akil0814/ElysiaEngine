#include "engine/localization/localization_service.h"
#include "game/input/local_controls.h"
#include "game/input/command_view.h"
#include "game/showcase/input/local_multiplayer_scene.h"
#include "game/showcase/shared/showcase_enter_payload.h"
#include <stdexcept>
#include "game/navigation/showcase_scene_keys.h"
#include "game/showcase/shared/colored_block_object.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_text_input.h"
#include "engine/core/render/colors.h"
#include "engine/tools/logger.h"
#include <sstream>
namespace example::scene
{
using namespace elysia::input;
using namespace elysia::ui;
using elysia::core::Rect;
namespace
{
class PlayerBlock final : public example::showcase::ColoredBlockObject,
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
LocalMultiplayerScene::LocalMultiplayerScene()
    : GameplayScene(elysia::gameplay::GameplaySceneFeatures{
          .camera = elysia::scene::CameraSceneConfig{}})
{}

void LocalMultiplayerScene::on_enter(const elysia::scene::ScenePayload &payload)
{
    const auto *route = elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!route || !elysia::scene::SceneKeys::is_supported(route->return_route.target))
        throw std::logic_error("LocalMultiplayerScene requires a valid ShowcaseEnterPayload return route.");
    auto previous_players = local_players();
    std::optional<elysia::gameplay::ControllerHandle> previous_first_controller;
    std::optional<elysia::gameplay::ControllerHandle> previous_second_controller;
    bool acquired_first_controller = false, acquired_second_controller = false;
    try
    {
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
    previous_first_controller = example::input::existing_session_player(PrimaryLocalPlayer);
    previous_second_controller = example::input::existing_session_player(_second_player);
    _first_controller = example::input::session_player(PrimaryLocalPlayer);
    acquired_first_controller = true;
    _second_controller = example::input::session_player(_second_player);
    acquired_second_controller = true;
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
        _view.build(*_window,{[this] {
            _assign_to = PrimaryLocalPlayer;
            close_menu();
        },
[this] {
            if (!_second_player.value)
                _second_player = local_players().create_player();
            _assign_to = _second_player;
            close_menu();
        },
[this] {
            for (auto player : local_players().players())
                for (auto source : local_players().sources(player))
                    if (source.is_gamepad())
                        if (!local_players().unbind_source(source)) throw std::logic_error("Cannot unbind input source");
            close_menu();
        },
[this] { configure_keyboard(false); },
[this] { configure_keyboard(true); },
[this] { if (!local_players().transfer_source(PrimaryLocalPlayer, InputSourceId::mouse())) throw std::logic_error("Cannot transfer mouse"); },
[this] { if (!local_players().transfer_source(_second_player, InputSourceId::mouse())) throw std::logic_error("Cannot transfer mouse"); },
[this] {
            set_ui_gamepad(local_players().configuration().bindings.at(PrimaryLocalPlayer).gamepad);
        },
[this] { set_ui_gamepad(local_players().configuration().bindings.at(_second_player).gamepad); },
[this] { close_menu(); },
[this] { request_scene_switch(_return_route); }},[this]{open_menu();},[this]{request_scene_switch(_return_route);});
    }
    if (!local_players().transfer_source(PrimaryLocalPlayer, InputSourceId::mouse()))
        throw std::logic_error("Multiplayer mouse assignment failed");
    configure_keyboard(_swapped);
    _window->set_visible(true);
    _window->set_active(true);
    camera_runtime().set_center(elysia::camera::CameraSlot::Main, {0, 0});
    }
    catch (...)
    {
        auto* service = elysia::gameplay::ControllerService::instance();
        auto cleanup = [](auto&& action) {
            try { action(); }
            catch (...)
            {
                elysia::tools::Logger::instance()->error("scene_cleanup",
                    "Multiplayer controller rollback failed after enter exception.");
            }
        };
        for (auto handle : {_first_controller, _second_controller})
            if (service->get(handle))
                cleanup([&] { (void)service->unbind_target(handle); });
        if (acquired_first_controller && !previous_first_controller)
            cleanup([&] { (void)service->remove(_first_controller); });
        if (acquired_second_controller && !previous_second_controller)
            cleanup([&] { (void)service->remove(_second_controller); });
        local_players().swap_state(previous_players);
        _saved_devices.reset();
        throw;
    }
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
    _window->open_overlay(*_view.menu);
    set_all_gameplay_input_blocked(true);
}
void LocalMultiplayerScene::close_menu()
{
    if (_window && _view.menu)
        _window->close_overlay(*_view.menu);
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
void LocalMultiplayerScene::on_before_update(double dt)
{
    (void)dt;
    if (_view.menu && !_window->is_overlay_open(*_view.menu))
    {
        set_all_gameplay_input_blocked(false);
        set_ui_interaction_mode(UiInteractionMode::Pointer);
    }
}

void LocalMultiplayerScene::on_after_update(double dt)
{
    (void)dt;
    auto tr=[](const char* key){return std::string(ELYSIA_LOCALIZATION->tr(key));};
    std::ostringstream out;
    for (auto player : {PrimaryLocalPlayer, _second_player})
        if (player.value)
        {
            out << "P" << player.value << (" -> "+tr(player==PrimaryLocalPlayer?"gameplay_ui_demo.player":"gameplay_ui_demo.enemy")+" [");
            const auto &binding = local_players().configuration().bindings.at(player);
            if (binding.keyboard.value)
                out << local_players().configuration().partitions.at(binding.keyboard).name << " ";
            if (binding.mouse)
                out << tr("showcase.input.mouse") << " ";
            if (binding.gamepad.value)
                out << tr("showcase.input.pad") << " " << binding.gamepad.value << " ";
            else
                out << tr("showcase.input.disconnected") << " ";
            out << "] ";
        }
    out << tr("showcase.input.ui_access");
    if (ui_gamepad().value)
        out << " + " << tr("showcase.input.pad") << " " << ui_gamepad().value;
    if (_assign_to.value)
        out << " | " << tr("showcase.input.assignment") << " " << _assign_to.value;
    _view.status->set_text_content(ui_raw_text(out.str()));
}
} // namespace example::scene
