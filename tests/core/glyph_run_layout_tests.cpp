#include "engine/core/render/glyph_run_layout.h"
#include "tests/support/test_assertions.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
using elysia::tests::require;

void test_proportional_layout_and_spacing()
{
    const std::vector<elysia::core::Vector2> sizes = { { 5,10 },{ 10,20 },{ 4,10 } };
    const elysia::core::GlyphRunLayout layout = elysia::core::layout_glyph_run(
        sizes,
        { .target_height = 20.0f,.spacing = 2.0f }
    );

    require(layout.glyphs.size() == 3,"all valid glyphs must be laid out");
    require(layout.glyphs[0].local_rect.nearly_equals({ 0,0,10,20 }),"first glyph must start at the origin");
    require(layout.glyphs[1].local_rect.nearly_equals({ 12,0,10,20 }),"spacing must separate proportional glyphs");
    require(layout.glyphs[2].local_rect.nearly_equals({ 24,0,8,20 }),"source aspect ratio must be preserved");
    require(std::fabs(layout.width - 32.0f) < 0.001f && layout.height == 20.0f,
        "run extent must use accumulated advances and spacing");
}

void test_fixed_advance_overlap_and_invalid_input()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::vector<elysia::core::Vector2> sizes = { { 10,10 },{ 0,10 },{ nan,10 },{ 4,10 } };
    const elysia::core::GlyphRunLayout layout = elysia::core::layout_glyph_run(
        sizes,
        { .target_height = 20.0f,.spacing = 3.0f,.fixed_advance = 5.0f }
    );

    require(layout.glyphs.size() == 2,"invalid source sizes must be skipped");
    require(layout.glyphs[0].glyph_index == 0 && layout.glyphs[1].glyph_index == 3,
        "placements must retain their original source indices");
    require(layout.glyphs[0].local_rect.width() == 20.0f && layout.glyphs[1].local_rect.x() == 8.0f,
        "fixed advance smaller than render width must preserve overlap semantics");
    require(layout.width == 13.0f,"run width must be based on fixed advances");

    require(elysia::core::layout_glyph_run(sizes,{ .target_height = 0.0f }).glyphs.empty(),
        "non-positive target height must produce an empty layout");
    require(elysia::core::layout_glyph_run({}, { .target_height = 20.0f }).width == 0.0f,
        "empty input must produce an empty extent");
}
}

int main()
{
    test_proportional_layout_and_spacing();
    test_fixed_advance_overlap_and_invalid_input();
    return EXIT_SUCCESS;
}
