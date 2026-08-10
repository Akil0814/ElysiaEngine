#define SDL_MAIN_HANDLED

#include "engine/typography/font_settings.h"
#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/io/path/path_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/application_failure_scene.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
using elysia::typography::EffectTypographyRole;
using elysia::typography::FontSettings;
using elysia::typography::FontSource;
using elysia::typography::FontResolveErrorCode;
using elysia::typography::FontResolver;
using elysia::typography::ResolvedFontSettings;
using elysia::typography::UiTypographyProfile;
using elysia::typography::UiTypographyRole;
using elysia::typography::resolve_font_settings;
using elysia::tests::require;

class FontResolverFixture
{
public:
    FontResolverFixture()
    {
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
            "FontResolver tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "FontResolver tests must initialize PNG support");
        require(TTF_Init() == 0,
            "FontResolver tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
            "FontResolver tests must open SDL_mixer audio");

        _surface = SDL_CreateRGBSurfaceWithFormat(
            0,128,128,32,SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr,
            "FontResolver tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr,
            "FontResolver tests must create a software renderer");

        const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
        require(elysia::io::PathManager::instance()->init(source_root),
            "FontResolver tests must initialize project paths");
        require(_engine_cache.initialize(
            _renderer,
            elysia::builtin::BuiltinAssetCatalog(source_root),
            std::array{10,20,24,30,40,50,60,70}).has_value(),
            "FontResolver tests must initialize built-in fonts");
        elysia::resources::ResourceManager::instance()->clear();
    }

