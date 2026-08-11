#pragma once

#include "collision_contact.h"

#include <cstdint>
#include <vector>

namespace elysia::physics
{
enum class CollisionEventPhase : std::uint8_t
{
    Begin,
    Stay,
    End
};

struct CollisionEvent
{
    CollisionEventPhase phase = CollisionEventPhase::Begin;
    CollisionContact contact{};
};

struct CollisionFrame
{
    std::vector<CollisionContact> contacts;
    std::vector<CollisionEvent> events;

    void clear() noexcept
    {
        contacts.clear();
        events.clear();
    }
};
}
