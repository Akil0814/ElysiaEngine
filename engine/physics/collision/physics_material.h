#pragma once

#include <algorithm>
#include <cmath>

namespace elysia::physics
{
struct PhysicsMaterial
{
    float static_friction = 0.6f;
    float dynamic_friction = 0.4f;
    float restitution = 0.0f;

    [[nodiscard]] constexpr bool operator==(const PhysicsMaterial&) const noexcept = default;
};

[[nodiscard]] inline PhysicsMaterial normalized_physics_material(
    PhysicsMaterial material) noexcept
{
    material.dynamic_friction = std::isfinite(material.dynamic_friction)
        ? std::max(0.0f, material.dynamic_friction)
        : 0.0f;
    material.static_friction = std::isfinite(material.static_friction)
        ? std::max(material.dynamic_friction, material.static_friction)
        : material.dynamic_friction;
    material.restitution = std::isfinite(material.restitution)
        ? std::clamp(material.restitution, 0.0f, 1.0f)
        : 0.0f;
    return material;
}

[[nodiscard]] inline PhysicsMaterial combine_physics_materials(
    PhysicsMaterial first,
    PhysicsMaterial second) noexcept
{
    first = normalized_physics_material(first);
    second = normalized_physics_material(second);
    const auto geometric_mean = [](float a, float b) noexcept
    {
        return static_cast<float>(std::sqrt(
            static_cast<double>(a) * static_cast<double>(b)));
    };
    PhysicsMaterial combined;
    combined.static_friction = geometric_mean(
        first.static_friction, second.static_friction);
    combined.dynamic_friction = geometric_mean(
        first.dynamic_friction, second.dynamic_friction);
    combined.restitution = std::max(first.restitution, second.restitution);
    return normalized_physics_material(combined);
}
}
