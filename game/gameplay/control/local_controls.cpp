#include "local_controls.h"
#include "engine/gameplay/scene/gameplay_scene.h"
#include <map>
#include <stdexcept>
#include <utility>

namespace example::gameplay
{
namespace
{
// Game-owned session choices survive scene recreation; expired engine handles are replaced.
std::map<elysia::input::LocalPlayerId, elysia::gameplay::ControllerHandle>&
session_player_handles()
{
    static std::map<elysia::input::LocalPlayerId, elysia::gameplay::ControllerHandle> players;
    return players;
}
} // namespace

void configure_scene_player(elysia::gameplay::GameplayScene &scene,
                            elysia::gameplay::ControllerHandle &handle,
                            elysia::core::GameObject &target,
                            elysia::input::LocalPlayerId player,
                            elysia::input::InputActionMap map)
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
    if (!service->bind_target(handle, scene.control_context(), target).succeeded())
        throw std::logic_error("Demo controller target binding failed.");
}
std::optional<elysia::gameplay::ControllerHandle>
existing_session_player(elysia::input::LocalPlayerId player)
{
    const auto& players = session_player_handles();
    const auto found = players.find(player);
    if (found == players.end() || !elysia::gameplay::ControllerService::instance()->get(found->second))
        return {};
    return found->second;
}
elysia::gameplay::ControllerHandle session_player(elysia::input::LocalPlayerId player)
{
    using namespace elysia::gameplay;
    auto *service = ControllerService::instance();
    auto &handle = session_player_handles()[player];
    if (!service->get(handle))
    {
        auto result = service->create<LocalPlayerController>({ControllerScope::Session, {}}, player,
                                                             example::input::make_gameplay_input_map());
        if (!result)
            throw std::logic_error("Session player creation failed.");
        handle = *result;
    }
    return handle;
}
} // namespace example::gameplay
