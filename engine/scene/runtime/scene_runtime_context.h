#pragma once

struct SDL_Renderer;

namespace elysia::io
{
struct ContentRegistry;
}

namespace elysia::builtin
{
class BuiltinAssetCache;
class BuiltinAudioPlayer;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::scene
{
class SceneRuntimeContext
{
public:
    SceneRuntimeContext(
        SDL_Renderer* renderer,
        const elysia::io::ContentRegistry& content_registry,
        int logical_width,
        int logical_height,
        const elysia::builtin::BuiltinAssetCache* builtin_asset_cache = nullptr,
        elysia::typography::FontResolver* font_resolver = nullptr,
        const elysia::builtin::BuiltinAudioPlayer* builtin_audio_player = nullptr
    ) noexcept;

    [[nodiscard]] SDL_Renderer* renderer() const noexcept;
    [[nodiscard]] const elysia::io::ContentRegistry& content_registry() const noexcept;
    [[nodiscard]] int logical_width() const noexcept;
    [[nodiscard]] int logical_height() const noexcept;
    [[nodiscard]] const elysia::builtin::BuiltinAssetCache* builtin_asset_cache() const noexcept;
    [[nodiscard]] elysia::typography::FontResolver* font_resolver() const noexcept;
    [[nodiscard]] const elysia::builtin::BuiltinAudioPlayer*
        builtin_audio_player() const noexcept;

private:
    SDL_Renderer* _renderer = nullptr;
    const elysia::io::ContentRegistry* _content_registry = nullptr;
    int _logical_width = 0;
    int _logical_height = 0;
    const elysia::builtin::BuiltinAssetCache* _builtin_asset_cache = nullptr;
    elysia::typography::FontResolver* _font_resolver = nullptr;
    const elysia::builtin::BuiltinAudioPlayer* _builtin_audio_player = nullptr;
};
}
