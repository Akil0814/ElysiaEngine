#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_asset_cache.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/widgets/number/ui_number.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <cstdlib>
#include <vector>

namespace
{
using elysia::tests::require;

std::vector<elysia::core::UiRenderCommand> texture_commands(
    const std::vector<elysia::core::UiRenderCommand>& commands
)
{
    std::vector<elysia::core::UiRenderCommand> textures;
    for (const auto& command : commands)
    {
        if (command.type == elysia::core::UiRenderCommandType::Texture)
            textures.push_back(command);
    }
    return textures;
}

void test_ui_number_uses_shared_localized_glyphs()
{
    using namespace elysia;
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0,
        "UI number tests must initialize SDL video and audio");
    require((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG,
        "UI number tests must initialize PNG support");
    require(TTF_Init() == 0,"UI number tests must initialize SDL_ttf");
    require(Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0,
        "UI number tests must open SDL_mixer audio");
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0,256,128,32,SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr,"UI number tests must create a software surface");
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr,"UI number tests must create a software renderer");

    auto* paths = io::PathManager::instance();
    auto* resources = resources::ResourceManager::instance();
    auto* localization_manager = localization::LocalizationManager::instance();
    auto* localization = ELYSIA_LOCALIZATION;
    require(paths->init(),"UI number tests must initialize paths");
    resources->clear();
    const auto resolved_font_settings =
        typography::resolve_font_settings(typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "UI number default font settings must resolve");
    builtin::BuiltinAssetCache engine_cache;
    require(engine_cache.initialize(
        renderer,
        builtin::BuiltinAssetCatalog(*paths),
        resolved_font_settings->engine_point_sizes()).has_value(),
        "UI number tests must initialize built-in fonts");
    typography::FontResolver font_resolver;
    localization_manager->shutdown();
    require(localization_manager->init(
        renderer,
        paths->configs() / "manifests" / "i18n_manifest.json",
        "en",
        &font_resolver,
        &engine_cache
    ),"UI number tests must initialize localization");
    require(font_resolver.configure(
        *resolved_font_settings,
        engine_cache,
        *resources::ResourceService::instance(),
        localization->supported_languages()).has_value(),
        "UI number tests must configure Engine fonts");

    {
        ui::UiNumber number(core::Rect{ 10,20,200,60 });
        number.set_value(-12.5);
        number.set_decimal_places(1);
        number.set_trim_trailing_zeros(false);
        number.set_suffix(ui::UiNumberSuffix::Percent);
        number.set_target_height(20.0f);
        number.set_digit_spacing(3.0f);
        number.set_fixed_glyph_advance(12.0f);
        number.set_horizontal_align(ui::TextHorizontalAlign::Center);
        number.set_vertical_align(ui::TextVerticalAlign::Bottom);
        number.set_opacity(128);

        std::vector<core::UiRenderCommand> commands;
        number.submit_ui_render_commands(commands);
        const auto first = texture_commands(commands);
        require(first.size() == 6,"negative decimal percent values must render one command per glyph");
        require(first.front().screen_rect.x() == 66.5f
            && first.front().screen_rect.y() == 60.0f
            && first.front().screen_rect.height() == 20.0f,
            "centered fixed-advance layout must preserve spacing and bottom alignment");
        require(first.front().alpha == 128,"widget opacity must apply to every number glyph");
        require(first.back().screen_rect.x() == 141.5f,"percent glyph must occupy the final fixed-advance column");

        commands.clear();
        number.submit_ui_render_commands(commands);
        const auto repeated = texture_commands(commands);
        require(repeated.size() == first.size(),"unchanged UI numbers must remain renderable");
        for (std::size_t index = 0; index < first.size(); ++index)
            require(repeated[index].texture == first[index].texture,"UI numbers must reuse localized glyph textures");

        localization_manager->clear_texture_cache();
        commands.clear();
        number.submit_ui_render_commands(commands);
        require(texture_commands(commands).size() == 6,"UI numbers must rebuild after shared cache clearing");

        require(localization->set_language("zh-Hans"),"UI number tests must switch language");
        commands.clear();
        number.submit_ui_render_commands(commands);
        require(texture_commands(commands).size() == 6,"UI number glyphs must follow the active language font mapping");

        number.set_horizontal_align(ui::TextHorizontalAlign::Right);
        number.set_vertical_align(ui::TextVerticalAlign::Top);
        commands.clear();
        number.submit_ui_render_commands(commands);
        const auto right_aligned = texture_commands(commands);
        require(!right_aligned.empty() && right_aligned.front().screen_rect.x() == 123.0f
            && right_aligned.front().screen_rect.y() == 20.0f,
            "right and top alignment must be applied by the UI widget");
    }

    localization_manager->shutdown();
    font_resolver.shutdown();
    resources->clear();
    engine_cache.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}
}

int main()
{
    test_ui_number_uses_shared_localized_glyphs();
    return EXIT_SUCCESS;
}
