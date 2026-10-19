#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/widgets/number/ui_number.h"
#include "tests/support/test_assertions.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <locale>
#include <string>

namespace
{
using elysia::tests::require;
using elysia::ui::UiNumber;
using elysia::ui::UiNumberSuffix;

void test_exact_integer_formatting()
{
    UiNumber number;
    require(number.has_value() && !number.integer_value() && number.value() == 0.0
        && number.formatted_text() == "0" && !number.shrinks_to_fit(),
        "default numbers must retain floating zero and disabled fitting");

    const std::array<std::int64_t,4> integers{
        9007199254740993LL,std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),0
    };
    for (const auto integer : integers)
    {
        number.set_integer_value(integer);
        require(number.has_value() && number.integer_value() == integer
            && number.formatted_text() == std::to_string(integer),
            "integer assignments must preserve every signed 64-bit digit");
        require(number.value() == static_cast<double>(integer),
            "the floating view must convert the stored integer");
    }

    number.set_integer_value(std::numeric_limits<std::int64_t>::min());
    number.set_decimal_places(2);
    number.set_trim_trailing_zeros(false);
    number.set_suffix(UiNumberSuffix::Percent);
    require(number.formatted_text() == "-9223372036854775808.00%",
        "exact integers must support decimal places and suffixes");
    number.set_trim_trailing_zeros(true);
    require(number.formatted_text() == "-9223372036854775808%",
        "integer fractional zeros must be removable");
    number.set_keep_decimal_point(true);
    require(number.formatted_text() == "-9223372036854775808.%",
        "integer trimming must preserve an explicitly requested decimal point");
    number.set_decimal_places(0);
    require(number.formatted_text() == "-9223372036854775808%",
        "keeping the decimal point must not invent a fractional part");
}

void test_missing_values_and_type_transitions()
{
    UiNumber number;
    number.set_decimal_places(2);
    number.set_trim_trailing_zeros(false);
    number.set_suffix(UiNumberSuffix::Percent);
    number.set_integer_value(42);
    number.clear_value();
    number.clear_value();
    require(!number.has_value() && !number.integer_value() && number.value() == 0.0
        && number.formatted_text() == "--",
        "missing numbers must omit suffixes while retaining a zero floating view");

    number.set_integer_value(0);
    require(number.has_value() && number.integer_value() == 0 && number.formatted_text() == "0.00%",
        "an exact zero assignment must leave the missing state");
    number.set_value(0.0);
    require(number.has_value() && !number.integer_value() && number.formatted_text() == "0.00%",
        "equal floating assignments must still leave integer mode");
    number.set_value(12.5);
    require(number.formatted_text() == "12.50%", "floating values must retain decimal formatting");
    number.set_integer_value(12);
    require(number.integer_value() == 12 && number.formatted_text() == "12.00%",
        "floating assignments must transition back to exact integers");

    const std::array invalid_values{
        std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (const double invalid : invalid_values)
    {
        number.set_integer_value(12);
        number.set_value(invalid);
        require(!number.has_value() && !number.integer_value() && number.value() == 0.0
            && number.formatted_text() == "--", "non-finite values must enter the missing state");
        number.set_value(0.0);
        require(number.has_value() && !number.integer_value() && number.formatted_text() == "0.00%",
            "finite floating zero must recover from a missing assignment");
    }

    number.set_suffix(UiNumberSuffix::None);
    number.set_decimal_places(2);
    number.set_trim_trailing_zeros(true);
    number.set_value(12.125);
    require(number.formatted_text() == "12.12", "floating formatting must retain existing rounding");
    number.set_value(-0.0);
    require(number.formatted_text() == "0", "negative zero must retain existing normalization");
    number.set_value(-1e-10);
    require(number.formatted_text() == "0", "near-zero values must retain existing normalization");
}

class CommaPunctuation final : public std::numpunct<char>
{
protected:
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '_'; }
    std::string do_grouping() const override { return "\3"; }
};

class ScopedGlobalLocale
{
public:
    ScopedGlobalLocale() : _previous(std::locale())
    {
        std::locale::global(std::locale(_previous,new CommaPunctuation));
    }
    ~ScopedGlobalLocale() { std::locale::global(_previous); }
private:
    std::locale _previous;
};

