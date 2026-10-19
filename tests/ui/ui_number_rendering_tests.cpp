#include "tests/support/sdl_audio_fixture.h"
#define SDL_MAIN_HANDLED

#include "engine/builtin/resources/builtin_resources.h"
#include "engine/builtin/resources/builtin_asset_catalog.h"
#include "engine/io/path/path_manager.h"
#include "engine/localization/localization_manager.h"
#include "engine/localization/localization_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/resource_service.h"
#include "engine/typography/font_resolver.h"
#include "engine/ui/widgets/number/ui_number.h"
#include "tests/support/test_assertions.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

std::vector<elysia::core::UiRenderCommand> render_glyphs(const elysia::ui::UiNumber& number)
{
    std::vector<elysia::core::UiRenderCommand> commands;
    number.submit_ui_render_commands(commands);
    return texture_commands(commands);
}

elysia::core::Rect glyph_bounds(const std::vector<elysia::core::UiRenderCommand>& glyphs)
{
    require(!glyphs.empty(),"glyph bounds require a nonempty run");
    float left = glyphs.front().screen_rect.x();
    float top = glyphs.front().screen_rect.y();
    float right = glyphs.front().screen_rect.right();
    float bottom = glyphs.front().screen_rect.bottom();
    for (const auto& glyph : glyphs)
    {
        left = std::min(left,glyph.screen_rect.x());
        top = std::min(top,glyph.screen_rect.y());
        right = std::max(right,glyph.screen_rect.right());
        bottom = std::max(bottom,glyph.screen_rect.bottom());
    }
    return {left,top,right - left,bottom - top};
}

void require_close(float actual,float expected,const char* message)
{
    require(std::abs(actual - expected) <= 0.001f,message);
}

