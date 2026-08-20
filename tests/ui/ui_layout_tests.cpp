#define SDL_MAIN_HANDLED

#include "engine/ui/containers/ui_chrome_container.h"
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

elysia::ui::UiInputEvent mouse_wheel_event(int wheel_y)
{
    return elysia::ui::UiInputEvent{
        .type = elysia::ui::UiInputEventType::MouseWheel,
        .device = elysia::input::InputDevice::Mouse,
        .mouse_x = 100,
        .mouse_y = 100,
        .wheel_y = wheel_y
    };
}

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

void test_chrome_header_measurement_stays_stable_while_scrolling()
{
    using namespace elysia;
    ui::UiScrollContainer scroll(core::Rect{ 0,0,900,390 });
    scroll.set_scroll_axis(ui::UiScrollAxis::Vertical);
    scroll.set_scrollbar_visibility(ui::UiScrollBarVisibility::Auto);
    scroll.set_scroll_step(core::Vector2{ 0,36 });

    auto page = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,870,0 });
    ui::UiListContainer* page_raw = page.get();
    page->set_padding(ui::UiLayoutPadding{ 12,12,12,12 });
    page->set_item_spacing(12.0f);
    page->set_cross_align(ui::UiLayoutAlign::Start);

    auto chrome = std::make_unique<ui::UiChromeContainer>(core::Rect{ 0,0,840,0 });
    ui::UiChromeContainer* chrome_raw = chrome.get();
    chrome->set_header_height(42.0f);
    chrome->add_title_child(std::make_unique<ui::UiElement>(core::Rect{ 0,0,420,32 }));

    auto body = std::make_unique<ui::UiListContainer>(core::Rect{ 0,0,820,0 });
    ui::UiListContainer* body_raw = body.get();
    body->set_padding(ui::UiLayoutPadding{ 12,8,12,8 });
    body->set_item_spacing(8.0f);
    body->set_cross_align(ui::UiLayoutAlign::Start);
    ui::UiButton* last_button = nullptr;
    for (int index = 0; index < 12; ++index)
    {
        auto button = std::make_unique<ui::UiButton>(core::Rect{ 0,0,240,40 });
        last_button = button.get();
        body->add_back(std::move(button));
    }
    chrome->set_body(std::move(body));
    page->add_back(std::move(chrome));
    scroll.set_content(std::move(page));

    scroll.update(0.0);
    scroll.update(0.0);
    require(body_raw->screen_rect().y() - chrome_raw->screen_rect().y() == 44.0f,
        "chrome layout must expand a 42px minimum header to fit a 32px title and 12px padding");

    const core::Vector2 initial_scroll_size = scroll.size();
    const core::Vector2 initial_content_size = scroll.content_size();
    const core::Vector2 initial_allocated_size = page_raw->size();
    const core::Vector2 initial_content_extent = page_raw->content_extent();
    const core::Vector2 initial_max_offset = scroll.max_scroll_offset();
    require(initial_content_size.nearly_equals(initial_content_extent),
        "scroll measurement and chrome-backed content extent must settle to the same size");
    require(initial_max_offset.y > 0.0f,
        "chrome regression content must overflow the viewport vertically");

    for (int cycle = 0; cycle < 50; ++cycle)
    {
        for (int step = 0; step < 20; ++step)
            (void)scroll.on_ui_input_event(mouse_wheel_event(-1));
        scroll.update(0.0);
        require(scroll.scroll_offset_y() == initial_max_offset.y,
            "repeated wheel input must still reach the bottom of chrome-backed content");
        require(last_button && last_button->screen_rect().bottom() <= scroll.screen_rect().bottom()
                && last_button->screen_rect().bottom() > scroll.screen_rect().top(),
            "the final chrome body control must be visible at the bottom scroll offset");

        for (int step = 0; step < 20; ++step)
            (void)scroll.on_ui_input_event(mouse_wheel_event(1));
        scroll.update(0.0);
        require(scroll.scroll_offset_y() == 0.0f,
            "repeated reverse wheel input must return chrome-backed content to the top");
        require(scroll.size().nearly_equals(initial_scroll_size)
                && scroll.content_size().nearly_equals(initial_content_size)
                && page_raw->size().nearly_equals(initial_allocated_size)
                && page_raw->content_extent().nearly_equals(initial_content_extent)
                && scroll.max_scroll_offset().nearly_equals(initial_max_offset),
            "chrome measurement and scroll range must remain idempotent across wheel cycles");
    }
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
    test_chrome_header_measurement_stays_stable_while_scrolling();
    std::cout << "ui layout tests passed\n";
    return EXIT_SUCCESS;
}
