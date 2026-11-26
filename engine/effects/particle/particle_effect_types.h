#pragma once

#include "../../core/depth_layer.h"
#include "../../core/geometry/rect.h"
#include "../../core/render/color.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace elysia::effects
{
struct ParticleRange
{
    double min = 0.0;
    double max = 0.0;
};
enum class ParticleBlendMode { Alpha, Add };

struct ParticleEffectDefinition
{
    std::string effect_key;
    std::string texture_key;
    std::optional<elysia::core::Rect> source_rect;
    elysia::core::DepthLayer layer = elysia::core::DepthLayer::EffectFront;
    ParticleBlendMode blend = ParticleBlendMode::Alpha;
    std::size_t max_particles = 1024;
    std::size_t burst_count = 32;
    double emission_rate = 0.0;
    // Omitted: continuous emission lasts until stop(). Burst-only effects ignore this.
    std::optional<double> emission_duration_seconds;
    elysia::core::Vector2 velocity_min;
    elysia::core::Vector2 velocity_max;
    ParticleRange lifetime_seconds{0.4, 0.8};
    elysia::core::Vector2 acceleration;
    ParticleRange angle_degrees;
    ParticleRange angular_velocity_degrees;
    elysia::core::Vector2 start_size{8, 8};
    elysia::core::Vector2 end_size{0, 0};
    elysia::core::Color start_color{255, 255, 255, 255};
    elysia::core::Color end_color{255, 255, 255, 0};
};

struct ParticleEffectSpawnRequest
{
    std::string effect_key;
    // World-space center of newly emitted particles. Existing particles never follow it.
    elysia::core::Vector2 position;
    std::optional<std::uint32_t> seed;
};

struct ParticleEffectHandle
{
    std::uint32_t slot = 0;
    std::uint64_t generation = 0;
    bool operator==(const ParticleEffectHandle&) const noexcept = default;
};
}
