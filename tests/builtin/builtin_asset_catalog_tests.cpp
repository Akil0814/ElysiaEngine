#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/builtin/resources/builtin_asset_keys.h"
#include "tests/support/test_assertions.h"

#include <chrono>
#include <filesystem>
#include <set>
#include <string>

namespace
{
using elysia::tests::require;
using elysia::builtin::BuiltinAssetCatalog;
using elysia::builtin::BuiltinAssetValidationErrorCode;

template<typename Descriptor>
std::set<std::string> keys_of(const std::span<const Descriptor> descriptors)
{
    std::set<std::string> keys;
    for (const auto& descriptor : descriptors)
        keys.emplace(descriptor.key);
    return keys;
}

std::set<std::string> locales_of(
    const std::span<const elysia::builtin::BuiltinLocaleDescriptor> descriptors)
{
    std::set<std::string> locales;
    for (const auto& descriptor : descriptors)
        locales.emplace(descriptor.locale);
    return locales;
}

class TemporaryDirectory
{
public:
    explicit TemporaryDirectory(const std::filesystem::path& path)
        : _path(path)
    {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
        require(!error, "temporary directory must be removable");
        std::filesystem::create_directories(_path, error);
        require(!error, "temporary directory must be creatable");
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

private:
    std::filesystem::path _path;
};
}

int main()
{
    using namespace elysia::builtin::asset_keys;

    static_assert(LatinFont == "engine.font.latin");
    static_assert(SimplifiedChineseFont == "engine.font.zh_hans");
    static_assert(TraditionalChineseFont == "engine.font.zh_hant");
    static_assert(JapaneseFont == "engine.font.ja");
    static_assert(KoreanFont == "engine.font.ko");
    static_assert(ElysiaDefaultTexture == "engine.brand.elysia.default");
    static_assert(ElysiaBlackTexture == "engine.brand.elysia.black");
    static_assert(ElysiaBlackAlphaInverseTexture
        == "engine.brand.elysia.black_alpha_inverse");
    static_assert(ElysiaLightEdgeTexture == "engine.brand.elysia.light_edge");
    static_assert(ElysiaWhiteTexture == "engine.brand.elysia.white");
    static_assert(EngineCharacterIdleTexture == "engine.character.sprite.idle");
    static_assert(EngineCharacterMoveTexture == "engine.character.sprite.move");
    static_assert(EngineCharacterIdleAnimation == "engine.character.idle");
    static_assert(EngineCharacterMoveAnimation == "engine.character.move");
    static_assert(TestSound == "engine.test.sound");
    static_assert(TestMusic == "engine.test.music");
    static_assert(ElysianRealm == "engine.elysia.music");

    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    BuiltinAssetCatalog catalog(source_root);

    require(catalog.root() == source_root / "assets" / "engine",
        "assist root must be resolved below the project assets root");
    require(catalog.fonts().size() == 5, "assist catalog must describe five font faces");
    require(catalog.textures().size() == 7, "assist catalog must describe all seven Engine textures");
    require(catalog.locales().size() == 5, "assist catalog must describe five locales");
    require(catalog.animations().size() == 2, "assist catalog must describe both Engine character animations");
    require(catalog.sounds().empty(), "assist catalog must not register sounds before assets are added");
    require(catalog.music().size() == 1,
        "assist catalog must describe the Elysia scene music");
    require(
        keys_of(catalog.fonts()) == std::set<std::string>{
            std::string(JapaneseFont),
            std::string(KoreanFont),
            std::string(LatinFont),
            std::string(SimplifiedChineseFont),
            std::string(TraditionalChineseFont),
        },
        "assist font keys must be stable"
    );
    require(
        keys_of(catalog.textures()) == std::set<std::string>{
            std::string(ElysiaBlackTexture),
            std::string(ElysiaBlackAlphaInverseTexture),
            std::string(ElysiaDefaultTexture),
            std::string(ElysiaLightEdgeTexture),
            std::string(ElysiaWhiteTexture),
            std::string(EngineCharacterIdleTexture),
            std::string(EngineCharacterMoveTexture),
        },
        "assist texture keys must be stable"
    );
    require(
        keys_of(catalog.music()) == std::set<std::string>{
            std::string(ElysianRealm)
        },
        "assist music keys must be stable"
    );
    const auto animations = catalog.animations();
    require(animations[0].key == EngineCharacterIdleAnimation
            && animations[0].texture_key == EngineCharacterIdleTexture
            && animations[1].key == EngineCharacterMoveAnimation
            && animations[1].texture_key == EngineCharacterMoveTexture,
        "Engine character animation descriptors must use stable animation and texture keys");
    for (const auto& animation : animations)
    {
        require(animation.frame_width == 32 && animation.frame_height == 32
                && animation.frame_count == 8 && animation.fps == 8.0 && animation.loop,
            "Engine character animations must retain their hardcoded strip contract");
        require(animation.has_expected_texture_dimensions(256, 32)
                && !animation.has_expected_texture_dimensions(255, 32),
            "Engine character animations must reject invalid strip dimensions");
    }
    require(
        locales_of(catalog.locales()) == std::set<std::string>{
            "en", "ja", "ko", "zh-Hans", "zh-Hant"
        },
        "assist locales must use the Engine BCP-47 contract"
    );
    require(
        catalog.validate_required_files().has_value(),
        "all repository built-in files must be present"
    );
    require(
        catalog.required_marker_path() == source_root / "assets" / "engine" / ".elysia_engine_required",
        "assist catalog must require the Engine-specific marker file"
    );

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    const std::filesystem::path temporary_root =
        std::filesystem::temp_directory_path() / ("elysia_assist_catalog_" + unique_suffix);
    TemporaryDirectory temporary_directory(temporary_root / "assets" / "engine");

    BuiltinAssetCatalog missing_file_catalog(temporary_root);
    const auto missing_file = missing_file_catalog.validate_required_files();
    require(!missing_file.has_value(), "catalog must reject a missing required resource");
    require(
        missing_file.error().code == BuiltinAssetValidationErrorCode::RequiredMarkerMissing,
        "existing assist root with no marker must report a marker failure"
    );
    require(
        missing_file.error().path == temporary_root / "assets" / "engine" / ".elysia_engine_required",
        "missing marker failure must identify the Engine-specific marker"
    );

    BuiltinAssetCatalog missing_root_catalog(temporary_root / "missing");
    const auto missing_root = missing_root_catalog.validate_required_files();
    require(!missing_root.has_value(), "catalog must reject a missing assist root");
    require(
        missing_root.error().code == BuiltinAssetValidationErrorCode::RootMissing,
        "missing assist root must have a distinct diagnostic"
    );

    return 0;
}
