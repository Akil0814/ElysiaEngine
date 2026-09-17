#include "game/input/command_view.h"
#include "tests/support/input_snapshot_builder.h"
#define SDL_MAIN_HANDLED

#include "engine/gameplay/control/control_command.h"
#include "../../game/input/gameplay_input_map.h"
#include "tests/support/test_assertions.h"

#include <iostream>

using elysia::tests::require;

int main()
{
    using namespace elysia::gameplay;
    using namespace elysia::input;

    InputActionMap map = example::input::make_default_gameplay_input_map();
    const InputActionId custom{"example.transform"};
    require(map.register_action({ custom, InputActionValueType::Button },
        { { custom, ButtonInputBinding{ RawInputControl::KeyT } } }),
        "Projects must be able to add custom gameplay actions");

    elysia::tests::InputSnapshotBuilder raw;
    raw.press(RawInputControl::KeyW, true);
    raw.press(RawInputControl::KeyD, true);
    raw.press(RawInputControl::KeySpace, true);
    raw.press(RawInputControl::KeyT, true);
    auto result = map.resolve(raw.take());
    ControlCommand gameplay{.state = std::move(result.frame), .events = std::move(result.events)};
    require(example::input::CommandView(gameplay).move() == elysia::core::Vector2(1.0f, -1.0f),
        "Standard gameplay movement bindings must resolve");
    require(example::input::CommandView(gameplay).jump_pressed(), "Gameplay semantic accessors must expose standard actions");
    require(gameplay.state.is_just_pressed(custom),
        "Gameplay frames must retain custom action lookup");

    require(map.replace_bindings(example::input::actions::Jump,
        { { example::input::actions::Jump, ButtonInputBinding{ RawInputControl::KeyK } } }),
        "Standard gameplay bindings must be replaceable");
    raw.press(RawInputControl::KeySpace, false);
    raw.press(RawInputControl::KeyK, true);
    require(map.resolve(raw.take()).frame.is_just_pressed(example::input::actions::Jump),
        "Rebound standard gameplay actions must resolve immediately");

    std::cout << "engine gameplay tests passed\n";
    return 0;
}
