#include "engine/typography/font_settings.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>
#include <span>
#include <type_traits>
#include <vector>

namespace
{
using elysia::typography::FontSettings;
using elysia::typography::FontSource;
using elysia::typography::UiTypographyProfile;
using elysia::typography::resolve_font_settings;
using elysia::tests::require;

static_assert(
    !std::is_default_constructible_v<UiTypographyProfile>,
    "typography profiles must provide a complete role table");

void require_sizes(
    std::span<const int> actual,
    std::initializer_list<int> expected,
    const char* message)
{
    require(
        std::vector<int>(actual.begin(),actual.end())
            == std::vector<int>(expected),
        message);
}

void require_default_font_settings_resolve()
{
    const FontSettings settings;
    require(
        settings.ui.source == FontSource::EngineBuiltIn
            && settings.floating_number.source == FontSource::EngineBuiltIn,
        "font settings must use Engine fonts by default");
    require(
        !settings.ui.typography_override
            && !settings.floating_number.point_size_override,
        "font settings must leave size overrides empty by default");

    const auto resolved = resolve_font_settings(settings);
    require(resolved.has_value(),"default font settings must resolve");
    require(
        resolved->ui_typography().point_sizes()
            == UiTypographyProfile::PointSizes{
                30,30,70,50,30,20,30,20,30,60,30,20,30,30,30,10,40
            },
        "unconfigured UI typography must resolve to Engine defaults");
    require(
        resolved->floating_number_point_size() == 20,
        "unconfigured floating numbers must resolve to 20pt");
    require_sizes(
        resolved->engine_point_sizes(),
        {10,20,30,40,50,60,70},
        "default Engine point sizes must be sorted and deduplicated");
    require(
        resolved->project_point_sizes().empty(),
        "Engine-only defaults must not request project font sizes");
}

void require_custom_font_settings_resolve()
{
    UiTypographyProfile::PointSizes point_sizes{};
    point_sizes.fill(24);

    FontSettings settings;
    settings.ui.source = FontSource::Project;
    settings.ui.typography_override = UiTypographyProfile(point_sizes);
    settings.floating_number.source = FontSource::Project;
    settings.floating_number.point_size_override = 18;

    const auto resolved = resolve_font_settings(settings);
    require(resolved.has_value(),"custom font settings must resolve");
    require(
        resolved->ui_source() == FontSource::Project
            && resolved->floating_number_source() == FontSource::Project,
        "resolved settings must preserve configured font sources");
    require(
        resolved->ui_typography().point_sizes() == point_sizes
            && resolved->floating_number_point_size() == 18,
        "resolved settings must preserve explicit size overrides");
    require_sizes(
        resolved->engine_point_sizes(),
        {18,24},
        "Engine fallback sizes must include every resolved font size");
    require_sizes(
        resolved->project_point_sizes(),
        {18,24},
        "project sizes must include only requested project font sizes");

    settings.ui.source = FontSource::EngineBuiltIn;
    const auto mixed = resolve_font_settings(settings);
    require(mixed.has_value(),"mixed font settings must resolve");
    require_sizes(
        mixed->engine_point_sizes(),
        {18,24},
        "Engine sizes must remain independent of configured sources");
    require_sizes(
        mixed->project_point_sizes(),
        {18},
        "project sizes must omit Engine-only UI sizes");
}

void require_invalid_font_settings_fail()
{
    FontSettings settings;
    UiTypographyProfile::PointSizes point_sizes{};
    point_sizes.fill(20);
    point_sizes.front() = 0;
    settings.ui.typography_override = UiTypographyProfile(point_sizes);
    require(
        !resolve_font_settings(settings),
        "non-positive UI typography must fail resolution");

    settings.ui.typography_override.reset();
    settings.floating_number.point_size_override = 0;
    require(
        !resolve_font_settings(settings),
        "non-positive floating-number typography must fail resolution");

    settings.floating_number.point_size_override.reset();
    settings.ui.source = static_cast<FontSource>(255);
    require(
        !resolve_font_settings(settings),
        "unknown font sources must fail resolution");
}
}

int main()
{
    require_default_font_settings_resolve();
    require_custom_font_settings_resolve();
    require_invalid_font_settings_fail();
    return EXIT_SUCCESS;
}
