#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/typography/font_settings.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/test_assertions.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/style/ui_style_defaults.h"
#include "engine/ui/window/ui_window.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/text/ui_text_block.h"
#include "engine/core/render/sdl_render_command_executor.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
using elysia::tests::require;

elysia::ui::UiDropdown* find_dropdown(elysia::ui::UiChildHost& host)
{
    for (std::size_t i = 0; i < host.child_count(); ++i)
    {
        if (auto* dropdown = dynamic_cast<elysia::ui::UiDropdown*>(host.child_at(i)))
            return dropdown;
        if (auto* child = dynamic_cast<elysia::ui::UiChildHost*>(host.child_at(i)))
            if (auto* found = find_dropdown(*child)) return found;
    }
    return nullptr;
}

void test_native_language_dropdown(SDL_Renderer* renderer)
{
    using namespace elysia;
    auto* localization = ELYSIA_LOCALIZATION;
    auto* builtin = builtin::BuiltinResources::instance();
    const auto languages = localization->supported_languages();
    const std::array<std::string,5> names{ "English","简体中文","繁體中文","日本語","한국어" };
    const std::array<std::string,5> locales{ "en","zh-Hans","zh-Hant","ja","ko" };
    for (std::size_t i = 0; i < locales.size(); ++i)
    {
        const auto locale = builtin::builtin_locale_id(locales[i]);
        const auto* name = builtin->find_translation(locale,"engine.settings.native_name");
        require(name && *name == names[i],"each locale must store its own native name");
        const char* cursor = name->c_str();
        while (*cursor)
            require(TTF_FontHasGlyph(builtin->find_font(builtin::builtin_font_id(locale),20),
                SDL_StepUTF8(&cursor,nullptr)),"native-name fonts must contain every displayed glyph");
    }

    ui::SettingsPanel panel(core::Rect{0,0,700,500},{
        .window_mode = false,.target_fps = false,.vsync = false,
        .master_volume = false,.music_volume = false,.sound_volume = false,.language = true });
    auto* scroll = dynamic_cast<ui::UiScrollContainer*>(panel.child_at(1));
    require(scroll != nullptr,"settings must have a content scroll container");
    auto* content = dynamic_cast<ui::UiChildHost*>(const_cast<ui::UiElement*>(scroll->content()));
    require(content != nullptr,"settings must have content");
    auto* dropdown = find_dropdown(*content);
    require(dropdown != nullptr,"language-only settings must contain a language dropdown");
    ui::UiWindow window(core::Rect{0,0,800,600});
    dropdown->register_with_window(window);

    for (const auto& current : languages)
    {
        require(localization->set_language(current),"native names must work in each UI language");
        panel.set_options({ .languages = languages });
        dropdown->set_screen_rect(core::Rect{20,20,300,44});
        const auto& options = dropdown->options();
        require(options.size() == languages.size(),"all supported language options must be present");
        std::vector<SDL_Texture*> expected;
        for (std::size_t i = 0; i < options.size(); ++i)
        {
            require(options[i].content.kind == ui::UiTextContentKind::RawText
                && options[i].font_language_override == languages[i]
                && options[i].font_source_override == typography::FontSource::EngineBuiltIn,
                "language options must carry raw names and their own Engine font");
            const auto* native = builtin->find_translation(builtin::builtin_locale_id(languages[i]),
                "engine.settings.native_name");
            require(native && options[i].content.value == *native,"names must be independent of the UI locale");
            localization::LocalizedTextStyle style;
            style.typography_role = typography::UiTypographyRole::Button;
            style.font_source_override = typography::FontSource::EngineBuiltIn;
            style.font_language_override = languages[i];
            style.color = ui::UiStyleDefaults::button().text.enabled;
            auto* texture = localization->get_raw_text_texture(*native,style);
            require(texture != nullptr,"native names must render with project fonts active");
            expected.push_back(texture);
            require(dropdown->set_selected_index(i),"every native option must be selectable");
            std::vector<core::UiRenderCommand> trigger;
            dropdown->submit_ui_render_commands(trigger);
            require(std::ranges::any_of(trigger,[texture](const auto& command) {
                return command.texture == texture;
            }),"the collapsed trigger must use the selected option's font");
            int width = 0,height = 0,expected_width = 0,expected_height = 0;
            require(localization->measure_raw_text(*native,style,width,height)
                && TTF_GetStringSize(builtin->find_font(builtin::builtin_font_id(
                    builtin::builtin_locale_id(languages[i])),20),native->c_str(),0,
                    &expected_width,&expected_height)
                && width == expected_width && height == expected_height,
                "native-name measurement must use the same font as rendering");
        }
        dropdown->open();
        require(dropdown->is_open(),"native language popup must open");
        std::vector<core::UiRenderCommand> popup;
        dropdown->submit_popup_render_commands(popup);
        for (auto* texture : expected)
            require(std::ranges::any_of(popup,[texture](const auto& command) {
                return command.texture == texture;
            }),"every popup row must use its own font");
        require(localization->current_language() == current,"rendering options must not change the UI locale");
        if (const char* directory = SDL_getenv("ELYSIA_LANGUAGE_QA_DIR"))
        {
            SDL_SetRenderDrawColor(renderer,22,26,36,255);
            require(SDL_RenderClear(renderer),"language QA canvas must clear");
            dropdown->submit_ui_render_commands(popup);
            require(core::execute_render_commands(renderer,popup).has_value(),"language QA commands must render");
            std::filesystem::create_directories(directory);
            SDL_Surface* capture = SDL_RenderReadPixels(renderer,nullptr);
            require(capture != nullptr,"language QA pixels must be readable");
            require(IMG_SavePNG(capture,(std::filesystem::path(directory)/(current+".png")).string().c_str()),
                "language QA image must save");
            SDL_DestroySurface(capture);
        }
        dropdown->close();
        dropdown->open();
        require(dropdown->is_open(),"language popup must reopen after selection");
        dropdown->clear_options();
        std::vector<core::UiRenderCommand> cleared;
        dropdown->submit_ui_render_commands(cleared);
        require(!dropdown->is_open() && !dropdown->selected_index()
            && std::ranges::none_of(cleared,[](const auto& command) { return command.texture != nullptr; }),
            "clearing language options must clear the trigger and close the popup");
        dropdown->set_options({ {ui::ui_raw_text("Moon")} });
        localization::LocalizedTextStyle plain;
        plain.typography_role = typography::UiTypographyRole::Button;
        plain.color = ui::UiStyleDefaults::button().text.enabled;
        auto* plain_texture = localization->get_raw_text_texture("Moon",plain);
        cleared.clear();
        dropdown->submit_ui_render_commands(cleared);
        require(plain_texture && std::ranges::any_of(cleared,[plain_texture](const auto& command) {
            return command.texture == plain_texture;
        }),"ordinary options must clear previous font language and source overrides");
    }
    panel.set_options({.languages = {"unknown-locale"}});
    require(dropdown->options().front().content.value == "unknown-locale"
        && !dropdown->options().front().font_language_override
        && !dropdown->options().front().font_source_override,
        "unknown languages must show their identifier without an invalid font mapping");
    require(localization->set_language("en"),"language tests must restore English");
}

