#define SDL_MAIN_HANDLED

#include "engine/ui/style/ui_style_defaults.h"
#include "engine/ui/style/ui_theme_defaults.h"
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/ui/widgets/ui_button.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>

namespace
{
using elysia::ui::UiBuiltinTheme;
using elysia::ui::UiButton;
using elysia::ui::UiButtonVisualRole;
using elysia::ui::UiStyleDefaults;
using elysia::ui::UiThemeDefaults;
using elysia::ui::UiThemeManager;
using elysia::tests::require;

class DefaultThemeRestore final
{
public:
    DefaultThemeRestore() noexcept
        : _theme(UiThemeDefaults::current_builtin_theme())
    {
    }

    ~DefaultThemeRestore()
    {
        (void)UiThemeDefaults::set_builtin_theme(_theme);
    }

private:
    UiBuiltinTheme _theme;
};

void test_builtin_theme_selection_and_validation()
{
    const DefaultThemeRestore restore;
    require(
        UiThemeDefaults::current_builtin_theme()
            == UiBuiltinTheme::BlueGlassMoon,
        "the process construction default must start as BlueGlassMoon");

    constexpr std::array themes{
        UiBuiltinTheme::BlueGlassMoon,
        UiBuiltinTheme::ElysiaLight,
        UiBuiltinTheme::ElysiaDark,
        UiBuiltinTheme::EvangelionUnit00,
        UiBuiltinTheme::EvangelionUnit01,
        UiBuiltinTheme::EvangelionUnit02,
        UiBuiltinTheme::QuietSlate
    };

    for (const UiBuiltinTheme theme : themes)
    {
        require(
            UiThemeDefaults::set_builtin_theme(theme),
            "every built-in theme must be accepted as a construction default");
        require(
            UiThemeDefaults::current_builtin_theme() == theme,
            "the selected construction default must be observable");
        require(
            UiThemeDefaults::theme().button().chrome.background.idle
                == elysia::ui::builtin_theme(theme)
                    .button().chrome.background.idle,
            "the default theme payload must match the selected built-in theme");
    }

    const UiBuiltinTheme before_invalid =
        UiThemeDefaults::current_builtin_theme();
    require(
        !UiThemeDefaults::set_builtin_theme(
            static_cast<UiBuiltinTheme>(255)),
        "an invalid built-in theme must be rejected");
    require(
        UiThemeDefaults::current_builtin_theme() == before_invalid,
        "an invalid theme must not mutate the construction default");
}

void test_control_defaults_are_construction_snapshots()
{
    const DefaultThemeRestore restore;
    require(
        UiThemeDefaults::set_builtin_theme(UiBuiltinTheme::BlueGlassMoon),
        "test setup must select the original default theme");

    UiButton before(elysia::core::Rect{0,0,120,40});
    const auto original_color = before.style().chrome.background.idle;

    require(
        UiThemeDefaults::set_builtin_theme(UiBuiltinTheme::ElysiaDark),
        "test setup must select the replacement default theme");
    UiButton after(elysia::core::Rect{0,0,120,40});
    const auto dark_color = elysia::ui::builtin_theme(
        UiBuiltinTheme::ElysiaDark)
        .button(UiButtonVisualRole::Default)
        .chrome.background.idle;

    require(
        before.style().chrome.background.idle == original_color,
        "changing the default theme must not restyle existing controls");
    require(
        after.style().chrome.background.idle == dark_color,
        "controls created later must use the current default theme");
    require(
        UiStyleDefaults::button().chrome.background.idle == dark_color,
        "UiStyleDefaults must resolve through the current default theme");

    before.reset();
    require(
        before.style().chrome.background.idle == dark_color,
        "an explicit reset must adopt the current construction default");
}

void test_theme_manager_snapshots_the_construction_default()
{
    const DefaultThemeRestore restore;
    require(
        UiThemeDefaults::set_builtin_theme(UiBuiltinTheme::ElysiaLight),
        "test setup must select a manager construction default");
    UiThemeManager manager;

    require(
        UiThemeDefaults::set_builtin_theme(UiBuiltinTheme::ElysiaDark),
        "test setup must change the later construction default");
    require(
        manager.current_builtin_theme() == UiBuiltinTheme::ElysiaLight,
        "an existing manager must not follow construction-default changes");

    manager.set_theme(UiBuiltinTheme::QuietSlate);
    require(
        manager.current_builtin_theme() == UiBuiltinTheme::QuietSlate,
        "a manager must retain independent runtime theme control");
}
}

int main()
{
    test_builtin_theme_selection_and_validation();
    test_control_defaults_are_construction_snapshots();
    test_theme_manager_snapshots_the_construction_default();
    return EXIT_SUCCESS;
}
