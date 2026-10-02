#include "game/showcase/ui/ui_gallery_pages.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/containers/ui_chrome_container.h"
namespace example::showcase::ui {
std::unique_ptr<elysia::ui::UiButton> UiGalleryPages::make_button(
    const char* text_key) const
{
    // UiButton owns the copied text descriptor and invokes its callback only after
    // a complete confirm or primary-pointer press/release sequence.
    return std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0,0,240,40 },
        elysia::ui::UiButtonConfig{
            .content = elysia::ui::ui_text_key(text_key)
        },
        0);
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiGalleryPages::make_page_scroll(
    elysia::ui::UiListContainer*& content) const
{
    // UiScrollContainer owns one content subtree, clips it to the viewport, and
    // delegates focus navigation into the active child scope while scrolling.
    auto page = std::make_unique<elysia::ui::UiScrollContainer>(
        elysia::core::Rect{ 0,0,900,390 });
    page->set_scroll_axis(elysia::ui::UiScrollAxis::Vertical);
    page->set_scrollbar_visibility(elysia::ui::UiScrollBarVisibility::Auto);
    page->set_scroll_step(elysia::core::Vector2(0.0f,36.0f));

    // UiListContainer adopts every row and derives both layout order and focus
    // neighbors from the same vertical child sequence.
    auto list = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0,0,870,0 });
    list->set_padding(elysia::ui::UiLayoutPadding{ 12,12,12,12 });
    list->set_item_spacing(12.0f);
    list->set_cross_align(elysia::ui::UiLayoutAlign::Start);
    content = list.get();
    page->set_content(std::move(list));
    return page;
}

elysia::ui::UiListContainer* UiGalleryPages::add_section(
    elysia::ui::UiListContainer& page,
    const char* title_key,
    const char* description_key) const
{
    // UiChromeContainer separates header slots from its body while retaining a
    // single delegated focus region for all controls placed inside those slots.
    auto chrome = std::make_unique<elysia::ui::UiChromeContainer>(
        elysia::core::Rect{ 0,0,840,0 });
    chrome->set_header_height(42.0f);

    // UiLabel resolves localized single-line content through its typography and
    // semantic visual roles without accepting focus or input.
    auto heading = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,420,32 },0,
        elysia::ui::ui_text_key(title_key));
    heading->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    chrome->add_title_child(std::move(heading));

    auto body = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0,0,820,0 });
    body->set_padding(elysia::ui::UiLayoutPadding{ 12,8,12,8 });
    body->set_item_spacing(8.0f);
    body->set_cross_align(elysia::ui::UiLayoutAlign::Start);

    auto note = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0,0,760,28 },0,
        elysia::ui::ui_text_key(description_key));
    note->set_visual_role(elysia::ui::UiLabelVisualRole::Muted);
    body->add_back(std::move(note));

    elysia::ui::UiListContainer* body_ptr = body.get();
    chrome->set_body(std::move(body));
    page.add_back(std::move(chrome));
    return body_ptr;
}

}