void test_formatting_is_independent_of_global_locale()
{
    ScopedGlobalLocale locale;
    UiNumber number;
    number.set_decimal_places(2);
    number.set_trim_trailing_zeros(false);
    number.set_value(1234.5);
    require(number.formatted_text() == "1234.50",
        "floating formatting must use ASCII punctuation without locale grouping");
    number.set_integer_value(1234);
    require(number.formatted_text() == "1234.00", "integers must use the same punctuation as floats");
    number.set_value(1234.5);
    number.set_trim_trailing_zeros(true);
    require(number.formatted_text() == "1234.5", "trimming must remain valid under a custom locale");
}

class LayoutNotificationHost final : public elysia::ui::UiPanel
{
public:
    int invalidations = 0;
protected:
    void on_child_intrinsic_layout_invalidated(elysia::ui::UiElement& child) noexcept override
    {
        ++invalidations;
        UiPanel::on_child_intrinsic_layout_invalidated(child);
    }
};

void test_repeated_assignments_do_not_invalidate_parent_layout()
{
    LayoutNotificationHost host;
    auto* number = host.create_child<UiNumber>(elysia::core::Rect::zero());
    require(number != nullptr, "the notification test must attach a number to a parent");
    host.invalidations = 0;
    number->set_value(0.0);
    require(host.invalidations == 0, "assigning the existing floating value must be a no-op");
    number->set_value(12.0);
    number->set_value(12.0);
    require(host.invalidations == 1, "repeated floating values must notify only once");
    number->set_integer_value(12);
    number->set_integer_value(12);
    require(host.invalidations == 2, "integer mode changes must notify even for equal numeric values");
    number->clear_value();
    number->clear_value();
    number->set_value(std::numeric_limits<double>::infinity());
    require(host.invalidations == 3, "repeated missing values must notify only once");
    number->set_value(12.0);
    require(host.invalidations == 4, "restoring a finite value must notify the parent");
    number->set_shrink_to_fit(true);
    number->set_shrink_to_fit(true);
    require(host.invalidations == 5, "repeated fitting assignments must be a no-op");
}

void test_reset_restores_number_defaults()
{
    UiNumber number({10,20,100,40});
    number.set_integer_value(17);
    number.set_shrink_to_fit(true);
    number.set_decimal_places(3);
    number.set_trim_trailing_zeros(false);
    number.set_keep_decimal_point(true);
    number.set_suffix(UiNumberSuffix::Percent);
    number.set_fixed_glyph_advance(12);
    number.set_target_height(24);
    number.set_padding(4);
    number.set_digit_spacing(3);
    number.set_font_source_override(elysia::typography::FontSource::Project);
    number.set_typography_role(elysia::typography::UiTypographyRole::SliderValue);
    number.set_horizontal_align(elysia::ui::TextHorizontalAlign::Right);
    number.set_vertical_align(elysia::ui::TextVerticalAlign::Bottom);
    number.set_opacity(128);
    number.set_style_overrides({.draw_background = true});
    number.clear_value();
    number.reset();
    require(number.has_value() && !number.integer_value() && number.value() == 0.0
        && number.formatted_text() == "0" && !number.shrinks_to_fit(),
        "reset must restore floating zero and disabled fitting");
    require(number.decimal_places() == 0 && number.trims_trailing_zeros()
        && !number.keeps_decimal_point() && number.suffix() == UiNumberSuffix::None,
        "reset must restore all numeric formatting options");
    require(!number.fixed_glyph_advance() && !number.target_height() && number.padding() == 0
        && number.digit_spacing() == 0 && !number.font_source_override()
        && number.typography_role() == elysia::typography::UiTypographyRole::Number
        && number.horizontal_align() == elysia::ui::TextHorizontalAlign::Left
        && number.vertical_align() == elysia::ui::TextVerticalAlign::Top
        && number.opacity() == 255 && !number.has_style_overrides(),
        "reset must restore layout, font and visual options");
}
}

int main()
{
    test_exact_integer_formatting();
    test_missing_values_and_type_transitions();
    test_formatting_is_independent_of_global_locale();
    test_repeated_assignments_do_not_invalidate_parent_layout();
    test_reset_restores_number_defaults();
    return EXIT_SUCCESS;
}
