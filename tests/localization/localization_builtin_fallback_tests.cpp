#define SDL_MAIN_HANDLED

#include "engine/typography/font_settings.h"
#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
using elysia::tests::require;

std::size_t count_occurrences(std::string_view text,std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle,position)) != std::string_view::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}

void write_text_file(const std::filesystem::path& path,std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path,std::ios::binary | std::ios::trunc);
    require(output.good(),"localization test must create its temporary text file");
    output << text;
    require(output.good(),"localization test must write its temporary text file");
}

class LocalizationFixture
{
public:
    LocalizationFixture()
    {
        SDL_setenv("SDL_AUDIODRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
            "localization fallback tests must initialize SDL video and audio");
        require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
            "localization fallback tests must initialize PNG support");
        require(TTF_Init() == 0, "localization fallback tests must initialize SDL_ttf");
        require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
            "localization fallback tests must open SDL_mixer audio");
        _surface = SDL_CreateRGBSurfaceWithFormat(0, 128, 128, 32, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr, "localization fallback tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr, "localization fallback tests must create a software renderer");
    }

    ~LocalizationFixture()
    {
        elysia::localization::LocalizationManager::instance()->shutdown();
        elysia::resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(_renderer);
        SDL_FreeSurface(_surface);
        Mix_CloseAudio();
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* renderer() const noexcept { return _renderer; }

private:
    SDL_Surface* _surface = nullptr;
    SDL_Renderer* _renderer = nullptr;
};
}

