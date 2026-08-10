#define SDL_MAIN_HANDLED

#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/widgets/ui_button.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace
{
using elysia::tests::require;

class WidthAwareDesiredElement final : public elysia::ui::UiElement
{
public:
    explicit WidthAwareDesiredElement(const elysia::core::Rect& rect)
        : UiElement(rect) {}

    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override
    {
        return { 400.0f,size().x * 0.5f };
    }
};

class AllocationSensitiveContent final : public elysia::ui::UiElement
{
public:
    explicit AllocationSensitiveContent(const elysia::core::Rect& rect)
        : UiElement(rect) {}

    [[nodiscard]] elysia::core::Vector2 content_extent() const noexcept override
    {
        return { size().x,std::max(400.0f,size().y + 36.0f) };
    }
};

void test_scroll_offset_does_not_remeasure_allocated_content_as_growth()
{
    using namespace elysia;
    ui::UiScrollContainer scroll(core::Rect{ 0,0,200,100 });
    scroll.set_scroll_axis(ui::UiScrollAxis::Vertical);
    scroll.set_content(std::make_unique<AllocationSensitiveContent>(core::Rect{ 0,0,200,0 }));

    const float initial_height = scroll.content_size().y;
    require(initial_height == 400.0f,"scroll container should measure the initial intrinsic content height");

    for (int offset = 20; offset <= 200; offset += 20)
    {
        scroll.set_scroll_offset_y(static_cast<float>(offset));
        scroll.update_layout_if_dirty();
    }

    require(scroll.content_size().y == initial_height,
        "scrolling must only reposition content, not repeatedly expand its measured height");
}


void test_list_consumes_desired_extent_and_cross_alignment()
{
    using namespace elysia;
    ui::UiListContainer list(core::Rect{ 0,0,300,400 });
    list.set_padding(ui::UiLayoutPadding{ 10,10,10,10 });

    auto centered = std::make_unique<WidthAwareDesiredElement>(core::Rect{ 0,0,1,1 });
    WidthAwareDesiredElement* centered_raw = centered.get();
    list.add_back(std::move(centered));
    list.update_layout_if_dirty();
    require(centered_raw->screen_rect().width() == 280.0f,"list should constrain desired width to its content width");
    require(centered_raw->screen_rect().height() == 140.0f,"width-constrained desired height should be remeasured");
    require(centered_raw->screen_rect().x() == 10.0f,"oversized child should fill the constrained cross axis");

    list.set_size(core::Vector2{ 200,400 });
    list.update_layout_if_dirty();
    require(centered_raw->screen_rect().width() == 180.0f,"parent width changes should relayout desired width");
    require(centered_raw->screen_rect().height() == 90.0f,"parent width changes should remeasure desired height");

    auto narrow = std::make_unique<ui::UiButton>(core::Rect{ 0,0,60,30 });
    ui::UiButton* narrow_raw = narrow.get();
    list.add_back(std::move(narrow));
    list.update_layout_if_dirty();
    require(narrow_raw->screen_rect().x() == 70.0f,"default list cross alignment should remain centered");

    list.set_cross_align(ui::UiLayoutAlign::Start);
    list.update_layout_if_dirty();
    require(narrow_raw->screen_rect().x() == 10.0f,"start cross alignment should left-align narrow children");

    ui::UiLayoutChildOptions fixed_options{};
    fixed_options._size_override = core::Vector2{ 70,25 };
    fixed_options._use_size_override = true;
    auto fixed = std::make_unique<WidthAwareDesiredElement>(core::Rect{ 0,0,1,1 });
    WidthAwareDesiredElement* fixed_raw = fixed.get();
    list.add_child(std::move(fixed),fixed_options);
    list.update_layout_if_dirty();
    require(fixed_raw->screen_rect().size().nearly_equals(core::Vector2{ 70,25 }),
        "explicit layout size override should take precedence over desired extent");
}
}

int main()
{
    test_list_consumes_desired_extent_and_cross_alignment();
    test_scroll_offset_does_not_remeasure_allocated_content_as_growth();
    std::cout << "ui layout tests passed\n";
    return EXIT_SUCCESS;
}

