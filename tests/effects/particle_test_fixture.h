#pragma once
#include "engine/effects/particle/particle_runtime.h"
#include "engine/effects/effect_service.h"
#include "engine/effects/runtime/effect_manager.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/test_assertions.h"
#include <SDL3_image/SDL_image.h>

namespace elysia::tests
{
struct ParticleFixture
{
    SDL_Surface* surface = nullptr;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    explicit ParticleFixture(bool gpu = false)
    {
        require(SDL_Init(SDL_INIT_VIDEO), SDL_GetError());
        if (gpu)
        {
            window = SDL_CreateWindow("CPU particle GPU batch verification", 128, 96, SDL_WINDOW_HIDDEN);
            require(window, SDL_GetError()); renderer = SDL_CreateGPURenderer(nullptr, window);
        }
        else
        {
            surface = SDL_CreateSurface(128, 96, SDL_PIXELFORMAT_RGBA32);
            require(surface, SDL_GetError()); renderer = SDL_CreateSoftwareRenderer(surface);
        }
        require(renderer, SDL_GetError());
        auto* pixels = SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_RGBA32);
        require(pixels, SDL_GetError());
        require(SDL_FillSurfaceRect(pixels, nullptr, SDL_MapSurfaceRGBA(pixels,255,0,0,255)), SDL_GetError());
        SDL_Rect right{4,0,4,4};
        require(SDL_FillSurfaceRect(pixels, &right, SDL_MapSurfaceRGBA(pixels,0,0,255,255)), SDL_GetError());
        texture = SDL_CreateTextureFromSurface(renderer, pixels); SDL_DestroySurface(pixels);
        require(texture, SDL_GetError());
        require(SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST), SDL_GetError());
        require(resources::ResourceManager::instance()->store_texture("particle.test", resources::TexturePtr(texture)), "register test texture");
    }
    ~ParticleFixture()
    {
        effects::EffectManager::instance()->clear_content();
        effects::EffectManager::instance()->set_runtime_dependencies(nullptr,nullptr);
        resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(renderer); if (window) SDL_DestroyWindow(window); if (surface) SDL_DestroySurface(surface); SDL_Quit();
    }
    void clear()
    {
        require(SDL_SetRenderDrawColor(renderer,0,0,0,255) && SDL_RenderClear(renderer), SDL_GetError());
    }
    effects::ParticleEffectDefinition definition() const
    {
        effects::ParticleEffectDefinition d;
        d.effect_key="particle.test"; d.texture_key="particle.test"; d.max_particles=16; d.burst_count=1;
        d.lifetime_seconds={2,2}; d.start_size={24,8}; d.end_size=d.start_size; d.end_color=d.start_color;
        d.source_rect=core::Rect{0,0,4,4}; return d;
    }
};
}