int main()
{
    LocalizationFixture fixture;
    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    auto* path_manager = elysia::io::PathManager::instance();
    require(path_manager->initialize(source_root), "localization fallback tests must initialize project paths");

    elysia::builtin::BuiltinAssetCache cache;
    require(cache.initialize(
        fixture.renderer(),
        elysia::builtin::BuiltinAssetCatalog(source_root),
        std::array{20}).has_value(),
        "localization fallback tests must initialize built-in asset cache");

    auto* localization_manager = elysia::localization::LocalizationManager::instance();
    auto* localization = ELYSIA_LOCALIZATION;
    elysia::typography::FontResolver font_resolver;
    require(!localization_manager->is_initialized(),
        "LocalizationManager must begin uninitialized");
	const auto missing_manifest = localization_manager->initialize(
		fixture.renderer(),source_root / "assets/configs/manifests/missing_i18n.json",
		"en",&font_resolver,&cache);
	require(!missing_manifest
		&& missing_manifest.error().error_code() == "LOCALIZATION-MANIFEST"
		&& !missing_manifest.error().diagnostic.entries.empty()
		&& missing_manifest.error().diagnostic.entries.front().expected_path.filename()
			== "missing_i18n.json"
		&& !localization_manager->is_initialized(),
		"unhandled manifest failures must stay typed and must not publish initialized state");

	const auto fallback_root = std::filesystem::temp_directory_path()
		/ "elysia_localization_fallback_warning_tests";
	std::filesystem::remove_all(fallback_root);
	std::filesystem::create_directories(fallback_root);
	std::filesystem::copy(source_root / "assets",fallback_root / "assets",
		std::filesystem::copy_options::recursive);
	std::filesystem::remove_all(fallback_root / "assets/i18n/zh-Hans");
	require(path_manager->initialize(fallback_root),
		"localization fallback warning test must initialize isolated project paths");
	std::ostringstream captured_warning;
	std::streambuf* previous_log_buffer = std::clog.rdbuf(captured_warning.rdbuf());
	const auto fallback_result = localization_manager->initialize(
		fixture.renderer(),fallback_root / "assets/configs/manifests/i18n_manifest.json",
		"zh-Hans",&font_resolver,&cache);
	std::clog.rdbuf(previous_log_buffer);
	require(fallback_result && localization->current_language() == "en",
		"a requested locale failure must recover to the loaded default language");
	const std::string warning_text = captured_warning.str();
	const auto first_warning = warning_text.find("LOCALIZATION-LOCALE");
	require(first_warning != std::string::npos
		&& warning_text.find("LOCALIZATION-LOCALE",first_warning + 1) == std::string::npos,
		"a recovered requested-language failure must emit exactly one structured warning");
	localization_manager->shutdown();
	require(path_manager->initialize(source_root),
		"localization fallback warning test must restore source project paths");
	std::filesystem::remove_all(fallback_root);

    require(localization_manager->initialize(
        fixture.renderer(),
        source_root / "assets" / "configs" / "manifests" / "i18n_manifest.json",
        "en",
        &font_resolver,
        &cache),
        "LocalizationManager must initialize with built-in defaults");
    require(localization_manager->is_initialized(),
        "successful localization initialization must publish initialized state");

    elysia::typography::UiTypographyProfile::PointSizes point_sizes{};
    point_sizes.fill(20);
    elysia::typography::FontSettings font_settings;
    font_settings.ui.source =
        elysia::typography::FontSource::Project;
    font_settings.ui.typography_override =
        elysia::typography::UiTypographyProfile(point_sizes);
    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(font_settings);
    require(resolved_font_settings.has_value(),
        "localization fallback font settings must resolve");
    require(font_resolver.configure(
        *resolved_font_settings,
        cache,
        *elysia::resources::ResourceService::instance(),
        localization->supported_languages()).has_value(),
        "FontResolver must configure after localization publishes its languages");

    const elysia::localization::LocalizedTextStyle style{
        .typography_role =
            elysia::typography::UiTypographyRole::ButtonCompact
    };

    constexpr std::string_view missing_key = "test.localization.missing";
    constexpr std::string_view direct_texture_missing_key =
        "test.localization.direct_texture_missing";
    std::ostringstream missing_key_warnings;
    std::streambuf* previous_missing_key_log_buffer =
        std::clog.rdbuf(missing_key_warnings.rdbuf());

    require(localization->tr("common.save") == "Save",
        "project translations must remain the first lookup source");
    require(localization->tr("engine.settings.title") == "Settings",
        "missing Engine namespace keys must fall back to Engine translations");
    require(localization->tr(missing_key) == missing_key
            && localization->tr(missing_key) == missing_key,
        "fully missing translations must preserve the key fallback");

    SDL_Texture* missing_texture =
        localization->get_text_texture(missing_key,style);
    SDL_Texture* repeated_missing_texture =
        localization->get_text_texture(missing_key,style);
    elysia::localization::LocalizedTextStyle alternate_missing_style = style;
    alternate_missing_style.wrap_width = 240;
    SDL_Texture* alternate_missing_texture =
        localization->get_text_texture(missing_key,alternate_missing_style);
    SDL_Texture* direct_missing_texture = localization->get_text_texture(
        direct_texture_missing_key,style);
    SDL_Texture* raw_key_like_texture =
        localization->get_raw_text_texture(missing_key,style);
    require(missing_texture && repeated_missing_texture == missing_texture
            && alternate_missing_texture && direct_missing_texture
            && raw_key_like_texture,
        "missing key and raw text fallbacks must continue producing cached textures");

    require(localization->set_language("zh-Hans"),
        "missing-key warning test must switch to Simplified Chinese");
    require(localization->tr(missing_key) == missing_key
            && localization->tr(missing_key) == missing_key,
        "the same missing key must retain its fallback in a new locale");
    require(localization->set_language("en"),
        "missing-key warning test must switch back to English");
    require(localization->tr(missing_key) == missing_key,
        "switching back must preserve the original missing-key fallback");

    std::clog.rdbuf(previous_missing_key_log_buffer);
    const std::string missing_warning_text = missing_key_warnings.str();
    require(count_occurrences(
            missing_warning_text,
            "Missing localization key: key='test.localization.missing', locale='en'") == 1,
        "a missing key must warn once for its English locale across all key entry points");
    require(count_occurrences(
            missing_warning_text,
            "Missing localization key: key='test.localization.missing', locale='zh-Hans'") == 1,
        "the same missing key must warn once in a different locale");
    require(count_occurrences(
            missing_warning_text,
            "Missing localization key: key='test.localization.direct_texture_missing', locale='en'") == 1,
        "a first request through the texture entry point must emit one missing-key warning");
    require(count_occurrences(missing_warning_text,"Missing localization key:") == 3,
        "known translations, successful fallbacks and raw text must not emit missing-key warnings");

    const std::array legacy_locales{
        std::string("zh") + "_cn",
        std::string("zh_") + "Hans",
        std::string("zh_") + "Hant"
    };
    for (const std::string& legacy_locale : legacy_locales)
    {
		auto language_result = localization->set_language(legacy_locale);
        require(!language_result
			&& language_result.error().error_code() == "LOCALIZATION-LANGUAGE"
			&& localization->current_language() == "en",
            "LocalizationService must return typed errors and retain the current language");
    }

    SDL_Texture* engine_text_texture =
        localization->get_text_texture("engine.settings.title",style);
    require(engine_text_texture != nullptr,
        "Engine default font must render text before project content fonts load");

    auto* resources = elysia::resources::ResourceManager::instance();
    require(resources->load_font(
        "ui.latin.20",
        path_manager->fonts() / "fusion-pixel-10px-proportional-latin.ttf",
        20),
        "test must load a project font for precedence validation");
    require(resources->load_font(
        "ui.zh_hans.20",
        path_manager->fonts() / "fusion-pixel-10px-proportional-zh_hans.ttf",
        20),
        "test must load the Simplified Chinese project font");
    require(resources->load_font(
        "ui.zh_hant.20",
        path_manager->fonts() / "fusion-pixel-10px-proportional-zh_hant.ttf",
        20),
        "test must load the Traditional Chinese project font");
    require(resources->load_font(
        "ui.ja.20",
        path_manager->fonts() / "fusion-pixel-10px-proportional-ja.ttf",
        20),
        "test must load the Japanese project font");
    require(resources->load_font(
        "ui.ko.20",
        path_manager->fonts() / "fusion-pixel-10px-proportional-ko.ttf",
        20),
        "test must load the Korean project font");
    int localized_width = 0;
    int localized_height = 0;
    require(localization->measure_raw_text("Moon", style, localized_width, localized_height),
        "localized measurement must succeed with a project font present");
    int engine_width = 0;
    int engine_height = 0;
    require(TTF_SizeUTF8(cache.find_font("en", 20), "Moon", &engine_width, &engine_height) == 0,
        "Engine font must measure the precedence probe text");
    require(localized_width == engine_width && localized_height == engine_height,
        "Engine fonts must remain active until project activation");

    require(font_resolver.activate_project_fonts().has_value(),
        "complete project fonts must activate");
    SDL_Texture* project_text_texture =
        localization->get_text_texture("engine.settings.title",style);
    require(project_text_texture
        && project_text_texture != engine_text_texture,
        "font generation changes must not reuse Engine text textures");
    require(localization->measure_raw_text("Moon", style, localized_width, localized_height),
        "active project fonts must render localized text");
    int project_width = 0;
    int project_height = 0;
    require(TTF_SizeUTF8(ELYSIA_RESOURCES->find_font("ui.latin.20"), "Moon", &project_width, &project_height) == 0,
        "project font must measure the fallback probe text");
    require(localized_width == project_width && localized_height == project_height,
        "LocalizationService must render through the active project font");

    elysia::localization::LocalizedTextStyle engine_override_style = style;
    engine_override_style.font_source_override =
        elysia::typography::FontSource::EngineBuiltIn;
    SDL_Texture* inherited_raw_texture =
        localization->get_raw_text_texture("Moon",style);
    SDL_Texture* engine_raw_texture =
        localization->get_raw_text_texture("Moon",engine_override_style);
    require(inherited_raw_texture && engine_raw_texture
            && inherited_raw_texture != engine_raw_texture,
        "text texture cache keys must distinguish inherited and explicit Engine fonts");

    int overridden_width = 0;
    int overridden_height = 0;
    require(localization->measure_raw_text(
            "Moon",
            engine_override_style,
            overridden_width,
            overridden_height)
            && overridden_width == engine_width
            && overridden_height == engine_height,
        "LocalizationService measurement must honor an explicit Engine font source");

    localization_manager->shutdown();

    const std::filesystem::path resolution_root =
        std::filesystem::temp_directory_path()
        / "elysia_localization_resolution_warning_tests";
    std::filesystem::remove_all(resolution_root);
    std::filesystem::create_directories(resolution_root);
    std::filesystem::copy(
        source_root / "assets",
        resolution_root / "assets",
        std::filesystem::copy_options::recursive);
    write_text_file(
        resolution_root / "assets/configs/manifests/i18n_manifest.json",
        R"({"default_language":"en","languages":["en","zh-Hans"],"file":["base.json"]})");
    write_text_file(
        resolution_root / "assets/i18n/en/base.json",
        R"({"test":{"same_as_key":"test.same_as_key","fallback_only":"Default fallback"}})");
    write_text_file(
        resolution_root / "assets/i18n/zh-Hans/base.json",
        R"({"test":{}})");
    require(path_manager->initialize(resolution_root),
        "translation resolution test must initialize isolated project paths");
    require(localization_manager->initialize(
            fixture.renderer(),
            resolution_root / "assets/configs/manifests/i18n_manifest.json",
            "en",
            &font_resolver,
            &cache),
        "translation resolution test must reinitialize localization");

    std::ostringstream resolution_warnings;
    std::streambuf* previous_resolution_log_buffer =
        std::clog.rdbuf(resolution_warnings.rdbuf());
    require(localization->tr("test.same_as_key") == "test.same_as_key",
        "a translation equal to its key must still count as an explicit translation");
    require(localization->tr(missing_key) == missing_key
            && localization->tr(missing_key) == missing_key,
        "reinitialization must retain the missing-key visual fallback");
    require(localization->set_language("zh-Hans"),
        "translation resolution test must switch to its secondary locale");
    require(localization->tr("test.fallback_only") == "Default fallback",
        "a missing current-locale entry must fall back to the default language");
    std::clog.rdbuf(previous_resolution_log_buffer);

    const std::string resolution_warning_text = resolution_warnings.str();
    require(count_occurrences(
            resolution_warning_text,
            "Missing localization key: key='test.localization.missing', locale='en'") == 1,
        "shutdown and reinitialize must reset missing-key warning deduplication");
    require(count_occurrences(resolution_warning_text,"Missing localization key:") == 1,
        "equal-to-key translations and successful default fallbacks must not warn");

    localization_manager->shutdown();
    localization_manager->shutdown();
    require(!localization_manager->is_initialized(),
        "localization shutdown must be idempotent and clear initialized state");
    require(path_manager->initialize(source_root),
        "translation resolution test must restore source project paths");
    std::filesystem::remove_all(resolution_root);
    font_resolver.shutdown();
    cache.shutdown();
    return 0;
}
