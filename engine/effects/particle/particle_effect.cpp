#include "particle_effect.h"
#include "../../core/render/sdl_render_boundary.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace elysia::effects
{
namespace
{
bool finite(elysia::core::Vector2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
bool range_valid(ParticleRange range) { return std::isfinite(range.min) && std::isfinite(range.max) && range.min <= range.max; }

class ParticleTextureState
{
public:
    explicit ParticleTextureState(SDL_Texture* texture) : _texture(texture)
    {
        elysia::core::detail::checked_render_operation("SDL_GetTextureBlendMode", [&] { return SDL_GetTextureBlendMode(texture, &_blend); });
        elysia::core::detail::checked_render_operation("SDL_GetTextureColorMod", [&] { return SDL_GetTextureColorMod(texture, &_r, &_g, &_b); });
        elysia::core::detail::checked_render_operation("SDL_GetTextureAlphaMod", [&] { return SDL_GetTextureAlphaMod(texture, &_a); });
    }
    void apply(SDL_BlendMode mode)
    {
        _changed = true;
        elysia::core::detail::checked_render_operation("SDL_SetTextureBlendMode", [&] { return SDL_SetTextureBlendMode(_texture, mode); });
    }
    template<class Restore> void restore(Restore&& restore)
    {
        if (!_changed) return;
        // SDL's software geometry fallback can change these despite ignoring their input values.
        restore([&] {
            elysia::core::detail::checked_render_operation("restore.SDL_SetTextureColorMod", [&] { return SDL_SetTextureColorMod(_texture, _r, _g, _b); });
        });
        restore([&] {
            elysia::core::detail::checked_render_operation("restore.SDL_SetTextureAlphaMod", [&] { return SDL_SetTextureAlphaMod(_texture, _a); });
        });
        restore([&] {
            elysia::core::detail::checked_render_operation("restore.SDL_SetTextureBlendMode", [&] { return SDL_SetTextureBlendMode(_texture, _blend); });
        });
    }
private:
    SDL_Texture* _texture;
    SDL_BlendMode _blend{};
    Uint8 _r{}, _g{}, _b{}, _a{};
    bool _changed = false;
};
}

namespace
{
auto invalid_definition(const ParticleEffectDefinition& definition, EffectRegistrationError code,
    std::string message, std::string_view field, std::source_location origin = std::source_location::current())
{
    return std::unexpected(EffectRegistrationFailure{code,
        elysia::core::make_failure_diagnostic(std::move(message), definition.effect_key, {}, origin), field});
}
}

std::expected<void, EffectRegistrationFailure> ParticleEffect::validate_parameters(const ParticleEffectDefinition& d)
{
    const auto invalid = [&](std::string_view field, std::string message,
        std::source_location origin = std::source_location::current()) {
        return invalid_definition(d, EffectRegistrationError::InvalidParticleParameters, std::move(message), field, origin);
    };
    if (d.effect_key.empty())
        return invalid_definition(d, EffectRegistrationError::InvalidKey, "Particle effect key is empty.", "key");
    if (d.texture_key.empty())
        return invalid_definition(d, EffectRegistrationError::MissingTexture, "Particle texture key is empty.", "texture_key");
    if (d.max_particles == 0 || d.max_particles > static_cast<std::size_t>(std::numeric_limits<int>::max() / 6))
        return invalid("max_particles", "Particle capacity is out of range.");
    if (!std::isfinite(d.emission_rate) || d.emission_rate < 0)
        return invalid("emission_rate", "Particle emission rate must be finite and nonnegative.");
    if (d.burst_count == 0 && d.emission_rate == 0)
        return invalid("burst_count", "Particle effect requires a burst or continuous emission.");
    if (d.emission_duration_seconds && (!std::isfinite(*d.emission_duration_seconds) || *d.emission_duration_seconds <= 0))
        return invalid("emission_duration_seconds", "Particle emission duration must be finite and positive.");
    if (static_cast<std::size_t>(d.layer) >= static_cast<std::size_t>(elysia::core::DepthLayer::Count))
        return invalid("layer", "Invalid particle depth layer.");
    if (d.blend != ParticleBlendMode::Alpha && d.blend != ParticleBlendMode::Add)
        return invalid("blend", "Invalid particle blend mode.");
    if (!finite(d.velocity_min))
        return invalid("velocity_min", "Minimum particle velocity must be finite.");
    if (!finite(d.velocity_max) || d.velocity_min.x > d.velocity_max.x || d.velocity_min.y > d.velocity_max.y)
        return invalid("velocity_max", "Maximum particle velocity must be finite and at least the minimum.");
    if (!finite(d.acceleration))
        return invalid("acceleration", "Particle acceleration must be finite.");
    if (!range_valid(d.lifetime_seconds) || d.lifetime_seconds.min <= 0)
        return invalid("lifetime_seconds", "Particle lifetime must be a finite, ordered, positive range.");
    if (!range_valid(d.angle_degrees))
        return invalid("angle_degrees", "Particle angle must be a finite, ordered range.");
    if (!range_valid(d.angular_velocity_degrees))
        return invalid("angular_velocity_degrees", "Particle angular velocity must be a finite, ordered range.");
    if (!finite(d.start_size) || d.start_size.x <= 0 || d.start_size.y <= 0)
        return invalid("start_size", "Particle start size must be finite and positive.");
    if (!finite(d.end_size) || d.end_size.x < 0 || d.end_size.y < 0)
        return invalid("end_size", "Particle end size must be finite and nonnegative.");
    if (d.source_rect)
    {
        const auto& rect = *d.source_rect;
        if (!finite(rect.position()) || !finite(rect.size()) || rect.x() < 0 || rect.y() < 0
            || rect.width() <= 0 || rect.height() <= 0
            || !std::isfinite(rect.right()) || !std::isfinite(rect.bottom()))
            return invalid("source_rect", "Particle source rectangle must be finite, positive in size, and have a nonnegative origin.");
    }
    return {};
}

std::expected<elysia::core::Vector2, EffectRegistrationFailure> ParticleEffect::validate(
    const ParticleEffectDefinition& d, SDL_Texture* texture)
{
    if (auto valid = validate_parameters(d); !valid)
        return std::unexpected(std::move(valid.error()));
    if (!texture)
        return invalid_definition(d, EffectRegistrationError::MissingTexture,
            "Particle texture does not exist: " + d.texture_key, "texture_key");
    elysia::core::Vector2 size;
    if (!SDL_GetTextureSize(texture, &size.x, &size.y))
        return invalid_definition(d, EffectRegistrationError::CreationFailed,
            std::string("SDL_GetTextureSize failed: ") + SDL_GetError(), "texture_key");
    if (!finite(size) || size.x <= 0 || size.y <= 0)
        return invalid_definition(d, EffectRegistrationError::InvalidSize,
            "Particle texture dimensions must be positive.", "texture_key");
    if (d.source_rect && (d.source_rect->right() > size.x || d.source_rect->bottom() > size.y))
        return invalid_definition(d, EffectRegistrationError::InvalidParticleParameters,
            "Particle source rectangle is outside the texture.", "source_rect");
    return size;
}

ParticleEffect::ParticleEffect(const ParticleEffectDefinition& definition, SDL_Texture* texture,
    elysia::core::Vector2 texture_size, elysia::core::Vector2 position, std::uint32_t seed)
    : _definition(definition), _texture(texture), _texture_size(texture_size), _position(position), _random(seed),
      _emitting(definition.emission_rate > 0)
{
    _particles.reserve(definition.max_particles);
    _vertices.reserve(definition.max_particles * 4);
    _indices.reserve(definition.max_particles * 6);
    for (std::size_t i = 0; i < definition.max_particles; ++i)
    {
        const int base = static_cast<int>(i * 4);
        for (const int index : {0, 1, 2, 0, 2, 3}) _indices.push_back(base + index);
    }
    for (std::size_t i = 0; i < std::min(definition.burst_count, definition.max_particles); ++i) emit(0);
}

double ParticleEffect::sample(ParticleRange range)
{
    const double unit = static_cast<double>(_random()) / 4294967296.0;
    return std::lerp(range.min, range.max, unit);
}

bool ParticleEffect::advance(Particle& p, double delta) const noexcept
{
    p.age += delta;
    if (p.age >= p.lifetime) return false;
    p.position.x = static_cast<float>(p.position.x + p.velocity.x * delta + 0.5 * _definition.acceleration.x * delta * delta);
    p.position.y = static_cast<float>(p.position.y + p.velocity.y * delta + 0.5 * _definition.acceleration.y * delta * delta);
    p.velocity.x = static_cast<float>(p.velocity.x + _definition.acceleration.x * delta);
    p.velocity.y = static_cast<float>(p.velocity.y + _definition.acceleration.y * delta);
    p.angle_degrees = std::remainder(p.angle_degrees + p.angular_velocity_degrees * delta, 360.0);
    return finite(p.position) && finite(p.velocity) && std::isfinite(p.angle_degrees);
}

void ParticleEffect::emit(double age)
{
    Particle p;
    p.position = _position;
    p.velocity = {static_cast<float>(sample({_definition.velocity_min.x, _definition.velocity_max.x})),
        static_cast<float>(sample({_definition.velocity_min.y, _definition.velocity_max.y}))};
    p.lifetime = sample(_definition.lifetime_seconds);
    p.angle_degrees = sample(_definition.angle_degrees);
    p.angular_velocity_degrees = sample(_definition.angular_velocity_degrees);
    if (advance(p, age)) _particles.push_back(p);
}

void ParticleEffect::update(double delta)
{
    if (!std::isfinite(delta) || delta <= 0) return;
    std::erase_if(_particles, [&](Particle& p) { return !advance(p, delta); });
    if (!_emitting) return;
    double emission_delta = delta;
    if (_definition.emission_duration_seconds)
        emission_delta = std::min(delta, std::max(0.0, *_definition.emission_duration_seconds - _emission_elapsed));
    _emission_elapsed += emission_delta;
    if (_definition.emission_duration_seconds && _emission_elapsed >= *_definition.emission_duration_seconds) _emitting = false;
    const double total = _emission_remainder + _definition.emission_rate * emission_delta;
    const double count = std::floor(total);
    _emission_remainder = std::isfinite(total) ? total - count : 0.0;
    const auto available = _definition.max_particles - _particles.size();
    const auto births = count >= static_cast<double>(available) ? available : static_cast<std::size_t>(count);
    // Keep only the most recent bounded set of births; dropped emission never becomes debt.
    for (std::size_t i = births; i > 0; --i)
        emit((delta - emission_delta) + (_emission_remainder + static_cast<double>(i - 1)) / _definition.emission_rate);
}

bool ParticleEffect::stop() noexcept
{
    if (!_emitting) return false;
    _emitting = false;
    return true;
}

elysia::core::RenderResult ParticleEffect::render(SDL_Renderer* renderer, const elysia::camera::Camera& camera)
{
    return elysia::core::detail::render_boundary(renderer, [&] {
        _vertices.clear();
        const auto src = _definition.source_rect.value_or(elysia::core::Rect{0, 0, _texture_size.x, _texture_size.y});
        const SDL_FPoint uv[] = {{src.x() / _texture_size.x, src.y() / _texture_size.y},
            {src.right() / _texture_size.x, src.y() / _texture_size.y},
            {src.right() / _texture_size.x, src.bottom() / _texture_size.y},
            {src.x() / _texture_size.x, src.bottom() / _texture_size.y}};
        for (const auto& p : _particles)
        {
            const float t = static_cast<float>(std::clamp(p.age / p.lifetime, 0.0, 1.0));
            const auto size = _definition.start_size + (_definition.end_size - _definition.start_size) * t;
            if (size.x <= 0 || size.y <= 0) continue;
            const auto channel = [t](std::uint8_t a, std::uint8_t b) { return std::lerp(float(a), float(b), t) / 255.0f; };
            const auto& a = _definition.start_color; const auto& b = _definition.end_color;
            const SDL_FColor color{channel(a.r, b.r), channel(a.g, b.g), channel(a.b, b.b), channel(a.a, b.a)};
            if (color.a <= 0) continue;
            const double radians = p.angle_degrees * std::numbers::pi / 180.0;
            const float c = static_cast<float>(std::cos(radians)), s = static_cast<float>(std::sin(radians));
            const elysia::core::Vector2 corners[] = {{-size.x/2, -size.y/2}, {size.x/2, -size.y/2}, {size.x/2, size.y/2}, {-size.x/2, size.y/2}};
            SDL_Vertex quad[4];
            bool valid = true;
            float left = std::numeric_limits<float>::max(), top = left, right = -left, bottom = -left;
            for (int i = 0; i < 4; ++i)
            {
                const auto v = camera.world_to_screen(p.position + elysia::core::Vector2{corners[i].x*c - corners[i].y*s, corners[i].x*s + corners[i].y*c});
                valid = valid && finite(v);
                left = std::min(left, v.x); right = std::max(right, v.x); top = std::min(top, v.y); bottom = std::max(bottom, v.y);
                quad[i] = {{v.x, v.y}, color, uv[i]};
            }
            if (!valid || right <= 0 || bottom <= 0 || left >= camera.viewport_size().x || top >= camera.viewport_size().y) continue;
            _vertices.insert(_vertices.end(), std::begin(quad), std::end(quad));
        }
        if (_vertices.empty()) return;
        ParticleTextureState state(_texture);
        elysia::core::detail::with_render_state(state, [&] {
            state.apply(_definition.blend == ParticleBlendMode::Alpha ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_ADD);
            elysia::core::detail::checked_render_operation("SDL_RenderGeometry", [&] {
                return SDL_RenderGeometry(renderer, _texture, _vertices.data(), static_cast<int>(_vertices.size()),
                    _indices.data(), static_cast<int>(_vertices.size() / 4 * 6));
            });
        });
    });
}
}
