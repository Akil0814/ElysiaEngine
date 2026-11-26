#pragma once

#include "particle_effect_types.h"
#include "../effect_registration_failure.h"
#include "../../camera/camera.h"
#include "../../core/render/render_failure.h"
#include <SDL3/SDL.h>
#include <expected>
#include <random>
#include <span>
#include <vector>

namespace elysia::effects
{
struct Particle
{
    elysia::core::Vector2 position;
    elysia::core::Vector2 velocity;
    double age = 0.0;
    double lifetime = 0.0;
    double angle_degrees = 0.0;
    double angular_velocity_degrees = 0.0;
};

// One visual group, owned by ParticleRuntime; neither the group nor particles are GameObjects.
class ParticleEffect
{
public:
    [[nodiscard]] static std::expected<void, EffectRegistrationFailure> validate_parameters(
        const ParticleEffectDefinition& definition);
    [[nodiscard]] static std::expected<elysia::core::Vector2, EffectRegistrationFailure> validate(
        const ParticleEffectDefinition& definition, SDL_Texture* texture);
    // Internal constructor: validate() must succeed before construction.
    ParticleEffect(const ParticleEffectDefinition& definition, SDL_Texture* texture,
        elysia::core::Vector2 texture_size, elysia::core::Vector2 position, std::uint32_t seed);
    void update(double delta);
    bool stop() noexcept;
    void set_position(elysia::core::Vector2 position) noexcept { _position = position; }
    [[nodiscard]] bool finished() const noexcept { return !_emitting && _particles.empty(); }
    [[nodiscard]] elysia::core::DepthLayer layer() const noexcept { return _definition.layer; }
    [[nodiscard]] std::span<const Particle> particles() const noexcept { return _particles; }
    [[nodiscard]] std::size_t particle_capacity() const noexcept { return _particles.capacity(); }
    [[nodiscard]] std::size_t vertex_capacity() const noexcept { return _vertices.capacity(); }
    [[nodiscard]] std::size_t index_capacity() const noexcept { return _indices.capacity(); }
    [[nodiscard]] elysia::core::RenderResult render(SDL_Renderer* renderer, const elysia::camera::Camera& camera);

private:
    double sample(ParticleRange range);
    void emit(double age);
    bool advance(Particle& particle, double delta) const noexcept;
    ParticleEffectDefinition _definition;
    SDL_Texture* _texture;
    elysia::core::Vector2 _texture_size;
    elysia::core::Vector2 _position;
    std::mt19937 _random;
    std::vector<Particle> _particles;
    std::vector<SDL_Vertex> _vertices;
    std::vector<int> _indices;
    double _emission_elapsed = 0.0;
    double _emission_remainder = 0.0;
    bool _emitting;
};
}
