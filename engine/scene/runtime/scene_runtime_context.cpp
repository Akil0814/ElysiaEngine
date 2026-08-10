#include "scene_runtime_context.h"

#include "../../io/loaders/asset_config_types.h"

namespace elysia::scene
{
SceneRuntimeContext::SceneRuntimeContext(
    SDL_Renderer* renderer,
    const elysia::io::ContentRegistry& content_registry,
    int logical_width,
    int logical_height,
    const elysia::builtin::BuiltinAssetCache* builtin_asset_cache,
    elysia::typography::FontResolver* font_resolver,
    const elysia::builtin::BuiltinAudioPlayer* builtin_audio_player
) noexcept
    : _renderer(renderer)
    , _content_registry(&content_registry)
    , _logical_width(logical_width)
    , _logical_height(logical_height)
    , _builtin_asset_cache(builtin_asset_cache)
    , _font_resolver(font_resolver)
    , _builtin_audio_player(builtin_audio_player)
{
}

SDL_Renderer* SceneRuntimeContext::renderer() const noexcept
{
    return _renderer;
}

const elysia::io::ContentRegistry& SceneRuntimeContext::content_registry() const noexcept
{
    return *_content_registry;
}

int SceneRuntimeContext::logical_width() const noexcept
{
    return _logical_width;
}

int SceneRuntimeContext::logical_height() const noexcept
{
    return _logical_height;
}

const elysia::builtin::BuiltinAssetCache* SceneRuntimeContext::builtin_asset_cache() const noexcept
{
    return _builtin_asset_cache;
}

elysia::typography::FontResolver* SceneRuntimeContext::font_resolver() const noexcept
{
    return _font_resolver;
}

const elysia::builtin::BuiltinAudioPlayer*
SceneRuntimeContext::builtin_audio_player() const noexcept
{
    return _builtin_audio_player;
}
}