    ~FontResolverFixture()
    {
        elysia::resources::ResourceManager::instance()->clear();
        _engine_cache.shutdown();
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        Mix_CloseAudio();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    [[nodiscard]] elysia::builtin::BuiltinAssetCache& engine_cache() noexcept
    {
        return _engine_cache;
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept
    {
        return _renderer;
    }

    [[nodiscard]] elysia::resources::ResourceManager& resources() noexcept
    {
        return *elysia::resources::ResourceManager::instance();
    }

    [[nodiscard]] elysia::resources::ResourceService& resource_service() noexcept
    {
        return *elysia::resources::ResourceService::instance();
    }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
    elysia::builtin::BuiltinAssetCache _engine_cache;
};

ResolvedFontSettings settings_with_size(
    FontSource ui_source,
    FontSource effect_source,
    int point_size)
{
    UiTypographyProfile::PointSizes sizes{};
    sizes.fill(point_size);

    FontSettings settings;
    settings.ui.source = ui_source;
    settings.ui.typography_override = UiTypographyProfile(sizes);
    settings.floating_number.source = effect_source;
    settings.floating_number.point_size_override = point_size;
    const auto resolved = resolve_font_settings(settings);
    require(resolved.has_value(),
        "FontResolver test settings must resolve");
    return *resolved;
}

void load_project_fonts(
    elysia::resources::ResourceManager& resources,
    const std::filesystem::path& font_root,
    bool include_japanese,
    int point_size)
{
    const std::string size_suffix = "." + std::to_string(point_size);
    require(resources.load_font(
        "ui.latin" + size_suffix,
        font_root / "fusion-pixel-10px-proportional-latin.ttf",
        point_size), "FontResolver tests must load the Latin project font");
    require(resources.load_font(
        "ui.zh_hans" + size_suffix,
        font_root / "fusion-pixel-10px-proportional-zh_hans.ttf",
        point_size), "FontResolver tests must load the Simplified Chinese project font");
    require(resources.load_font(
        "ui.zh_hant" + size_suffix,
        font_root / "fusion-pixel-10px-proportional-zh_hant.ttf",
        point_size), "FontResolver tests must load the Traditional Chinese project font");
    require(resources.load_font(
        "ui.ko" + size_suffix,
        font_root / "fusion-pixel-10px-proportional-ko.ttf",
        point_size), "FontResolver tests must load the Korean project font");
    if (include_japanese)
    {
        require(resources.load_font(
            "ui.ja" + size_suffix,
            font_root / "fusion-pixel-10px-proportional-ja.ttf",
            point_size), "FontResolver tests must load the Japanese project font");
    }
}

void test_engine_resolution_and_validation(FontResolverFixture& fixture)
{
    FontResolver resolver;
    constexpr std::array<std::string_view,5> language_views{
        "en","ja","ko","zh-Hans","zh-Hant"
    };
    const std::array<std::string,5> languages{ "en","ja","ko","zh-Hans","zh-Hant" };
    const auto configured = resolver.configure(
        settings_with_size(
            FontSource::EngineBuiltIn,
            FontSource::EngineBuiltIn,
            24),
        fixture.engine_cache(),
        fixture.resource_service(),
        languages);
    require(configured.has_value(),
        "FontResolver must configure with complete Engine fonts");

    for (const std::string_view language : language_views)
    {
        for (std::size_t index = 0;
            index < UiTypographyProfile::RoleCount;
            ++index)
        {
            const auto resolved = resolver.resolve_ui(
                static_cast<UiTypographyRole>(index),
                language);
            require(resolved.has_value()
                && resolved->font
                && resolved->point_size == 24
                && resolved->source == FontSource::EngineBuiltIn,
                "FontResolver must resolve every Engine UI role and language");
        }
    }

    const auto effect = resolver.resolve_effect(
        EffectTypographyRole::FloatingNumber);
    require(effect.has_value()
        && effect->source == FontSource::EngineBuiltIn,
        "FontResolver must resolve the Engine floating-number font");

    const auto invalid_role = resolver.resolve_ui(
        UiTypographyRole::Count,
        "en");
    require(!invalid_role
        && invalid_role.error().code == FontResolveErrorCode::InvalidRole,
        "FontResolver must reject invalid UI roles");
    const auto invalid_source = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en",
        static_cast<FontSource>(999));
    require(!invalid_source
            && invalid_source.error().code == FontResolveErrorCode::InvalidSource,
        "FontResolver must reject invalid explicit UI font sources");
    const std::array legacy_locales{
        std::string("zh") + "_cn",
        std::string("zh_") + "Hans",
        std::string("zh_") + "Hant"
    };
    for (const std::string& legacy_locale : legacy_locales)
    {
        const auto legacy_language = resolver.resolve_ui(
            UiTypographyRole::Label,
            legacy_locale);
        require(!legacy_language
                && legacy_language.error().code == FontResolveErrorCode::UnsupportedLanguage,
            "FontResolver must reject every legacy locale spelling");
    }
}

void test_atomic_project_activation(FontResolverFixture& fixture)
{
    fixture.resources().clear();
    FontResolver resolver;
    const std::array<std::string,5> languages{ "en","ja","ko","zh-Hans","zh-Hant" };
    const auto configured = resolver.configure(
        settings_with_size(
            FontSource::Project,
            FontSource::Project,
            24),
        fixture.engine_cache(),
        fixture.resource_service(),
        languages);
    require(configured.has_value(),
        "project FontResolver settings must configure against Engine bootstrap fonts");
    require(resolver.project_point_sizes().size() == 1
            && resolver.project_point_sizes().front() == 24,
        "FontResolver must expose the resolved custom project point size");

    const auto before_activation = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en");
    require(before_activation.has_value()
        && before_activation->source == FontSource::EngineBuiltIn,
        "project fonts must remain inactive during startup loading");
    const auto explicit_project_before_activation = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en",
        FontSource::Project);
    require(!explicit_project_before_activation
            && explicit_project_before_activation.error().code
                == FontResolveErrorCode::FontUnavailable,
        "an explicit project source must fail before project fonts activate");
    const auto explicit_engine_before_activation = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en",
        FontSource::EngineBuiltIn);
    require(explicit_engine_before_activation
            && explicit_engine_before_activation->source
                == FontSource::EngineBuiltIn,
        "an explicit Engine source must resolve during bootstrap");
    const std::uint64_t bootstrap_generation = resolver.generation();

    const std::filesystem::path font_root =
        elysia::io::PathManager::instance()->fonts();
    load_project_fonts(fixture.resources(),font_root,false,24);
    const auto incomplete_activation = resolver.activate_project_fonts();
    require(!incomplete_activation
        && !resolver.project_fonts_active()
        && resolver.generation() == bootstrap_generation,
        "incomplete project fonts must not partially activate");

    require(fixture.resources().load_font(
        "ui.ja.24",
        font_root / "fusion-pixel-10px-proportional-ja.ttf",
        24), "FontResolver tests must complete the project font set");
    require(resolver.activate_project_fonts().has_value()
        && resolver.project_fonts_active()
        && resolver.generation() == bootstrap_generation + 1,
        "complete project fonts must activate atomically");

    const auto project_ui = resolver.resolve_ui(
        UiTypographyRole::DialogBody,
        "zh-Hans");
    const auto project_effect = resolver.resolve_effect(
        EffectTypographyRole::FloatingNumber);
    require(project_ui.has_value()
        && project_ui->source == FontSource::Project
        && project_effect.has_value()
        && project_effect->source == FontSource::Project,
        "active project fonts must serve UI and Latin floating numbers");
    const auto explicit_engine = resolver.resolve_ui(
        UiTypographyRole::DialogBody,
        "zh-Hans",
        FontSource::EngineBuiltIn);
    const auto explicit_project = resolver.resolve_ui(
        UiTypographyRole::DialogBody,
        "zh-Hans",
        FontSource::Project);
    require(explicit_engine
            && explicit_engine->source == FontSource::EngineBuiltIn
            && explicit_project
            && explicit_project->source == FontSource::Project,
        "explicit UI font sources must override the active global source exactly");

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(
        fixture.renderer(),
        registry,
        1280,
        720,
        &fixture.engine_cache(),
        &resolver);
    elysia::scene::SceneManager scene_manager;
    scene_manager.set_runtime_context(context);
    scene_manager.register_engine_scene<
        elysia::builtin::ApplicationFailureScene>(
            elysia::builtin::SceneKeys::ApplicationFailure);
    scene_manager.start(
        elysia::builtin::make_application_failure_route(
            elysia::builtin::ApplicationFailurePresentation::
                RuntimeFatal,
            "typography",
            "project font failure"));
    require(!resolver.project_fonts_active(),
        "entering ApplicationFailureScene must deactivate project fonts");
    const auto failure_scene_font = resolver.resolve_ui(
        UiTypographyRole::DialogBody,
        "zh-Hans");
    require(failure_scene_font
            && failure_scene_font->source
                == FontSource::EngineBuiltIn,
        "ApplicationFailureScene must render through Engine fonts");
    scene_manager.shutdown();
    require(resolver.activate_project_fonts().has_value(),
        "project fonts must remain valid after the failure-scene integration probe");

    fixture.resources().clear();
    const auto missing_after_activation = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en");
    require(!missing_after_activation
        && missing_after_activation.error().code
            == FontResolveErrorCode::FontUnavailable,
        "active project font loss must fail without Engine fallback");
    const auto explicit_engine_after_project_loss = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en",
        FontSource::EngineBuiltIn);
    const auto explicit_project_after_project_loss = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en",
        FontSource::Project);
    require(explicit_engine_after_project_loss
            && explicit_engine_after_project_loss->source
                == FontSource::EngineBuiltIn
            && !explicit_project_after_project_loss
            && explicit_project_after_project_loss.error().code
                == FontResolveErrorCode::FontUnavailable,
        "strict source overrides must not cross-fallback after project resource loss");

    resolver.deactivate_project_fonts();
    const auto after_deactivation = resolver.resolve_ui(
        UiTypographyRole::Label,
        "en");
    require(after_deactivation.has_value()
        && after_deactivation->source
            == FontSource::EngineBuiltIn,
        "deactivation must restore Engine bootstrap fonts");
}