void test_fitting_preserves_geometry_and_alignment()
{
    using namespace elysia;
    const std::array horizontal_alignments{
        ui::TextHorizontalAlign::Left,ui::TextHorizontalAlign::Center,ui::TextHorizontalAlign::Right
    };
    const std::array vertical_alignments{
        ui::TextVerticalAlign::Top,ui::TextVerticalAlign::Center,ui::TextVerticalAlign::Bottom
    };
    struct SpacingCase { std::optional<float> advance; float spacing; };
    const std::array<SpacingCase,4> spacing_cases{{ {std::nullopt,3},{1.0f,3},{0.0f,0},{40.0f,3} }};
    const std::array<core::Vector2,4> sizes{{ {0.25f,12},{24,80},{400,10},{400,80} }};
    for (const auto& spacing : spacing_cases)
    {
        ui::UiNumber number(core::Rect{10,20,2000,200});
        number.set_value(-888.5);
        number.set_decimal_places(1);
        number.set_suffix(ui::UiNumberSuffix::Percent);
        number.set_target_height(40);
        number.set_digit_spacing(spacing.spacing);
        if (spacing.advance)
            number.set_fixed_glyph_advance(*spacing.advance);
        number.set_opacity(128);
        const auto original = render_glyphs(number);
        require(original.size() == 7,"negative fractional percentages must retain every glyph");
        const auto original_bounds = glyph_bounds(original);
        number.set_shrink_to_fit(true);
        for (const auto size : sizes)
        {
            const int padding = size.x < 1 ? 0 : 3;
            number.set_padding(padding);
            number.set_size(size);
            const core::Rect content{10.0f + padding,20.0f + padding,
                size.x - 2.0f * padding,size.y - 2.0f * padding};
            for (const auto horizontal : horizontal_alignments)
            {
                number.set_horizontal_align(horizontal);
                for (const auto vertical : vertical_alignments)
                {
                    number.set_vertical_align(vertical);
                    const auto fitted = render_glyphs(number);
                    require(fitted.size() == original.size(),"fitting must not omit glyphs");
                    const auto bounds = glyph_bounds(fitted);
                    require(bounds.x() >= content.x() - 0.001f && bounds.y() >= content.y() - 0.001f
                        && bounds.right() <= content.right() + 0.001f
                        && bounds.bottom() <= content.bottom() + 0.001f,
                        "all fitted glyphs must remain inside the padded width and height");
                    const float scale = fitted.front().screen_rect.height() / original.front().screen_rect.height();
                    require(scale > 0 && scale <= 1.0f,"fitting must shrink uniformly without enlarging");
                    if (size.x == 400 && size.y == 80)
                        require_close(scale,1,"a run that already fits must retain its requested size");
                    for (std::size_t index = 0; index < fitted.size(); ++index)
                    {
                        require(fitted[index].texture == original[index].texture && fitted[index].alpha == 128,
                            "fitting must preserve cached textures and glyph opacity");
                        require_close(fitted[index].screen_rect.width(),original[index].screen_rect.width() * scale,
                            "fitting must preserve glyph aspect ratios");
                        require_close(fitted[index].screen_rect.height(),original[index].screen_rect.height() * scale,
                            "every glyph must share the same scale");
                        require_close(fitted[index].screen_rect.x() - fitted.front().screen_rect.x(),
                            (original[index].screen_rect.x() - original.front().screen_rect.x()) * scale,
                            "spacing and fixed advances must scale with the entire run");
                    }
                    // A larger fixed advance reserves a trailing slot beyond the last glyph.
                    if (!spacing.advance || *spacing.advance <= 1)
                    {
                        if (scale < 1.0f)
                            require(std::abs(bounds.width() - content.width()) <= 0.001f
                                || std::abs(bounds.height() - content.height()) <= 0.001f,
                                "a shrinking run must use the largest scale that fits both dimensions");
                        const float expected_x = horizontal == ui::TextHorizontalAlign::Left ? content.x()
                            : horizontal == ui::TextHorizontalAlign::Center ? content.center().x - bounds.width() * 0.5f
                            : content.right() - bounds.width();
                        require_close(bounds.x(),expected_x,"horizontal alignment must use the fitted run bounds");
                    }
                    const float expected_y = vertical == ui::TextVerticalAlign::Top ? content.y()
                        : vertical == ui::TextVerticalAlign::Center ? content.center().y - bounds.height() * 0.5f
                        : content.bottom() - bounds.height();
                    require_close(bounds.y(),expected_y,"vertical alignment must use the fitted run bounds");
                }
            }
        }

        number.set_shrink_to_fit(false);
        number.set_padding(0);
        number.set_screen_rect({10,20,6,10});
        number.set_horizontal_align(ui::TextHorizontalAlign::Left);
        number.set_vertical_align(ui::TextVerticalAlign::Top);
        const auto unfitted = render_glyphs(number);
        require_close(unfitted.front().screen_rect.height(),40,
            "disabling fitting must restore the requested height even when it overflows");
        require_close(glyph_bounds(unfitted).width(),original_bounds.width(),
            "disabling fitting must restore the original run width");
        require(number.content_extent().nearly_equals({6,10}),
            "fitting must not change the number's layout contribution");
        number.set_shrink_to_fit(true);
        number.set_size({0,10});
        require(render_glyphs(number).empty(),"zero-width widgets must not submit glyphs");
        number.set_size({10,10});
        number.set_padding(5);
        require(render_glyphs(number).empty(),"padding that consumes the interior must suppress glyphs");
    }
}

