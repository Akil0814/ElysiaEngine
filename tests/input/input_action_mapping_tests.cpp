#define SDL_MAIN_HANDLED

#include "engine/input/action/input_action_map.h"
#include "tests/support/test_assertions.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace elysia::input;
using elysia::tests::require;

namespace
{
const InputActionId Jump{"test.jump"};
const InputActionId Confirm{"test.confirm"};
const InputActionId Throttle{"test.throttle"};
const InputActionId Move{"test.move"};

InputBinding button(const InputActionId& action, RawInputControl control)
{
    return { action, ButtonInputBinding{ control } };
}

void test_action_id_validation()
{
    bool rejected = false;
    try { (void)InputActionId{"invalid action"}; }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Action ids must reject invalid dotted keys");
}

void test_button_edges_and_event_deduplication()
{
    InputActionMap map;
    require(map.register_action({ Jump, InputActionValueType::Button },
        { button(Jump, RawInputControl::KeySpace), button(Jump, RawInputControl::GamepadSouth) }),
        "Button action registration must succeed");
    require(map.register_action({ Confirm, InputActionValueType::Button },
        { button(Confirm, RawInputControl::KeySpace) }),
        "A control must be bindable to multiple actions");

    RawInputFrame raw;
    auto idle = map.resolve(raw);
    require(idle.events.empty(), "Idle input must not synthesize events");

    raw.state.set_pressed(RawInputControl::KeySpace, true);
    auto pressed = map.resolve(raw);
    require(pressed.frame.is_pressed(Jump) && pressed.frame.is_just_pressed(Jump),
        "Button press must appear in the action frame");
    require(pressed.frame.is_just_pressed(Confirm),
        "One physical control must drive multiple actions");
    require(pressed.events.size() == 2,
        "Each changed action must emit exactly one event");

    raw.state.set_pressed(RawInputControl::GamepadSouth, true);
    auto duplicate_source = map.resolve(raw);
    require(duplicate_source.events.empty(),
        "A second held binding must not duplicate an already active button event");

    raw.state.set_pressed(RawInputControl::KeySpace, false);
    auto still_pressed = map.resolve(raw);
    require(still_pressed.frame.is_pressed(Jump) && !still_pressed.frame.is_just_released(Jump),
        "An action must remain active while another binding is held");
    require(still_pressed.frame.is_just_released(Confirm),
        "Independent actions must retain their own edges");

    raw.state.set_pressed(RawInputControl::GamepadSouth, false);
    auto released = map.resolve(raw);
    require(released.frame.is_just_released(Jump)
        && released.events.size() == 1
        && released.events.front().phase == ActionInputPhase::Canceled,
        "Final binding release must cancel the action once");
}

void test_axis_dead_zones_and_composition()
{
    InputActionMap map;
    require(map.register_action({ Throttle, InputActionValueType::Axis1D, 0.5f, 0.2f },
        { { Throttle, AxisInputBinding{ RawInputAxis::GamepadRightX } } }),
        "Axis1D registration must succeed");
    require(map.register_action({ Move, InputActionValueType::Axis2D, 0.5f, 0.2f },
        {
            { Move, Button2DInputBinding{
                RawInputControl::KeyA, RawInputControl::KeyD,
                RawInputControl::KeyW, RawInputControl::KeyS } },
            { Move, Axis2DInputBinding{
                RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY } }
        }), "Axis2D registration must succeed");

    RawInputFrame raw;
    raw.state.set_axis(RawInputAxis::GamepadRightX, 0.19f);
    raw.state.set_axis(RawInputAxis::GamepadLeftX, 0.1f);
    raw.state.set_axis(RawInputAxis::GamepadLeftY, 0.1f);
    auto dead = map.resolve(raw);
    require(dead.frame.axis1d(Throttle) == 0.0f && dead.frame.axis2d(Move).is_zero(),
        "Axis values inside their dead zones must resolve to zero");

    raw.state.set_axis(RawInputAxis::GamepadRightX, -0.75f);
    raw.state.set_axis(RawInputAxis::GamepadLeftX, 0.6f);
    raw.state.set_axis(RawInputAxis::GamepadLeftY, 0.8f);
    auto analog = map.resolve(raw);
    require(std::fabs(analog.frame.axis1d(Throttle) + 0.75f) < 0.001f,
        "Axis1D must preserve values outside the dead zone");
    require(analog.frame.axis2d(Move).nearly_equals({ 0.6f, 0.8f }, 0.001f),
        "Axis2D must preserve a unit-length analog vector");

    raw.state.set_axis(RawInputAxis::GamepadLeftX, 0.0f);
    raw.state.set_axis(RawInputAxis::GamepadLeftY, 0.0f);
    raw.state.set_pressed(RawInputControl::KeyW, true);
    raw.state.set_pressed(RawInputControl::KeyD, true);
    auto digital = map.resolve(raw);
    require(digital.frame.axis2d(Move) == elysia::core::Vector2(1.0f, -1.0f),
        "Four-button composites must preserve diagonal components");

    raw.state.set_pressed(RawInputControl::KeyA, true);
    auto opposed = map.resolve(raw);
    require(opposed.frame.axis2d(Move) == elysia::core::Vector2(0.0f, -1.0f),
        "Opposing digital directions must cancel");
}

void test_runtime_rebinding_and_defaults()
{
    InputActionMap map;
    require(map.register_action({ Jump, InputActionValueType::Button },
        { button(Jump, RawInputControl::KeySpace) }), "Default binding registration must succeed");
    require(map.replace_bindings(Jump, { button(Jump, RawInputControl::KeyJ) }),
        "Runtime binding replacement must succeed");

    RawInputFrame raw;
    raw.state.set_pressed(RawInputControl::KeySpace, true);
    require(!map.resolve(raw).frame.is_pressed(Jump), "Replaced defaults must stop driving the action");
    raw.state.set_pressed(RawInputControl::KeySpace, false);
    raw.state.set_pressed(RawInputControl::KeyJ, true);
    require(map.resolve(raw).frame.is_pressed(Jump), "Replacement binding must drive the action");

    require(map.clear_bindings(Jump) && map.bindings(Jump).empty(),
        "Bindings must be clearable at runtime");
    map.reset_defaults();
    require(map.bindings(Jump).size() == 1,
        "Default bindings must be restorable");
}
}

int main()
{
    test_action_id_validation();
    test_button_edges_and_event_deduplication();
    test_axis_dead_zones_and_composition();
    test_runtime_rebinding_and_defaults();
    std::cout << "input action mapping tests passed\n";
    return 0;
}
