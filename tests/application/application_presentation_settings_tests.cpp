#include "engine/application/game_module.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>

namespace
{
using elysia::application::ApplicationDescriptor;
using elysia::application::ApplicationEngineLogoVariant;
using elysia::application::ApplicationTextureFilter;
using elysia::typography::FontSource;
using elysia::ui::UiBuiltinTheme;
using elysia::tests::require;

void require_default_presentation_settings()
{
    const ApplicationDescriptor descriptor;

    require(
        descriptor.presentation.render.texture_filter
            == ApplicationTextureFilter::Nearest,
        "application presentation must default to nearest filtering");
    require(
        descriptor.presentation.fonts.ui.source
                == FontSource::EngineBuiltIn
            && descriptor.presentation.fonts.floating_number.source
                == FontSource::EngineBuiltIn,
        "application presentation must compose default typography settings");
    require(
        descriptor.presentation.ui.default_theme
            == UiBuiltinTheme::BlueGlassMoon,
        "application presentation must default to BlueGlassMoon");
    require(
        descriptor.presentation.startup.engine_logo
            == ApplicationEngineLogoVariant::White,
        "application startup presentation must default to the white engine logo");
}

void require_ui_settings_are_data_only()
{
    ApplicationDescriptor descriptor;
    descriptor.presentation.ui.default_theme = UiBuiltinTheme::ElysiaDark;

    require(
        descriptor.presentation.ui.default_theme
            == UiBuiltinTheme::ElysiaDark,
        "application presentation must retain the selected default UI theme");
}

void require_render_settings_are_data_only()
{
    ApplicationDescriptor descriptor;
    descriptor.presentation.render.texture_filter =
        ApplicationTextureFilter::Linear;

    require(
        descriptor.presentation.render.texture_filter
            == ApplicationTextureFilter::Linear,
        "application presentation must retain the selected texture filter");
}

void require_engine_logo_variants_are_data_only()
{
    constexpr std::array variants{
        ApplicationEngineLogoVariant::Default,
        ApplicationEngineLogoVariant::Black,
        ApplicationEngineLogoVariant::BlackAlphaInverse,
        ApplicationEngineLogoVariant::LightEdge,
        ApplicationEngineLogoVariant::White
    };

    ApplicationDescriptor descriptor;
    for (const ApplicationEngineLogoVariant variant : variants)
    {
        descriptor.presentation.startup.engine_logo = variant;
        require(
            descriptor.presentation.startup.engine_logo == variant,
            "application presentation must retain the selected engine logo variant");
    }
}
}

int main()
{
    require_default_presentation_settings();
    require_render_settings_are_data_only();
    require_ui_settings_are_data_only();
    require_engine_logo_variants_are_data_only();
    return EXIT_SUCCESS;
}
