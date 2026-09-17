#pragma once
#include "input_source.h"
#include <map>
#include <vector>
#include <algorithm>
namespace elysia::input
{
class LocalPlayerRegistry
{
  public:
    LocalPlayerRegistry()
    {
        reset();
    }
    void reset()
    {
        _players.clear();
        _owners.clear();
        _next = 1;
        create_player();
        bind_source(PrimaryLocalPlayer, InputSourceId::keyboard_mouse());
    }
    LocalPlayerId create_player()
    {
        LocalPlayerId id{_next++};
        _players.push_back(id);
        ++_revision;
        return id;
    }
    bool contains(LocalPlayerId id) const
    {
        return std::ranges::find(_players, id) != _players.end();
    }
    bool remove_player(LocalPlayerId id)
    {
        if (id == PrimaryLocalPlayer || !contains(id))
            return false;
        std::erase(_players, id);
        std::erase_if(_owners, [&](const auto &e) { return e.second == id; });
        ++_revision;
        return true;
    }
    bool bind_source(LocalPlayerId player, InputSourceId source)
    {
        if (!contains(player) || source.value == 0)
            return false;
        if (auto it = _owners.find(source); it != _owners.end())
            return it->second == player;
        for (const auto &[s, p] : _owners)
            if (p == player && s.is_gamepad() == source.is_gamepad())
                return false;
        _owners.emplace(source, player);
        ++_revision;
        return true;
    }
    bool unbind_source(InputSourceId source)
    {
        if (!_owners.erase(source))
            return false;
        ++_revision;
        return true;
    }
    bool replace_gamepad(LocalPlayerId player, InputSourceId source)
    {
        if (!contains(player) || !source.is_gamepad() || (owner(source).value && owner(source) != player))
            return false;
        std::erase_if(_owners, [&](const auto &e) { return e.second == player && e.first.is_gamepad(); });
        _owners[source] = player;
        ++_revision;
        return true;
    }
    LocalPlayerId owner(InputSourceId source) const
    {
        auto it = _owners.find(source);
        return it == _owners.end() ? LocalPlayerId{} : it->second;
    }
    std::vector<InputSourceId> sources(LocalPlayerId player) const
    {
        std::vector<InputSourceId> out;
        for (auto [s, p] : _owners)
            if (p == player)
                out.push_back(s);
        return out;
    }
    const std::vector<LocalPlayerId> &players() const
    {
        return _players;
    }
    std::uint64_t revision() const
    {
        return _revision;
    }

  private:
    std::vector<LocalPlayerId> _players;
    std::map<InputSourceId, LocalPlayerId> _owners;
    std::uint64_t _next = 1, _revision = 0;
};
} // namespace elysia::input
