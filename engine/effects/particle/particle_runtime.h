#pragma once

#include "particle_effect.h"
#include "../effect_request_failure.h"
#include <optional>

namespace elysia::effects
{
class ParticleRuntime
{
public:
    [[nodiscard]] std::expected<ParticleEffectHandle, EffectRequestFailure> create(
        const ParticleEffectDefinition& definition, SDL_Texture* texture,
        const ParticleEffectSpawnRequest& request, const void* scene, std::optional<std::size_t> frame = {});
    [[nodiscard]] std::expected<void, EffectRequestFailure> set_position(ParticleEffectHandle handle, elysia::core::Vector2 position);
    bool stop(ParticleEffectHandle handle) noexcept;
    bool cancel(ParticleEffectHandle handle) noexcept;
    [[nodiscard]] bool active(ParticleEffectHandle handle) const noexcept;
    void update(double delta, bool paused, std::optional<std::size_t> frame = {});
    void unbind(const void* scene) noexcept;
    void clear() noexcept;
    [[nodiscard]] elysia::core::RenderResult render(SDL_Renderer* renderer, elysia::core::DepthLayer layer,
        const elysia::camera::Camera& camera);
    [[nodiscard]] std::size_t particle_count() const noexcept;
private:
    struct Slot
    {
        std::uint64_t generation = 1;
        const void* scene = nullptr;
        std::optional<std::size_t> created_frame;
        std::optional<ParticleEffect> effect;
    };
    static void retire(Slot& slot) noexcept;
    std::vector<Slot> _slots;
    std::vector<std::uint32_t> _order;
    std::uint32_t _next_seed = 0x9e3779b9;
};
}
