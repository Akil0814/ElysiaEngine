#define SDL_MAIN_HANDLED
#include "engine/ui/composites/ui_option_selector.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "tests/support/sdl_audio_fixture.h"
#include "tests/support/test_assertions.h"

#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace
{
using namespace elysia;
using namespace elysia::ui;
using tests::require;

class RenderFixture
{
public:
    RenderFixture()
    {
        SDL_setenv_unsafe("SDL_VIDEO_DRIVER","dummy",1);
        SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),"SDL initializes for selector rendering");
        require(TTF_Init(),"selector fonts initialize");
        require(tests::open_test_mixer(),"selector test mixer initializes");
        surface = SDL_CreateSurface(1000,500,SDL_PIXELFORMAT_RGBA32);
        require(surface != nullptr,"selector software surface creates");
        renderer = SDL_CreateSoftwareRenderer(surface);
        require(renderer != nullptr,"selector software renderer creates");
        auto* paths = io::PathManager::instance();
        require(paths->initialize(ELYSIA_SOURCE_DIR),"selector paths initialize");
        const auto settings = typography::resolve_font_settings({});
        require(settings.has_value(),"selector font settings resolve");
        require(builtin::BuiltinResources::instance()->initialize(renderer,
            builtin::BuiltinAssetCatalog(*paths),settings->engine_point_sizes()).has_value(),"selector built-in fonts initialize");
        require(localization::LocalizationManager::instance()->initialize(renderer,
            paths->configs()/"manifests"/"i18n_manifest.json","en",&fonts).has_value(),"selector localization initializes");
        require(fonts.configure(*settings,*resources::ResourceService::instance(),
            ELYSIA_LOCALIZATION->supported_languages()).has_value(),"selector font resolver configures");
    }
    ~RenderFixture()
    {
        localization::LocalizationManager::instance()->shutdown();
        fonts.shutdown();
        builtin::BuiltinResources::instance()->shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
        tests::close_test_mixer();
        TTF_Quit();
        SDL_Quit();
    }
    void capture(const std::vector<core::UiRenderCommand>& commands,const std::string& name)
    {
        SDL_SetRenderDrawColor(renderer,22,26,36,255);
        require(SDL_RenderClear(renderer),"selector software canvas clears");
        require(core::execute_render_commands(renderer,commands).has_value(),"selector commands execute");
        require(SDL_RenderPresent(renderer),"selector software canvas presents");
        if (const auto* directory = SDL_getenv("ELYSIA_SELECTOR_QA_DIR"))
        {
            std::filesystem::create_directories(directory);
            require(IMG_SavePNG(surface,(std::filesystem::path(directory)/(name+".png")).string().c_str()),
                "selector screenshot saves");
        }
    }
    SDL_Surface* surface = nullptr;
    SDL_Renderer* renderer = nullptr;
    typography::FontResolver fonts;
};

std::vector<core::UiRenderCommand> commands(const UiOptionSelector& selector)
{
    std::vector<core::UiRenderCommand> result;
    selector.submit_ui_render_commands(result);
    return result;
}

std::vector<core::UiRenderCommand> lines(const std::vector<core::UiRenderCommand>& all)
{
    std::vector<core::UiRenderCommand> result;
    for (const auto& command : all)
        if (command.type == core::UiRenderCommandType::DrawLine) result.push_back(command);
    return result;
}

const core::UiRenderCommand* text(const std::vector<core::UiRenderCommand>& all)
{
    for (const auto& command : all)
        if (command.type == core::UiRenderCommandType::Texture) return &command;
    return nullptr;
}

