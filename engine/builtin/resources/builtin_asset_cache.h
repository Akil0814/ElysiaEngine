#pragma once

#include "builtin_asset_catalog.h"
#include "../../animation/animation.h"
#include "../../resources/atlas/atlas.h"
#include "../../resources/texture/texture_loader.h"

#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

struct SDL_Renderer;

namespace elysia::builtin
{
struct BuiltinFontDeleter
{
    void operator()(TTF_Font* font) const noexcept;
};

struct BuiltinSoundDeleter
{
    void operator()(Mix_Chunk* sound) const noexcept;
};

struct BuiltinMusicDeleter
{
    void operator()(Mix_Music* music) const noexcept;
};

using BuiltinFontPtr = std::unique_ptr<TTF_Font, BuiltinFontDeleter>;
using BuiltinSoundPtr = std::unique_ptr<Mix_Chunk, BuiltinSoundDeleter>;
using BuiltinMusicPtr = std::unique_ptr<Mix_Music, BuiltinMusicDeleter>;
using BuiltinTranslationTable = std::unordered_map<std::string, std::string>;

struct BuiltinAnimationDefinition
{
    std::string key;
    const elysia::resources::Atlas* atlas = nullptr;
    double fps = 0.0;
    bool loop = false;
};

class BuiltinAssetCache
{
public:
    BuiltinAssetCache() = default;
    ~BuiltinAssetCache();

    BuiltinAssetCache(const BuiltinAssetCache&) = delete;
    BuiltinAssetCache& operator=(const BuiltinAssetCache&) = delete;
    BuiltinAssetCache(BuiltinAssetCache&&) = delete;
    BuiltinAssetCache& operator=(BuiltinAssetCache&&) = delete;

    [[nodiscard]] std::expected<void, std::string> initialize(SDL_Renderer* renderer,
        const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes);
    void shutdown() noexcept;

    [[nodiscard]] bool initialized() const noexcept;

    [[nodiscard]] SDL_Texture* find_texture(std::string_view key) const noexcept;
    [[nodiscard]] TTF_Font* find_font(std::string_view locale,int point_size) const noexcept;
    [[nodiscard]] const std::string* find_translation(std::string_view locale,std::string_view key) const noexcept;
    [[nodiscard]] const BuiltinAnimationDefinition* find_animation(std::string_view key) const noexcept;
    [[nodiscard]] Mix_Chunk* find_sound(std::string_view key) const noexcept;
    [[nodiscard]] Mix_Music* find_music(std::string_view key) const noexcept;
    [[nodiscard]] std::unique_ptr<elysia::animation::Animation> create_animation(std::string_view key) const;

    [[nodiscard]] std::size_t texture_count() const noexcept;
    [[nodiscard]] std::size_t font_count() const noexcept;
    [[nodiscard]] std::size_t locale_count() const noexcept;
    [[nodiscard]] std::size_t animation_count() const noexcept;
    [[nodiscard]] std::size_t sound_count() const noexcept;
    [[nodiscard]] std::size_t music_count() const noexcept;

    [[nodiscard]] static std::string font_key(std::string_view locale,int point_size);

private:
    using TextureMap = std::unordered_map<std::string, elysia::resources::TextureResource>;
    using FontMap = std::unordered_map<std::string, BuiltinFontPtr>;
    using TranslationTables = std::unordered_map<std::string, BuiltinTranslationTable>;
    using AtlasMap = std::unordered_map<std::string, std::unique_ptr<elysia::resources::Atlas>>;
    using AnimationDefinitions = std::unordered_map<std::string, BuiltinAnimationDefinition>;
    using SoundMap = std::unordered_map<std::string, BuiltinSoundPtr>;
    using MusicMap = std::unordered_map<std::string, BuiltinMusicPtr>;

    struct PreparedState
    {
        TextureMap textures;
        FontMap fonts;
        TranslationTables translations;
        AtlasMap atlases;
        AnimationDefinitions animations;
        SoundMap sounds;
        MusicMap music;
    };

    [[nodiscard]] std::expected<PreparedState, std::string> prepare(SDL_Renderer* renderer,
        const BuiltinAssetCatalog& catalog,std::span<const int> point_sizes) const;

private:
    TextureMap _textures;
    FontMap _fonts;
    TranslationTables _translations;
    AtlasMap _atlases;
    AnimationDefinitions _animations;
    SoundMap _sounds;
    MusicMap _music;
    SDL_Renderer* _renderer = nullptr;
};
}
