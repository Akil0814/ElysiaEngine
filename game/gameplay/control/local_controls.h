#pragma once
#include "game/input/gameplay_input_map.h"
#include "engine/gameplay/control/control_command.h"
#include <optional>

namespace elysia::core { class GameObject; }
namespace elysia::gameplay { class GameplayScene; }

namespace example::gameplay
{
void configure_scene_player(elysia::gameplay::GameplayScene& scene,
                            elysia::gameplay::ControllerHandle& handle,
                            elysia::core::GameObject& target,
                            elysia::input::LocalPlayerId player = elysia::input::PrimaryLocalPlayer,
                            elysia::input::InputActionMap map = example::input::make_gameplay_input_map());
[[nodiscard]] std::optional<elysia::gameplay::ControllerHandle>
existing_session_player(elysia::input::LocalPlayerId player);
[[nodiscard]] elysia::gameplay::ControllerHandle session_player(elysia::input::LocalPlayerId player);
} // namespace example::gameplay