void test_layout_states_and_cached_text(RenderFixture& fixture)
{
    UiOptionSelector selector(core::Rect{20,20,360,48});
    selector.set_options({{ui_raw_text("A long localized option name for width fitting")},{ui_raw_text("Other")}});
    const auto initial = commands(selector);
    require(text(initial) != nullptr && lines(initial).size() == 4,"selectors must render text and two geometric arrows");
    require(lines(initial)[0].color == selector.style().arrow.text.disabled
        && lines(initial)[2].color == selector.style().arrow.text.enabled,"bounded endpoints must resolve disabled arrow colors");
    for (const auto orientation : {UiOptionSelectorOrientation::Horizontal,UiOptionSelectorOrientation::Vertical})
    {
        selector.set_orientation(orientation);
        const std::array<core::Vector2,3> sizes = orientation == UiOptionSelectorOrientation::Horizontal
            ? std::array<core::Vector2,3>{{{360,48},{80,40},{6,10}}}
            : std::array<core::Vector2,3>{{{220,144},{60,80},{8,6}}};
        for (const auto size : sizes)
        {
            selector.set_size(size);
            const auto all = commands(selector);
            for (const auto& command : all)
            {
                if (command.type == core::UiRenderCommandType::Texture)
                    require(selector.screen_rect().contains(command.screen_rect),"fitted text must stay within the selector");
                if (command.type == core::UiRenderCommandType::DrawLine)
                    require(selector.screen_rect().contains(command.line_start) && selector.screen_rect().contains(command.line_end),
                        "arrow geometry must stay within the selector at all sizes");
            }
            if (size.x > 100)
            {
                require(text(all) != nullptr && text(all)->texture == text(initial)->texture,
                    "direction and size changes must reuse the same localized text texture");
                const auto center = text(all)->screen_rect.center();
                require(std::abs(center.x - selector.screen_rect().center().x) < 0.001f
                    && std::abs(center.y - selector.screen_rect().center().y) < 0.001f,"text must stay centered in both orientations");
            }
        }
    }
    selector.set_orientation(UiOptionSelectorOrientation::Horizontal);
    selector.set_screen_rect({20,20,360,48});
    selector.set_focused(true);
    const auto focused = commands(selector);
    require(selector.on_ui_input_event({.action=UiAction::Confirm,.type=UiInputEventType::ActionPressed,
        .device=input::InputDevice::Keyboard}),"selector must enter adjustment");
    const auto adjusting = commands(selector);
    require(focused[1].color == selector.style().chrome.border.focused
        && adjusting[1].color == selector.style().chrome.border.active,"adjustment must have a distinct active border");
    selector.set_opacity(128);
    const auto translucent = commands(selector);
    require(text(translucent)->alpha == 128,"text opacity must follow the selector");
    for (const auto& line : lines(translucent))
        require(line.color.a <= 128,"arrow opacity must follow the selector");
    selector.set_opacity(255);
    selector.set_enabled(false);
    const auto disabled = commands(selector);
    require(!selector.is_adjusting() && lines(disabled)[2].color == selector.style().arrow.text.disabled,
        "disabling adjustment must disable both arrows");
    selector.set_enabled(true);
    selector.clear_options();
    const auto empty = commands(selector);
    require(text(empty) != nullptr && lines(empty)[0].color == lines(empty)[2].color,"empty selectors must show a placeholder and disabled arrows");
    selector.set_options({{ui_raw_text("Only")}});
    selector.set_wrap_selection(true);
    require(lines(commands(selector))[2].color == selector.style().arrow.text.disabled,"single options must not enable wrapping arrows");
    selector.set_visible(false);
    require(commands(selector).empty(),"hidden selectors must submit no commands");
    (void)fixture;
}

void test_all_themes_and_languages(RenderFixture& fixture)
{
    UiPanel root(core::Rect{0,0,1000,500});
    auto* horizontal = root.create_child<UiOptionSelector>(core::Rect{40,50,420,52});
    auto* vertical = root.create_child<UiOptionSelector>(core::Rect{520,50,350,180});
    horizontal->set_options({{ui_text_key("ui_component_gallery.controls.selector.long_name")},{ui_raw_text("Other")}});
    vertical->set_options(horizontal->options());
    vertical->set_orientation(UiOptionSelectorOrientation::Vertical);
    vertical->set_wrap_selection(true);
    horizontal->set_style_overrides({.chrome={.corner_radius=7.0f},.arrow_extent=32.0f});
    UiThemeManager themes;
    auto registration = themes.register_root(root);
    const std::array theme_cases{UiBuiltinTheme::BlueGlassMoon,UiBuiltinTheme::ElysiaLight,UiBuiltinTheme::ElysiaDark,
        UiBuiltinTheme::EvangelionUnit00,UiBuiltinTheme::EvangelionUnit01,UiBuiltinTheme::EvangelionUnit02,UiBuiltinTheme::QuietSlate};
    for (const auto language : {"en","ja","ko","zh-Hans","zh-Hant"})
    {
        require(ELYSIA_LOCALIZATION->set_language(language).has_value(),"selector display language switches");
        for (std::size_t index = 0; index < theme_cases.size(); ++index)
        {
            themes.set_theme(theme_cases[index]);
            require(horizontal->style().chrome.corner_radius == 7 && horizontal->style().arrow_extent == 32,
                "local overrides must survive theme changes");
            require(vertical->style().text.enabled == themes.current_theme().label(UiLabelVisualRole::Default).text,
                "the selector adapter must follow label theme colors");
            auto all = commands(*horizontal);
            const auto other = commands(*vertical);
            require(text(all) != nullptr && text(other) != nullptr,"both orientations must resolve every display language");
            all.insert(all.end(),other.begin(),other.end());
            fixture.capture(all,std::string(language)+"-theme-"+std::to_string(index));
        }
    }
}
}

int main()
{
    RenderFixture fixture;
    test_layout_states_and_cached_text(fixture);
    test_all_themes_and_languages(fixture);
    return EXIT_SUCCESS;
}
