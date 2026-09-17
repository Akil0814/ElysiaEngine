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
    for (auto player : local_players().players())
        if (player != PrimaryLocalPlayer)
        {
            _second_player = player;
            break;
        }
    if (!_first)
    {
        _first = create_and_add_object<PlayerBlock>(Rect{-150, 0, 48, 48}, elysia::core::colors::blue_500);
        _second = create_and_add_object<PlayerBlock>(Rect{150, 0, 48, 48}, elysia::core::colors::red_500);
        _window = create_and_add_object<UiWindow>(Rect{0, 0, 1280, 720}, 100);
        _window->set_style_overrides({.draw_background = false, .draw_border = false});
        auto instructions = std::make_unique<UiLabel>(
            Rect{0, 0, 1248, 28}, 0,
            ui_raw_text("WASD: player 1 | Unassigned pad Start: join player 2 | Left stick / D-pad: move"));
        _window->add_child(std::move(instructions), {._margin = {16, 12, 0, 0}});
        auto status = std::make_unique<UiLabel>(Rect{0, 0, 1248, 28});
        _status = status.get();
        _window->add_child(std::move(status), {._margin = {16, 44, 0, 0}});
        auto open = std::make_unique<UiButton>(Rect{16, 80, 200, 40});
        open->set_text_content(ui_raw_text("Players / menu"));
        open->set_on_click([this] { open_menu(); });
        _window->add_child(std::move(open), {._margin = {16, 80, 0, 0}});
        auto menu = std::make_unique<UiListContainer>(Rect{0, 0, 520, 450});
        _menu = menu.get();
        _menu->set_item_spacing(8);
        auto add = [&](const char *title, auto callback) {
            auto b = std::make_unique<UiButton>(Rect{0, 0, 500, 42});
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
                        local_players().unbind_source(source);
            set_ui_owner(PrimaryLocalPlayer);
            close_menu();
        });
        add("UI owner: player 1", [this] { set_ui_owner(PrimaryLocalPlayer); });
        add("UI owner: player 2", [this] {
            if (_second_player.value)
                set_ui_owner(_second_player);
        });
        auto text = std::make_unique<UiTextInput>(Rect{0, 0, 500, 42});
        text->set_placeholder_content(ui_raw_text("Type here: typing must not move players"));
        _menu->add_back(std::move(text));
        add("Resume", [this] { close_menu(); });
        add("Back to gallery", [this] { request_scene_switch(_return_route); });
        _window->add_child(std::move(menu), {._anchor = elysia::ui::UiLayoutAnchor::Center});
        (void)_window->register_overlay(*_menu, {.open = false, .modal = true});
    }
    (void)elysia::gameplay::ControllerService::instance()->bind_target(example::input::session_player(PrimaryLocalPlayer), control_context(), *_first);
    if (_second_player.value && local_players().contains(_second_player))
        (void)elysia::gameplay::ControllerService::instance()->bind_target(example::input::session_player(_second_player), control_context(), *_second);
    _window->set_visible(true);
    _window->set_active(true);
    elysia::camera::CameraManager::instance()->set_center(elysia::camera::CameraSlot::Main, {0, 0});
}
void LocalMultiplayerScene::on_exit()
{
    close_menu();
    if (_window)
    {
        _window->set_visible(false);
        _window->set_active(false);
    }
}
void LocalMultiplayerScene::open_menu()
{
    _window->open_overlay(*_menu);
    set_all_gameplay_input_blocked(true);
}
void LocalMultiplayerScene::close_menu()
{
    if (_window && _menu)
        _window->close_overlay(*_menu);
    set_all_gameplay_input_blocked(false);
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
    (void)elysia::gameplay::ControllerService::instance()->bind_target(example::input::session_player(player), control_context(), player == PrimaryLocalPlayer ? *_first : *_second);
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
        set_all_gameplay_input_blocked(false);
    GameplayScene::on_update(dt);
    std::ostringstream out;
    for (auto player : {PrimaryLocalPlayer, _second_player})
        if (player.value)
        {
            out << "P" << player.value << (player == PrimaryLocalPlayer ? " -> blue [" : " -> red [");
            auto sources = local_players().sources(player);
            if (sources.empty())
                out << "disconnected";
            for (auto source : sources)
                out << (source.is_gamepad() ? "pad " : "keyboard ") << source.value << " ";
            out << "] ";
        }
    out << "UI: P" << ui_owner().value;
    if (_assign_to.value)
        out << " | Press Start on an unassigned pad for P" << _assign_to.value;
    _status->set_text_content(ui_raw_text(out.str()));
}
} // namespace example::scene
