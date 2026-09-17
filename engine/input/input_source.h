#pragma once
#include <compare>
#include <cstdint>
namespace elysia::input
{
enum class InputCancelReason
{
    FocusLost,
    Suppressed,
    Unbound,
    Paused,
    Unavailable,
    Overflow,
    SourceChanged
};
struct InputSourceId
{
    std::uint64_t value = 0;
    constexpr auto operator<=>(const InputSourceId &) const = default;
    [[nodiscard]] constexpr bool is_gamepad() const
    {
        return value > 1;
    }
    static constexpr InputSourceId keyboard_mouse()
    {
        return {1};
    }
    static constexpr InputSourceId gamepad(std::uint32_t instance)
    {
        return {std::uint64_t(instance) + 2};
    }
};
struct LocalPlayerId
{
    std::uint64_t value = 0;
    constexpr auto operator<=>(const LocalPlayerId &) const = default;
};
inline constexpr LocalPlayerId PrimaryLocalPlayer{1};
} // namespace elysia::input
