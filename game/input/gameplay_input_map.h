#pragma once
#include "../../engine/input/action/input_action_map.h"
#include <set>
namespace example::input
{
enum class KeyboardScheme
{
    None,
    Wasd,
    Arrows
};
struct InputScheme
{
    KeyboardScheme keyboard = KeyboardScheme::Wasd;
    bool mouse = true;
    bool gamepad = true;
};
[[nodiscard]] elysia::input::InputActionMap make_gameplay_input_map(InputScheme scheme = {});
[[nodiscard]] std::set<elysia::input::RawInputControl> keyboard_keys(KeyboardScheme scheme);
} // namespace example::input
