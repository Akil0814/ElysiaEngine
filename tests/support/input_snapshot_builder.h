#pragma once
#include "engine/input/input_snapshot.h"
#include <map>
#include <utility>
namespace elysia::tests
{
// Stateful test device producer. Events are recorded when controls change, never inferred from frame edges.
class InputSnapshotBuilder
{
  public:
    InputSnapshotBuilder &event(elysia::input::RawInputEvent event)
    {
        using namespace elysia::input;
        if (!event.source.value)
            event.source =
                is_gamepad_button_control(event.control) || event.type == RawInputEventType::AxisChanged
                    ? InputSourceId::gamepad(7)
                    : InputSourceId::keyboard_mouse();
        if (event.device == InputDevice::Unknown)
            event.device = event.source.is_gamepad() ? InputDevice::Gamepad
                                                     : (is_mouse_button_control(event.control) ||
                                                                event.type == RawInputEventType::MouseMoved ||
                                                                event.type == RawInputEventType::MouseWheel
                                                            ? InputDevice::Mouse
                                                            : InputDevice::Keyboard);
        auto &source = _sources[event.source];
        source.source = event.source;
        apply_raw_event(source.frame.state, event);
        source.frame.mouse_x = event.mouse_x;
        source.frame.mouse_y = event.mouse_y;
        source.frame.mouse_delta_x += event.mouse_delta_x;
        source.frame.mouse_delta_y += event.mouse_delta_y;
        _events.push_back(event);
        return *this;
    }
    void press(elysia::input::RawInputControl control, bool pressed)
    {
        using namespace elysia::input;
        auto source =
            is_gamepad_button_control(control) ? InputSourceId::gamepad(7) : InputSourceId::keyboard_mouse();
        if (_sources[source].frame.state.is_pressed(control) != pressed)
            event({.control = control,
                   .type = pressed ? RawInputEventType::ControlPressed : RawInputEventType::ControlReleased,
                   .source = source});
    }
    void axis(elysia::input::RawInputAxis axis, float value)
    {
        event({.axis = axis,
               .type = elysia::input::RawInputEventType::AxisChanged,
               .axis_value = value,
               .source = elysia::input::InputSourceId::gamepad(7)});
    }
    elysia::input::InputSnapshot take()
    {
        using namespace elysia::input;
        InputSnapshot result;
        _sources[InputSourceId::keyboard_mouse()].source = InputSourceId::keyboard_mouse();
        for (auto &[id, source] : _sources)
        {
            result.sources.push_back(source);
            source.frame.state.begin_frame();
            source.initial_state = source.frame.state;
            source.frame.mouse_delta_x = source.frame.mouse_delta_y = 0;
        }
        result.events = std::exchange(_events, {});
        return result;
    }

  private:
    std::map<elysia::input::InputSourceId, elysia::input::InputSourceFrame> _sources;
    std::vector<elysia::input::RawInputEvent> _events;
};
inline elysia::input::InputSnapshot events_snapshot(std::vector<elysia::input::RawInputEvent> events)
{
    InputSnapshotBuilder devices;
    for (auto event : events)
        devices.event(event);
    return devices.take();
}
} // namespace elysia::tests
