#define SDL_MAIN_HANDLED

#include "engine/gameplay/input/gameplay_input_frame.h"
#include "engine/gameplay/input/gameplay_input_map.h"
#include "tests/support/test_assertions.h"

#include <iostream>

using elysia::tests::require;

int main()
{
    using namespace elysia::gameplay;
    using namespace elysia::input;

    InputActionMap map = make_default_gameplay_input_map();
    const InputActionId custom{"example.transform"};
    require(map.register_action({ custom, InputActionValueType::Button },
        { { custom, ButtonInputBinding{ RawInputControl::KeyT } } }),
        "Projects must be able to add custom gameplay actions");

    RawInputFrame raw;
    raw.state.set_pressed(RawInputControl::KeyW, true);
    raw.state.set_pressed(RawInputControl::KeyD, true);
    raw.state.set_pressed(RawInputControl::KeySpace, true);
    raw.state.set_pressed(RawInputControl::KeyT, true);
    auto result = map.resolve(raw);
    GameplayInputFrame gameplay(std::move(result.frame));
    require(gameplay.move() == elysia::core::Vector2(1.0f, -1.0f),
        "Standard gameplay movement bindings must resolve");
    require(gameplay.jump_pressed(), "Gameplay semantic accessors must expose standard actions");
    require(gameplay.actions().is_just_pressed(custom),
        "Gameplay frames must retain custom action lookup");

    require(map.replace_bindings(actions::Jump,
        { { actions::Jump, ButtonInputBinding{ RawInputControl::KeyK } } }),
        "Standard gameplay bindings must be replaceable");
    raw.state.set_pressed(RawInputControl::KeySpace, false);
    raw.state.set_pressed(RawInputControl::KeyK, true);
    require(map.resolve(raw).frame.is_just_pressed(actions::Jump),
        "Rebound standard gameplay actions must resolve immediately");

    std::cout << "engine gameplay tests passed\n";
    return 0;
}
