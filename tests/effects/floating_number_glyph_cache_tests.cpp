#define SDL_MAIN_HANDLED

#include "engine/effects/number/floating_number_glyph_cache.h"
#include "engine/io/path/path_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdlib>

namespace
{
using elysia::tests::require;

struct GlyphCacheFixture
{
    GlyphCacheFixture()
    {
        require(SDL_Init(SDL_INIT_VIDEO) == 0,"glyph cache tests must initialize SDL video");
        require(TTF_Init() == 0,"glyph cache tests must initialize SDL_ttf");
        surface = SDL_CreateRGBSurfaceWithFormat(0,128,128,32,SDL_PIXELFORMAT_RGBA32);
        second_surface = SDL_CreateRGBSurfaceWithFormat(0,128,128,32,SDL_PIXELFORMAT_RGBA32);
        require(surface && second_surface,"glyph cache tests must create software surfaces");
        renderer = SDL_CreateSoftwareRenderer(surface);
        second_renderer = SDL_CreateSoftwareRenderer(second_surface);
        require(renderer && second_renderer,"glyph cache tests must create software renderers");

        require(elysia::io::PathManager::instance()->init(),"glyph cache tests must initialize paths");
        elysia::resources::ResourceManager::instance()->clear();
        require(elysia::resources::ResourceManager::instance()->load_font(
            "ui.latin.20",
            elysia::io::PathManager::instance()->fonts() / "fusion-pixel-10px-proportional-latin.ttf",
            20
        ),"glyph cache tests must load the floating-number font");
        font = ELYSIA_RESOURCES->find_font("ui.latin.20");
        require(font != nullptr,"loaded floating-number font must be available");
    }

    ~GlyphCacheFixture()
    {
        elysia::resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(second_renderer);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(second_surface);
        SDL_FreeSurface(surface);
        TTF_Quit();
        SDL_Quit();
    }

    SDL_Surface* surface = nullptr;
    SDL_Surface* second_surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Renderer* second_renderer = nullptr;
    TTF_Font* font = nullptr;
};

void test_cache_domains_and_lifetime(GlyphCacheFixture& fixture)
{
    using namespace elysia::effects;
    FloatingNumberGlyphCache cache;
    require(!cache.configure(nullptr,fixture.font,1),"cache must reject a missing renderer");
    require(!cache.configure(fixture.renderer,nullptr,1),"cache must reject a missing font");
    require(cache.configure(fixture.renderer,fixture.font,1),"cache must accept complete dependencies");
    require(!FloatingNumberGlyphCache::supports('A'),"letters must not be accepted as number glyphs");
    require(!cache.glyph(FloatingNumberColor::White,'A').has_value(),"unsupported glyph lookup must fail safely");

    const auto first = cache.glyph(FloatingNumberColor::White,'8');
    const auto reused = cache.glyph(FloatingNumberColor::White,'8');
    const auto yellow = cache.glyph(FloatingNumberColor::Yellow,'8');
    require(first && reused && yellow,"supported color glyphs must be generated");
    require(first->texture.get() == reused->texture.get(),"same color and character must reuse a texture");
    require(first->texture.get() != yellow->texture.get(),"different colors must use independent textures");

    const auto run = cache.resolve("12%",FloatingNumberColor::Orange);
    require(run && run->size() == 3,"complete supported text must resolve atomically");
    require(!cache.resolve("12A",FloatingNumberColor::Orange).has_value(),
        "a run containing an unsupported glyph must fail atomically");

    FloatingNumberTexturePtr retained = first->texture;
    cache.reset();
    int width = 0;
    int height = 0;
    require(retained && SDL_QueryTexture(retained.get(),nullptr,nullptr,&width,&height) == 0,
        "live effects must retain textures across cache reset");

    require(cache.configure(fixture.renderer,fixture.font,1),"cache must reconfigure after reset");
    const auto before_renderer_change = cache.glyph(FloatingNumberColor::White,'5');
    require(before_renderer_change.has_value(),"configured cache must create glyphs");
    require(cache.configure(fixture.renderer,fixture.font,2),
        "font generation changes must reconfigure the cache");
    const auto after_generation_change = cache.glyph(
        FloatingNumberColor::White,
        '5');
    require(after_generation_change
        && after_generation_change->texture.get()
            != before_renderer_change->texture.get(),
        "font generation changes must invalidate cached glyphs");
    require(cache.configure(fixture.second_renderer,fixture.font,2),"renderer changes must reconfigure the cache");
    const auto after_renderer_change = cache.glyph(FloatingNumberColor::White,'5');
    require(after_renderer_change && after_renderer_change->texture.get() != after_generation_change->texture.get(),
        "renderer changes must invalidate cached textures");

    cache.reset();
}
}

int main()
{
    GlyphCacheFixture fixture;
    test_cache_domains_and_lifetime(fixture);
    return EXIT_SUCCESS;
}