void test_invalid_configuration(FontResolverFixture& fixture)
{
    FontResolver resolver;
    const auto unconfigured = resolver.resolve_effect(
        EffectTypographyRole::FloatingNumber);
    require(!unconfigured
        && unconfigured.error().code == FontResolveErrorCode::NotConfigured,
        "unconfigured FontResolver must return a clear error");

    const std::array<std::string,0> no_languages{};
    const auto invalid = resolver.configure(
        settings_with_size(
            FontSource::EngineBuiltIn,
            FontSource::EngineBuiltIn,
            20),
        fixture.engine_cache(),
        fixture.resource_service(),
        no_languages);
    require(!invalid
        && invalid.error().code == FontResolveErrorCode::UnsupportedLanguage,
        "FontResolver must reject an empty supported-language set");
}

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream stream(path,std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

void test_font_consumer_source_contract()
{
    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    const std::filesystem::path engine_root = source_root / "engine";

    bool consumer_uses_direct_font_lookup = false;
    for (const auto& entry :
        std::filesystem::recursive_directory_iterator(engine_root))
    {
        if (!entry.is_regular_file())
            continue;
        const std::filesystem::path path = entry.path();
        if (path.extension() != ".cpp" && path.extension() != ".h")
            continue;

        const std::string normalized = path.generic_string();
        if (normalized.find("/resources/") != std::string::npos
            || normalized.find("/assist/") != std::string::npos
            || normalized.find("/typography/") != std::string::npos
            || normalized.find("/tests/") != std::string::npos)
        {
            continue;
        }

        if (read_text(path).find("find_font(") != std::string::npos)
        {
            consumer_uses_direct_font_lookup = true;
            break;
        }
    }
    require(!consumer_uses_direct_font_lookup,
        "production font consumers must not bypass FontResolver");

    const std::string localization_source = read_text(
        engine_root / "localization" / "localization_manager.cpp");
    require(localization_source.find("map_font_key") == std::string::npos
        && localization_source.find("resolve_font(") == std::string::npos,
        "LocalizationManager must not retain its legacy font resolver");

    const std::string localized_style = read_text(
        engine_root / "localization" / "localized_text_style.h");
    require(localized_style.find("point_size") == std::string::npos,
        "LocalizedTextStyle must carry a semantic role instead of a point size");

    const std::string effect_source = read_text(
        engine_root / "effects" / "effect_manager.cpp");
    require(effect_source.find("\"ui.latin.20\"") == std::string::npos,
        "floating-number effects must not hard-code a project font key");

    const std::string asset_config_types = read_text(
        engine_root / "io" / "loaders" / "asset_config_types.h");
    const std::string font_manifest_loader = read_text(
        engine_root / "io" / "loaders" / "fonts_manifest_loader.cpp");
    require(asset_config_types.find("point_sizes") == std::string::npos
            && font_manifest_loader.find("\"sizes\"") == std::string::npos,
        "font manifests must not retain the removed size-list contract");
}
}

int main()
{
    FontResolverFixture fixture;
    test_engine_resolution_and_validation(fixture);
    test_atomic_project_activation(fixture);
    test_invalid_configuration(fixture);
    test_font_consumer_source_contract();
    return EXIT_SUCCESS;
}