void test_text_widgets_font_language()
{
    using namespace elysia;
    auto* localization = ELYSIA_LOCALIZATION;
    ui::UiLabel label(core::Rect{0,0,300,50});
    ui::UiTextBlock block(core::Rect{0,0,80,10});
    block.set_padding(4);
    label.set_font_source_override(typography::FontSource::EngineBuiltIn);
    block.set_font_source_override(typography::FontSource::EngineBuiltIn);
    label.set_font_language_override("ko");
    block.set_font_language_override("ko");

    const auto check_render = [&](const auto& widget,const auto& style,const ui::UiTextContent& content)
    {
        auto* expected = content.kind == ui::UiTextContentKind::TextKey
            ? localization->get_text_texture(content.value,style)
            : localization->get_raw_text_texture(content.value,style);
        std::vector<core::UiRenderCommand> commands;
        widget.submit_ui_render_commands(commands);
        require(expected && std::ranges::any_of(commands,[expected](const auto& command) {
            return command.texture == expected;
        }),"text widgets must render with their requested font language");
    };

    // Project fonts are active: source and language overrides must both reach rendering.
    for (const auto& current : {"en","ja"})
    {
        require(localization->set_language(current),"text widget tests must switch UI languages");
        for (const auto& content : {ui::ui_raw_text("한국어 한국어 한국어 한국어"),
                ui::ui_text_key("engine.settings.title")})
        {
            label.set_text_content(content);
            block.set_text_content(content);
            localization::LocalizedTextStyle label_style;
            label_style.typography_role = label.typography_role();
            label_style.color = label.style().text;
            label_style.font_source_override = typography::FontSource::EngineBuiltIn;
            label_style.font_language_override = "ko";
            check_render(label,label_style,content);
            auto block_style = label_style;
            block_style.typography_role = block.typography_role();
            block_style.color = block.style().text;
            block_style.wrap_width = 72;
            check_render(block,block_style,content);

            const std::string text = content.kind == ui::UiTextContentKind::TextKey
                ? std::string(localization->tr(content.value)) : content.value;
            int width = 0,height = 0;
            require(localization->measure_raw_text(text,block_style,width,height),
                "overridden multi-line text must be measurable");
            require(block.content_extent().nearly_equals(core::Vector2(
                    std::max(80.0f,static_cast<float>(width+8)),
                    std::max(10.0f,static_cast<float>(height+8)))),
                "text block wrapping and content extent must use the rendered font language");
            require(localization->current_language() == current,
                "font overrides must leave translation lookup and global locale unchanged");

            label.clear_font_language_override();
            block.clear_font_language_override();
            label_style.font_language_override.reset();
            block_style.font_language_override.reset();
            check_render(label,label_style,content);
            check_render(block,block_style,content);
            label.set_font_language_override("ko");
            block.set_font_language_override("ko");
        }
    }
    label.set_font_language_override("");
    block.set_font_language_override("");
    require(!label.font_language_override() && !block.font_language_override(),
        "empty font language must restore inheritance");
    label.set_font_language_override("ja");
    block.set_font_language_override("ja");
    label.reset();
    block.reset();
    require(!label.font_language_override() && !block.font_language_override(),
        "reset must clear text widget font language overrides");
    require(localization->set_language("en"),"text widget tests must restore English");
}

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
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
            "localization fallback tests must initialize SDL video and audio");
        require(TTF_Init(), "localization fallback tests must initialize SDL_ttf");
        require(elysia::tests::open_test_mixer(),
            "localization fallback tests must open SDL_mixer audio");
	_surface = SDL_CreateSurface(800, 600, SDL_PIXELFORMAT_RGBA32);
        require(_surface != nullptr, "localization fallback tests must create a software surface");
        _renderer = SDL_CreateSoftwareRenderer(_surface);
        require(_renderer != nullptr, "localization fallback tests must create a software renderer");
    }

    ~LocalizationFixture()
    {
        elysia::localization::LocalizationManager::instance()->shutdown();
        elysia::resources::ResourceManager::instance()->clear();
        SDL_DestroyRenderer(_renderer);
        SDL_DestroySurface(_surface);
        elysia::tests::close_test_mixer();
        TTF_Quit();

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

    auto& builtin_resources = *elysia::builtin::BuiltinResources::instance();
    require(builtin_resources.initialize(
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
		"en",&font_resolver);
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
		"zh-Hans",&font_resolver);
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
        &font_resolver),
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
    require(localization->tr("engine.settings.status.save_failed") == "Failed to save settings",
        "save failure must fall back to a short built-in translation");
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
    require(TTF_GetStringSize(builtin_resources.find_font(
            elysia::builtin::BuiltinFontId::Latin, 20), "Moon", 0, &engine_width, &engine_height),
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
    require(TTF_GetStringSize(ELYSIA_RESOURCES->find_font("ui.latin.20"), "Moon", 0, &project_width, &project_height),
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

    auto korean_style = engine_override_style;
    korean_style.font_language_override = "ko";
    auto* korean_raw = localization->get_raw_text_texture("Moon",korean_style);
    auto* korean_key = localization->get_text_texture("engine.settings.title",korean_style);
    auto* engine_key = localization->get_text_texture("engine.settings.title",engine_override_style);
    require(korean_raw && korean_raw != engine_raw_texture && korean_key && korean_key != engine_key,
        "raw and translated caches must distinguish font language overrides");
    require(localization->get_raw_text_texture("Moon",korean_style) == korean_raw
        && localization->get_text_texture("engine.settings.title",korean_style) == korean_key,
        "identical font-language requests must reuse their cached textures");
    auto uncached = localization->create_uncached_raw_text_texture("한국어",korean_style);
    require(uncached != nullptr,"uncached text must also support font language overrides");
    uncached.reset();
    test_native_language_dropdown(fixture.renderer());
    test_text_widgets_font_language();

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
            &font_resolver),
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
    builtin_resources.shutdown();
    return 0;
}
