#include "builtin_asset_catalog.h"
#include "builtin_asset_keys.h"
#include "../../localization/locale.h"

#include <array>
#include <system_error>
#include <utility>

namespace elysia::builtin
{
namespace
{
const std::array<BuiltinAssetDescriptor, 5> kFontDescriptors = {
    BuiltinAssetDescriptor{asset_keys::LatinFont, "fonts/NotoSans-Regular.ttf"},
    BuiltinAssetDescriptor{asset_keys::SimplifiedChineseFont, "fonts/NotoSansSC-Regular.ttf"},
    BuiltinAssetDescriptor{asset_keys::TraditionalChineseFont, "fonts/NotoSansTC-Regular.ttf"},
    BuiltinAssetDescriptor{asset_keys::JapaneseFont, "fonts/NotoSansJP-Regular.ttf"},
    BuiltinAssetDescriptor{asset_keys::KoreanFont, "fonts/NotoSansKR-Regular.ttf"},
};

const std::array<BuiltinAssetDescriptor, 7> kTextureDescriptors = {
    BuiltinAssetDescriptor{asset_keys::ElysiaDefaultTexture, "textures/elysia.png"},
    BuiltinAssetDescriptor{asset_keys::ElysiaBlackTexture, "textures/elysia_black.png"},
    BuiltinAssetDescriptor{asset_keys::ElysiaBlackAlphaInverseTexture,"textures/elysia_black_alpha_inverse.png"},
    BuiltinAssetDescriptor{asset_keys::ElysiaLightEdgeTexture, "textures/elysia_light_edge.png"},
    BuiltinAssetDescriptor{asset_keys::ElysiaWhiteTexture,"textures/elysia_white.png"},
    BuiltinAssetDescriptor{asset_keys::EngineCharacterMoveTexture, "textures/character_move.png"},
    BuiltinAssetDescriptor{asset_keys::EngineCharacterIdleTexture, "textures/character_idle.png"},
};

const std::array<BuiltinLocaleDescriptor, 5> kLocaleDescriptors = {
    BuiltinLocaleDescriptor{elysia::localization::kEnglishLocale, "i18n/en/engine.json"},
    BuiltinLocaleDescriptor{elysia::localization::kSimplifiedChineseLocale, "i18n/zh-Hans/engine.json"},
    BuiltinLocaleDescriptor{elysia::localization::kTraditionalChineseLocale, "i18n/zh-Hant/engine.json"},
    BuiltinLocaleDescriptor{elysia::localization::kJapaneseLocale, "i18n/ja/engine.json"},
    BuiltinLocaleDescriptor{elysia::localization::kKoreanLocale, "i18n/ko/engine.json"},
};

const std::array<BuiltinAnimationDescriptor, 2> kAnimationDescriptors = {
    BuiltinAnimationDescriptor{
        .key = asset_keys::EngineCharacterIdleAnimation,
        .texture_key = asset_keys::EngineCharacterIdleTexture,
        .frame_width = 32,
        .frame_height = 32,
        .frame_count = 8,
        .fps = 8.0,
        .loop = true
    },
    BuiltinAnimationDescriptor{
        .key = asset_keys::EngineCharacterMoveAnimation,
        .texture_key = asset_keys::EngineCharacterMoveTexture,
        .frame_width = 32,
        .frame_height = 32,
        .frame_count = 8,
        .fps = 8.0,
        .loop = true
    }
};

const std::array<BuiltinAudioDescriptor, 0> kSoundDescriptors = {};

const std::array<BuiltinAudioDescriptor, 1> kMusicDescriptors = {
    BuiltinAudioDescriptor{asset_keys::ElysianRealm,"audio\\Elysian_Realm.ogg"}
};

constexpr std::string_view kRequiredMarkerFileName = ".elysia_engine_required";
}

BuiltinAssetCatalog::BuiltinAssetCatalog(std::filesystem::path project_root)
    : _root(std::move(project_root) / "assets" / "engine"){}

BuiltinAssetCatalog::BuiltinAssetCatalog(const elysia::io::PathManager& path_manager)
    : _root(path_manager.assets() / "engine"){}

const std::filesystem::path& BuiltinAssetCatalog::root() const noexcept
{
    return _root;
}

std::filesystem::path BuiltinAssetCatalog::required_marker_path() const
{
    return _root / kRequiredMarkerFileName;
}

std::span<const BuiltinAssetDescriptor> BuiltinAssetCatalog::fonts() const noexcept
{
    return kFontDescriptors;
}

std::span<const BuiltinAssetDescriptor> BuiltinAssetCatalog::textures() const noexcept
{
    return kTextureDescriptors;
}

std::span<const BuiltinLocaleDescriptor> BuiltinAssetCatalog::locales() const noexcept
{
    return kLocaleDescriptors;
}

std::span<const BuiltinAnimationDescriptor> BuiltinAssetCatalog::animations() const noexcept
{
    return kAnimationDescriptors;
}

std::span<const BuiltinAudioDescriptor> BuiltinAssetCatalog::sounds() const noexcept
{
    return kSoundDescriptors;
}

std::span<const BuiltinAudioDescriptor> BuiltinAssetCatalog::music() const noexcept
{
    return kMusicDescriptors;
}

std::filesystem::path BuiltinAssetCatalog::resolve(const std::filesystem::path& relative_path) const
{
    return _root / relative_path;
}

std::expected<void, BuiltinAssetValidationError>
BuiltinAssetCatalog::validate_required_files() const
{
    std::error_code error;
    if (!std::filesystem::is_directory(_root, error))
    {
        return std::unexpected(BuiltinAssetValidationError{
            .code = BuiltinAssetValidationErrorCode::RootMissing,
            .path = _root
        });
    }

    const std::filesystem::path required_marker = required_marker_path();
    if (!std::filesystem::is_regular_file(required_marker, error))
    {
        return std::unexpected(BuiltinAssetValidationError{
            .code = BuiltinAssetValidationErrorCode::RequiredMarkerMissing,
            .path = required_marker
        });
    }

    const auto validate_asset = [this](const auto& descriptor)-> std::expected<void, BuiltinAssetValidationError>
    {
        const std::filesystem::path path = resolve(descriptor.relative_path);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
        {
            return std::unexpected(BuiltinAssetValidationError{
                .code = BuiltinAssetValidationErrorCode::RequiredFileMissing,
                .path = path
            });
        }

        return {};
    };

    for (const BuiltinAssetDescriptor& descriptor : kFontDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinAssetDescriptor& descriptor : kTextureDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinLocaleDescriptor& descriptor : kLocaleDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinAudioDescriptor& descriptor : kSoundDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }
    for (const BuiltinAudioDescriptor& descriptor : kMusicDescriptors)
    {
        if (const auto result = validate_asset(descriptor); !result)
            return result;
    }

    for (const BuiltinAnimationDescriptor& descriptor : kAnimationDescriptors)
    {
        if (descriptor.key.empty() || descriptor.texture_key.empty()
            || descriptor.frame_width <= 0 || descriptor.frame_height <= 0
            || descriptor.frame_count == 0 || descriptor.fps <= 0.0)
        {
            return std::unexpected(BuiltinAssetValidationError{
                .code = BuiltinAssetValidationErrorCode::RequiredFileMissing,
                .path = _root
            });
        }
    }

    return {};
}
}
