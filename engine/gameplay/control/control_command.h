#pragma once
#include "../../input/action/action_input_frame.h"
#include "../../input/input_source.h"
#include <map>
#include <vector>
namespace elysia::gameplay
{
using InputCancelReason = elysia::input::InputCancelReason;
struct ControllerHandle
{
    std::uint64_t runtime = 0, instance = 0;
    auto operator<=>(const ControllerHandle &) const = default;
    explicit operator bool() const
    {
        return runtime && instance;
    }
};
struct ControlCommand
{
    ControllerHandle controller;
    std::uint64_t binding_generation = 0, sequence = 0, tick = 0;
    elysia::input::ActionInputFrame state;
    std::vector<elysia::input::ActionInputEvent> events;
    std::unordered_map<elysia::input::InputActionId, elysia::input::InputActionValue,
                       elysia::input::InputActionIdHash>
        deltas;
};
class ControlCommandReceiver
{
  public:
    virtual ~ControlCommandReceiver() = default;
    virtual void on_control_command(const ControlCommand &, double fixed_delta) = 0;
    virtual void on_control_cancelled(InputCancelReason) = 0;
};
} // namespace elysia::gameplay
