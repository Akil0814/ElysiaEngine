#pragma once
#include "input_suppression.h"
#include "local_player_registry.h"
#include "../ui/input/ui_input_router.h"
#include <functional>
#include <map>
namespace elysia::scene
{
class Scene;
}
namespace elysia::input
{
class SceneInputRouter
{
  public:
    explicit SceneInputRouter(elysia::scene::Scene &scene) : _scene(scene)
    {
    }
    void route(const InputSnapshot &);
    void reset();
    void suppress(LocalPlayerId);
    void consume_input(const RawInputEvent &);
    const std::vector<RawInputEvent> &consumed_operations() const
    {
        return _consumed;
    }
    void set_ui_owner(LocalPlayerId);
    LocalPlayerId ui_owner() const
    {
        return _ui_owner;
    }
    void set_all_gameplay_input_blocked(bool);
    void set_menu_claim(bool enabled)
    {
        _claim_first_gamepad = enabled;
    }
    void set_cancel_handler(std::function<void(LocalPlayerId, InputCancelReason)> handler)
    {
        _cancel = std::move(handler);
    }

  private:
    void cancel(LocalPlayerId player, InputCancelReason reason = InputCancelReason::Suppressed)
    {
        if (_cancel)
            _cancel(player, reason);
    }
    InputCapture ui_capture() const;
    void dispatch_ui_frame(const elysia::ui::UiInputFrame &input);
    elysia::scene::Scene &_scene;
    bool _claim_first_gamepad = true;
    std::function<void(LocalPlayerId, InputCancelReason)> _cancel;
    elysia::input::InputDevice _ui_active_device = elysia::input::InputDevice::Keyboard;
    elysia::input::LocalPlayerId _ui_owner = elysia::input::PrimaryLocalPlayer;
    elysia::ui::UiInputState _last_ui_state;
    bool _await_initial_input = true;
    std::vector<elysia::input::InputSourceId> _previous_ui_sources;
    std::map<elysia::input::InputSourceId, elysia::input::InputCapture> _previous_capture;
    std::map<elysia::input::InputSourceId, elysia::input::InputSuppression> _suppression, _ui_suppression;
    elysia::input::InputSnapshot _last_input;
    std::vector<elysia::input::RawInputEvent> _consumed;
    bool _block_gameplay = false;
    elysia::ui::UiInputRouter _ui_input_router;
};
} // namespace elysia::input
