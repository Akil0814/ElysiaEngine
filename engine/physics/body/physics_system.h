#pragma once

#include <vector>

namespace elysia::physics
{
class PhysicsSystem
{
public:
    PhysicsSystem() = default;

    template <typename Entry>
    void step(const std::vector<Entry>& body_entries, double delta)
    {
        (void)body_entries;
        (void)delta;
    }
};
}
