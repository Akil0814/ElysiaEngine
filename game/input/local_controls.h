#pragma once
#include "gameplay_input_map.h"
#include "gameplay_actions.h"
#include "../../engine/gameplay/scene/gameplay_scene.h"
#include <stdexcept>
namespace example::input
{
inline void configure_scene_player(elysia::gameplay::GameplayScene &scene,
                                   elysia::gameplay::ControllerHandle &handle,
                                   elysia::core::GameObject &target,
                                   elysia::input::LocalPlayerId player = elysia::input::PrimaryLocalPlayer,
                                   elysia::input::InputActionMap map = make_gameplay_input_map())
{
    using namespace elysia::gameplay;
    auto *service = ControllerService::instance();
    if (!service->get(handle))
    {
        auto result = service->create<LocalPlayerController>(
            {ControllerScope::Scene, scene.control_context().token()}, player, std::move(map));
        if (!result)
            throw std::logic_error("Demo requires an explicit controller session and valid scene context.");
        handle = *result;
    }
    if (!service->bind_target(handle, scene.control_context(), target))
        throw std::logic_error("Demo controller target binding failed.");
}
// Game-owned session choices survive scene recreation; expired engine handles are replaced.
inline elysia::gameplay::ControllerHandle session_player(elysia::input::LocalPlayerId player)
{
    using namespace elysia::gameplay;
    static std::map<elysia::input::LocalPlayerId, ControllerHandle> players;
    auto *service = ControllerService::instance();
    auto &handle = players[player];
    if (!service->get(handle))
    {
        auto result = service->create<LocalPlayerController>({ControllerScope::Session, {}}, player,
                                                             make_gameplay_input_map());
        if (!result)
            throw std::logic_error("Session player creation failed.");
        handle = *result;
    }
    return handle;
}
} // namespace example::input
