#pragma once
#include "game/input/gameplay_actions.h"
#include "engine/gameplay/control/control_command.h"
#include <algorithm>
namespace example::gameplay
{
class CommandView
{
  public:
    explicit CommandView(const elysia::gameplay::ControlCommand &command) : _command(command)
    {
    }
    const elysia::input::ActionInputFrame &actions() const
    {
        return _command.state;
    }
    elysia::core::Vector2 move() const
    {
        return _command.state.axis2d(example::input::actions::Move);
    }
    std::size_t press_count(const elysia::input::InputActionId &id) const
    {
        return std::ranges::count_if(_command.events, [&](const auto &e) {
            return e.action == id && e.phase == elysia::input::ActionInputPhase::Started;
        });
    }
    bool jump_pressed() const
    {
        return press_count(example::input::actions::Jump) > 0;
    }
    bool primary_pressed() const
    {
        return press_count(example::input::actions::Primary) > 0;
    }
    bool secondary_pressed() const
    {
        return press_count(example::input::actions::Secondary) > 0;
    }
    bool dash_pressed() const
    {
        return press_count(example::input::actions::Dash) > 0;
    }
    bool guard_held() const
    {
        return _command.state.is_pressed(example::input::actions::Guard);
    }

  private:
    const elysia::gameplay::ControlCommand &_command;
};
} // namespace example::gameplay