void test_new_number_states_reuse_cached_glyphs()
{
    using namespace elysia;
    ui::UiNumber number(core::Rect{0,0,200,40});
    number.set_value(101);
    const auto first = render_glyphs(number);
    require(first.size() == 3 && first[0].texture == first[2].texture,
        "repeated digits must share a cached texture");
    number.set_integer_value(111);
    const auto changed = render_glyphs(number);
    require(changed.size() == 3,"changing numeric state must retain all digits");
    for (const auto& glyph : changed)
        require(glyph.texture == first[0].texture,"new integer values must reuse existing digit textures");
    number.set_integer_value(std::numeric_limits<std::int64_t>::max());
    number.set_shrink_to_fit(true);
    number.set_size({30,20});
    const auto large_integer = render_glyphs(number);
    require(large_integer.size() == 19 && glyph_bounds(large_integer).right() <= 30.001f,
        "all exact integer digits must remain renderable within a narrow widget");
    number.set_integer_value(101);
    number.set_target_height(12);
    const auto resized = render_glyphs(number);
    for (std::size_t index = 0; index < first.size(); ++index)
        require(resized[index].texture == first[index].texture,
            "target height and fitting must not create a different texture cache entry");

    number.set_integer_value(-1);
    const auto negative = render_glyphs(number);
    number.set_suffix(ui::UiNumberSuffix::Percent);
    number.clear_value();
    const auto missing = render_glyphs(number);
    require(negative.size() == 2 && missing.size() == 2
        && missing[0].texture == negative[0].texture && missing[1].texture == missing[0].texture,
        "the missing placeholder must reuse the minus glyph twice without a percent suffix");

    number.set_suffix(ui::UiNumberSuffix::None);
    number.set_integer_value(1);
    number.set_typography_role(typography::UiTypographyRole::Caption);
    const auto caption = render_glyphs(number);
    require(caption.size() == 1 && caption[0].texture != first[0].texture,
        "changing font role must resolve the corresponding cached glyph");
    number.set_typography_role(typography::UiTypographyRole::Number);
    require(render_glyphs(number).front().texture == first[0].texture,
        "returning to the number role must reuse its cached glyph");
    number.set_font_source_override(typography::FontSource::EngineBuiltIn);
    const auto overridden = render_glyphs(number);
    require(overridden.size() == 1 && overridden[0].texture != first[0].texture,
        "font source overrides must participate in glyph lookup");
    number.clear_font_source_override();
    require(render_glyphs(number).front().texture == first[0].texture,
        "clearing the font override must restore the inherited cache entry");

    localization::LocalizationManager::instance()->clear_texture_cache();
    require(render_glyphs(number).size() == 1,
        "fitted exact integers must rebuild glyphs after shared cache clearing");
    require(ELYSIA_LOCALIZATION->set_language("en"),"fitted number tests must switch to English");
    number.clear_value();
    const auto translated_missing = render_glyphs(number);
    require(translated_missing.size() == 2 && translated_missing[0].texture == translated_missing[1].texture,
        "missing values must resolve shared glyphs again after a language change");
}

void test_ui_number_uses_shared_localized_glyphs()
{
    using namespace elysia;
    SDL_setenv_unsafe("SDL_AUDIO_DRIVER","dummy",1);
    require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO),
        "UI number tests must initialize SDL video and audio");
    require(TTF_Init(),"UI number tests must initialize SDL_ttf");
    require(elysia::tests::open_test_mixer(),
        "UI number tests must open SDL_mixer audio");
    SDL_Surface* surface = SDL_CreateSurface(256, 128, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr,"UI number tests must create a software surface");
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    require(renderer != nullptr,"UI number tests must create a software renderer");

    auto* paths = io::PathManager::instance();
    auto* resources = resources::ResourceManager::instance();
    auto* localization_manager = localization::LocalizationManager::instance();
    auto* localization = ELYSIA_LOCALIZATION;
    require(paths->initialize(),"UI number tests must initialize paths");
    resources->clear();
    const auto resolved_font_settings =
        typography::resolve_font_settings(typography::FontSettings{});
    require(resolved_font_settings.has_value(),
        "UI number default font settings must resolve");
    auto& builtin_resources = *elysia::builtin::BuiltinResources::instance();
    require(builtin_resources.initialize(
        renderer,
        builtin::BuiltinAssetCatalog(*paths),
        resolved_font_settings->engine_point_sizes()).has_value(),
        "UI number tests must initialize built-in fonts");
    typography::FontResolver font_resolver;
    localization_manager->shutdown();
    require(localization_manager->initialize(
        renderer,
        paths->configs() / "manifests" / "i18n_manifest.json",
        "en",
        &font_resolver
    ),"UI number tests must initialize localization");
    require(font_resolver.configure(
        *resolved_font_settings,
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

    test_fitting_preserves_geometry_and_alignment();
    test_new_number_states_reuse_cached_glyphs();

    localization_manager->shutdown();
    font_resolver.shutdown();
    resources->clear();
    builtin_resources.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    elysia::tests::close_test_mixer();
    TTF_Quit();

    SDL_Quit();
}
}

int main()
{
    test_ui_number_uses_shared_localized_glyphs();
    return EXIT_SUCCESS;
}
