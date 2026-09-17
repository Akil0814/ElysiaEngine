#include "scene_input_router.h"
#include "../scene/scene.h"
#include <algorithm>
namespace elysia::input
{
InputCapture SceneInputRouter::ui_capture() const
{
    auto mask = InputCapture::None;
    for (const auto &root : _scene._ui_roots)
        if (root && root->is_visible() && root->is_active() && !root->is_destroyed())
            mask = mask | root->input_capture();
    return mask;
}
void SceneInputRouter::suppress(LocalPlayerId player)
{
    for (const auto &source : _last_input.sources)
        if (_scene._players->owner(source.source) == player)
            _suppression[source.source].block(source.frame.state, AllInputCapture);
    cancel(player);
}
void SceneInputRouter::set_ui_owner(LocalPlayerId owner)
{
    if (!_scene._players->contains(owner) || owner == _ui_owner)
        return;
    suppress(_ui_owner);
    suppress(owner);
    for (const auto &source : _last_input.sources)
        _ui_suppression[source.source].block(source.frame.state, AllInputCapture);
    _ui_owner = owner;
    _ui_input_router.reset_transient_state();
    for (auto &root : _scene._ui_roots)
        if (root)
            root->cancel_input_interaction();
    dispatch_ui_frame({});
}
void SceneInputRouter::set_all_gameplay_input_blocked(bool blocked)
{
    if (blocked == _block_gameplay)
        return;
    _block_gameplay = blocked;
    for (auto player : _scene._players->players())
        suppress(player);
}
void SceneInputRouter::reset()
{
    for (auto player : _scene._players->players())
        suppress(player);
    _await_initial_input = true;
    _last_ui_state = {};
    for (auto &root : _scene._ui_roots)
        if (root)
            root->cancel_input_interaction();
    _ui_input_router.reset_transient_state();
    _consumed.clear();
    _block_gameplay = false;
}
void SceneInputRouter::consume_input(const RawInputEvent &event)
{
    _consumed.push_back(event);
    _suppression[event.source].block(event);
}
void SceneInputRouter::route(const InputSnapshot &snapshot)
{
    InputSnapshot input = snapshot;
    for (std::size_t index = 0; index < input.events.size(); ++index)
        input.events[index].routing_id = index + 1;
    auto update_capture = [&](InputSourceId source, InputCapture mask) {
        auto &previous = _previous_capture[source];
        if (previous != mask && mask != InputCapture::None)
            cancel(_scene._players->owner(source));
        previous = mask;
    };
    _last_input = input;
    _consumed.clear();
    for (auto source : input.removed)
    {
        const auto player = _scene._players->owner(source);
        if (player.value)
            cancel(player, InputCancelReason::SourceChanged);
        if (player == _ui_owner)
        {
            _ui_input_router.reset_transient_state();
            for (auto &root : _scene._ui_roots)
                if (root)
                    root->cancel_input_interaction();
        }
        _scene._players->unbind_source(source);
        _previous_capture.erase(source);
        _suppression.erase(source);
        _ui_suppression.erase(source);
    }
    if (!_scene._players->contains(_ui_owner))
        set_ui_owner(PrimaryLocalPlayer);
    if (input.focus_lost)
    {
        for (auto player : _scene._players->players())
        {
            suppress(player);
            cancel(player, InputCancelReason::FocusLost);
        }
        for (const auto &source : input.sources)
            _ui_suppression[source.source].block(source.frame.state, AllInputCapture);
        _ui_input_router.reset_transient_state();
        for (auto &root : _scene._ui_roots)
            if (root)
                root->cancel_input_interaction();
        dispatch_ui_frame({});
    }
    // Only menu scenes may automatically claim an unassigned first controller.
    for (const auto &event : input.events)
        if (event.source.is_gamepad() && !_scene._players->owner(event.source).value)
        {
            if (_scene.on_unassigned_input(event))
            {
                consume_input(event);
                continue;
            }
            if (_claim_first_gamepad && event.type == RawInputEventType::ControlPressed)
            {
                if (_scene._players->bind_source(PrimaryLocalPlayer, event.source))
                    consume_input(event);
            }
        }
    const auto ui_sources = _scene._players->sources(_ui_owner);
    if (_await_initial_input || ui_sources != _previous_ui_sources)
    {
        _ui_input_router.reset_transient_state();
        for (auto &root : _scene._ui_roots)
            if (root)
                root->cancel_input_interaction();
        for (const auto &source : input.sources)
        {
            if (_await_initial_input)
                _suppression[source.source].block(source.initial_state, AllInputCapture);
            if (_scene._players->owner(source.source) == _ui_owner)
                _ui_suppression[source.source].block(source.initial_state, AllInputCapture);
        }
        _previous_ui_sources = ui_sources;
        _await_initial_input = false;
    }
    InputSnapshot routed = input;
    routed.events.clear();
    std::map<InputSourceId, RawInputState> physical;
    RawInputFrame ui_frame;
    for (auto &source : routed.sources)
    {
        physical[source.source] = source.initial_state;
        auto mask = input.capture;
        if (_scene._players->owner(source.source) == _ui_owner)
            mask = mask | ui_capture();
        if (_scene._paused || _block_gameplay || input.focus_lost)
            mask = AllInputCapture;
        update_capture(source.source, mask);
        source.initial_state = _suppression[source.source].filter(source.initial_state, mask);
        if (_scene._players->owner(source.source) == _ui_owner)
        {
            auto state = _ui_suppression[source.source].filter(source.frame.state, input.capture);
            merge_raw_state(ui_frame.state, state);

            if (!source.source.is_gamepad())
            {
                ui_frame.mouse_x = source.frame.mouse_x;
                ui_frame.mouse_y = source.frame.mouse_y;
            }
        }
    }
    const auto previous_ui_device = _ui_active_device;
    for (const auto &event : input.events)
        if (_scene._players->owner(event.source) == _ui_owner && !captured(input.capture, event.device))
            _ui_active_device = event.device;
    ui_frame.active_device = _ui_active_device;
    ui_frame.device_switched_this_frame = previous_ui_device != _ui_active_device;
    dispatch_ui_frame(_ui_input_router.route_frame(ui_frame));
    std::vector<RawInputEvent> shortcut_events;
    for (const auto &event : input.events)
    {
        auto &state = physical[event.source];
        apply_raw_event(state, event);
        const auto player = _scene._players->owner(event.source);
        auto before = ui_capture();
        bool consumed =
            std::ranges::any_of(_consumed, [&](const auto &e) { return e.routing_id == event.routing_id; });
        const auto ui_state = _ui_suppression[event.source].filter(state, input.capture);
        bool suppressed =
            (event.type == RawInputEventType::ControlPressed && !ui_state.is_pressed(event.control)) ||
            (event.type == RawInputEventType::AxisChanged &&
             ui_state.axis_value(event.axis) != event.axis_value);
        if (!consumed && player == _ui_owner && !captured(input.capture, event.device) && !suppressed &&
            !input.focus_lost)
            consumed = _scene.dispatch_ui_events(_ui_input_router.route_event(event));
        auto mask = input.capture;
        if (player == _ui_owner)
            mask = mask | before | ui_capture();
        if (!consumed && player == _ui_owner && !captured(mask, event.device) && !suppressed &&
            !input.focus_lost)
            shortcut_events.push_back(event);
        if (_scene._paused || _block_gameplay || input.focus_lost)
            mask = AllInputCapture;
        if (consumed)
            consume_input(event);
        update_capture(event.source, mask);
        auto filtered = _suppression[event.source].filter(state, mask);
        bool allowed = !consumed && !captured(mask, event.device);
        if (event.type == RawInputEventType::ControlPressed)
            allowed = allowed && filtered.is_pressed(event.control);
        if (event.type == RawInputEventType::AxisChanged)
            allowed = allowed && filtered.axis_value(event.axis) == event.axis_value;
        if (allowed)
            routed.events.push_back(event);
        else if (captured(mask, event.device))
        {
            std::erase_if(routed.events, [&](const auto &previous) {
                return previous.source == event.source && captured(mask, previous.device);
            });
        }
    }
    if (_scene.dispatch_ui_events(_ui_input_router.synthesize_events(ui_frame)))
    {
        // A synthetic scroll originates from the owner's left stick; retain that
        // attribution so a custom gameplay axis cannot consume the same operation.
        for (const auto &source : input.sources)
            if (source.source.is_gamepad() && _scene._players->owner(source.source) == _ui_owner)
            {
                for (auto axis : {RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY})
                {
                    RawInputEvent operation;
                    operation.source = source.source;
                    operation.device = InputDevice::Gamepad;
                    operation.type = RawInputEventType::AxisChanged;
                    operation.axis = axis;
                    operation.axis_value = source.frame.state.axis_value(axis);
                    _suppression[source.source].block(operation);
                }
                cancel(_ui_owner);
                std::erase_if(routed.events, [&](const auto &event) {
                    return event.source == source.source && event.type == RawInputEventType::AxisChanged &&
                           (event.axis == RawInputAxis::GamepadLeftX ||
                            event.axis == RawInputAxis::GamepadLeftY);
                });
            }
    }
    RawInputFrame shortcuts = ui_frame;
    shortcuts.state.clear();
    for (auto &source : routed.sources)
    {
        auto mask = input.capture;
        if (_scene._players->owner(source.source) == _ui_owner)
            mask = mask | ui_capture();
        if (_scene._paused || _block_gameplay || input.focus_lost)
            mask = AllInputCapture;
        source.frame.state = _suppression[source.source].filter(source.frame.state, mask);
        if (_scene._players->owner(source.source) == _ui_owner)
            merge_raw_state(shortcuts.state, source.frame.state);
    }
    _scene.on_shortcuts(shortcuts, shortcut_events);
    std::erase_if(routed.events, [&](const auto &event) {
        return std::ranges::any_of(_consumed,
                                   [&](const auto &e) { return e.routing_id == event.routing_id; });
    });
    for (auto &source : routed.sources)
    {
        auto mask = input.capture;
        const auto player = _scene._players->owner(source.source);
        if (player == _ui_owner)
            mask = mask | ui_capture();
        if (_scene._paused || _block_gameplay || input.focus_lost)
            mask = AllInputCapture;
        // Always observe physical state, never feed already filtered zeros back into a latch.
        if (const auto *physical_source = input.find(source.source))
            source.frame.state = _suppression[source.source].filter(physical_source->frame.state, mask);
        update_capture(source.source, mask);
        if (mask != InputCapture::None)
        {
            std::erase_if(routed.events, [&](const auto &event) {
                return event.source == source.source && captured(mask, event.device);
            });
        }
        if (_scene._paused || _block_gameplay)
        {
            source.frame.state.clear();
            source.initial_state.clear();
        }
    }
    if (_scene._paused || _block_gameplay)
        routed.events.clear();
    _scene.on_routed_input(routed);
}

void SceneInputRouter::dispatch_ui_frame(const elysia::ui::UiInputFrame &input)
{
    auto frame = input;
    for (int index = 1; index < int(elysia::ui::UiAction::Count); ++index)
    {
        const auto action = static_cast<elysia::ui::UiAction>(index);
        const bool current = input.state.is_pressed(action), previous = _last_ui_state.is_pressed(action);
        frame.state.set(action, current, current && !previous, !current && previous);
    }
    _last_ui_state = frame.state;

    _scene.dispatch_ui_frame(frame);
}
} // namespace elysia::input
