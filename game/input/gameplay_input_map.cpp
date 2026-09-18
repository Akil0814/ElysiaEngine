#include "gameplay_input_map.h"
#include "gameplay_actions.h"
#include <stdexcept>
namespace example::input
{
using namespace elysia::input;
std::set<RawInputControl> keyboard_keys(KeyboardScheme scheme)
{
    if (scheme == KeyboardScheme::Wasd)
        return {RawInputControl::KeyW, RawInputControl::KeyA,     RawInputControl::KeyS,
                RawInputControl::KeyD, RawInputControl::KeySpace, RawInputControl::KeyJ,
                RawInputControl::KeyK, RawInputControl::KeyL,     RawInputControl::KeyLeftShift,
                RawInputControl::KeyP};
    if (scheme == KeyboardScheme::Arrows)
        return {RawInputControl::KeyLeft, RawInputControl::KeyRight, RawInputControl::KeyUp,
                RawInputControl::KeyDown};
    return {};
}
InputActionMap make_gameplay_input_map(InputScheme scheme)
{
    InputActionMap map;
    auto add = [&](InputActionDescriptor desc, std::vector<InputBinding> bindings) {
        if (!map.register_action(desc, std::move(bindings)))
            throw std::logic_error("Invalid gameplay mapping");
    };
    std::vector<InputBinding> move;
    if (scheme.keyboard == KeyboardScheme::Wasd)
        move.push_back({actions::Move, Button2DInputBinding{RawInputControl::KeyA, RawInputControl::KeyD,
                                                            RawInputControl::KeyW, RawInputControl::KeyS}});
    if (scheme.keyboard == KeyboardScheme::Arrows)
        move.push_back(
            {actions::Move, Button2DInputBinding{RawInputControl::KeyLeft, RawInputControl::KeyRight,
                                                 RawInputControl::KeyUp, RawInputControl::KeyDown}});
    if (scheme.gamepad)
    {
        move.push_back(
            {actions::Move,
             Button2DInputBinding{RawInputControl::GamepadDPadLeft, RawInputControl::GamepadDPadRight,
                                  RawInputControl::GamepadDPadUp, RawInputControl::GamepadDPadDown}});
        move.push_back({actions::Move,
                        Axis2DInputBinding{RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY, 1, 1}});
    }
    add({actions::Move, InputActionValueType::Axis2D, 0.5f, 0.2f}, std::move(move));
    auto button = [&](const InputActionId &action, RawInputControl key, RawInputControl pad,
                      RawInputControl mouse = RawInputControl::None) {
        std::vector<InputBinding> bindings;
        if (scheme.keyboard == KeyboardScheme::Wasd)
            bindings.push_back({action, ButtonInputBinding{key}});
        if (scheme.gamepad)
            bindings.push_back({action, ButtonInputBinding{pad}});
        if (scheme.mouse && mouse != RawInputControl::None)
            bindings.push_back({action, ButtonInputBinding{mouse}});
        add({action, InputActionValueType::Button}, std::move(bindings));
    };
    button(actions::Jump, RawInputControl::KeySpace, RawInputControl::GamepadSouth);
    button(actions::Primary, RawInputControl::KeyJ, RawInputControl::GamepadWest, RawInputControl::MouseLeft);
    button(actions::Secondary, RawInputControl::KeyK, RawInputControl::GamepadNorth);
    button(actions::Guard, RawInputControl::KeyL, RawInputControl::GamepadLeftShoulder,
           RawInputControl::MouseRight);
    button(actions::Dash, RawInputControl::KeyLeftShift, RawInputControl::GamepadRightShoulder);
    button(actions::Pause, RawInputControl::KeyP, RawInputControl::GamepadStart);
    return map;
}
} // namespace example::input
