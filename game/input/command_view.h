#pragma once
#include "gameplay_actions.h"
#include "../../engine/gameplay/control/control_command.h"
#include <algorithm>
namespace example::input
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
        return _command.state.axis2d(actions::Move);
    }
    std::size_t press_count(const elysia::input::InputActionId &id) const
    {
        return std::ranges::count_if(_command.events, [&](const auto &e) {
            return e.action == id && e.phase == elysia::input::ActionInputPhase::Started;
        });
    }
    bool jump_pressed() const
    {
        return press_count(actions::Jump) > 0;
    }
    bool primary_pressed() const
    {
        return press_count(actions::Primary) > 0;
    }
    bool secondary_pressed() const
    {
        return press_count(actions::Secondary) > 0;
    }
    bool dash_pressed() const
    {
        return press_count(actions::Dash) > 0;
    }
    bool guard_held() const
    {
        return _command.state.is_pressed(actions::Guard);
    }

  private:
    const elysia::gameplay::ControlCommand &_command;
};
} // namespace example::input
