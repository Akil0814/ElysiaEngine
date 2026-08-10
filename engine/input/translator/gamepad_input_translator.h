#pragma once

#include "input_translator.h"

namespace elysia::input
{
class GamepadInputTranslator : public InputTranslator
{
public:
    std::vector<RawInputEvent> translate_event(const SDL_Event& event) override;
    void reset();

private:
    void append_controller_button_events(
        std::vector<RawInputEvent>& events,
        Uint8 button,
        RawInputEventType type
    ) const;
    void append_axis_event(
        std::vector<RawInputEvent>& events,
        RawInputAxis axis,
        float normalized_value
    ) const;
    void append_trigger_virtual_button_event(
        std::vector<RawInputEvent>& events,
        RawInputControl control,
        bool pressed
    ) const;
    float normalize_stick_axis(Sint16 value) const;
    float normalize_trigger_axis(Sint16 value) const;

private:
    bool _left_trigger_pressed = false;
    bool _right_trigger_pressed = false;
};

}
