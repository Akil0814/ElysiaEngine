#pragma once

#include "../body/physics_system.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace elysia::physics::detail
{
// Piecewise linear motion accepted by CCD, in normalized fixed-step time.
// Collision events must be tested on these segments, never on a discarded
// prediction that continued through a blocking surface.
class MotionPaths
{
public:
    struct Sample { float time; elysia::core::Vector2 origin; };

    explicit MotionPaths(std::span<const PhysicsObjectState> states)
    {
        _paths.reserve(states.size());
        for (const auto& state : states)
            _paths[state.object.value].push_back({0.0f, state.previous_owner_origin});
    }

    void record(PhysicsObjectHandle object, float time, elysia::core::Vector2 origin)
    {
        auto& path = _paths.at(object.value);
        if (time == path.back().time)
            path.back().origin = origin;
        else
            path.push_back({time, origin});
    }

    void finish(std::span<const PhysicsObjectState> states)
    {
        for (const auto& state : states)
            record(state.object, 1.0f, state.current_owner_origin);
    }

    [[nodiscard]] elysia::core::Vector2 origin_at(
        PhysicsObjectHandle object, float time, elysia::core::Vector2 fallback) const
    {
        const auto found = _paths.find(object.value);
        if (found == _paths.end()) return fallback;
        const auto& path = found->second;
        for (std::size_t i = 1; i < path.size(); ++i)
        {
            if (time <= path[i].time)
            {
                const auto& a = path[i - 1];
                const auto& b = path[i];
                const float alpha = (time - a.time) / (b.time - a.time);
                return a.origin + (b.origin - a.origin) * alpha;
            }
        }
        return path.back().origin;
    }

    [[nodiscard]] std::vector<float> boundaries(PhysicsObjectHandle a, PhysicsObjectHandle b) const
    {
        std::vector<float> times{0.0f, 1.0f};
        for (auto object : {a, b})
            if (const auto found = _paths.find(object.value); found != _paths.end())
                for (const auto& sample : found->second) times.push_back(sample.time);
        std::ranges::sort(times);
        times.erase(std::unique(times.begin(), times.end()), times.end());
        return times;
    }

    [[nodiscard]] elysia::core::Rect bounds(
        PhysicsObjectHandle object, const elysia::core::Rect& initial_bounds,
        elysia::core::Vector2 initial_origin) const
    {
        auto result = initial_bounds;
        if (const auto found = _paths.find(object.value); found != _paths.end())
            for (const auto& sample : found->second)
                result = result.merged(initial_bounds.translated(sample.origin - initial_origin));
        return result;
    }

private:
    std::unordered_map<std::uint64_t, std::vector<Sample>> _paths;
};
}
