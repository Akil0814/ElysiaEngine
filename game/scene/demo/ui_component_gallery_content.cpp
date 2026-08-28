#include "ui_component_gallery_scene.h"

#include "../../../engine/builtin/resources/builtin_resources.h"
#include "../../../engine/scene/runtime/scene_runtime_context.h"
#include "../../../engine/ui/composites/ui_tab_container.h"
#include "../../../engine/ui/containers/ui_button_group.h"
#include "../../../engine/ui/containers/ui_chrome_container.h"
#include "../../../engine/ui/containers/ui_grid_container.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_panel.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/image/ui_animation.h"
#include "../../../engine/ui/widgets/image/ui_blink_image.h"
#include "../../../engine/ui/widgets/image/ui_fade_image.h"
#include "../../../engine/ui/widgets/image/ui_image.h"
#include "../../../engine/ui/widgets/image/ui_pulse_image.h"
#include "../../../engine/ui/widgets/label/ui_blink_label.h"
#include "../../../engine/ui/widgets/label/ui_fade_label.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/label/ui_pulse_label.h"
#include "../../../engine/ui/widgets/number/ui_number.h"
#include "../../../engine/ui/widgets/text/ui_text_block.h"
#include "../../../engine/ui/widgets/ui_bar.h"
#include "../../../engine/ui/widgets/ui_button.h"

#include <array>
#include <memory>
#include <stdexcept>

namespace example::scene
{
namespace
{
using namespace elysia::ui;
using elysia::typography::UiTypographyRole;

std::unique_ptr<UiListContainer> horizontal_content_row(
    float width,
    float height,
    float spacing = 10.0f)
{
    auto row = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,width,height });
    row->set_direction(UiListDirection::Horizontal);
    row->set_cross_align(UiLayoutAlign::Center);
    row->set_item_spacing(spacing);
    return row;
}

std::unique_ptr<UiButton> compact_content_button(const char* key)
{
    return std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,180,40 },
        UiButtonConfig{ .content = ui_text_key(key) },0);
}
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_media_page(SDL_Texture* image_texture)
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);

    UiListContainer* text_section = add_section(
        *page_content,
        "ui_component_gallery.media.text_title",
        "ui_component_gallery.media.text_description");

    // UiTextBlock resolves TextKey at render time, wraps within its padded content
    // width, and reports the measured wrapped height to list and scroll layouts.
    auto localized_text = std::make_unique<UiTextBlock>(
        elysia::core::Rect{ 0,0,740,84 });
    localized_text->set_text_content(ui_text_key(
        "ui_component_gallery.media.long_text"));
    localized_text->set_padding(6);
    text_section->add_back(std::move(localized_text));

    auto raw_text = std::make_unique<UiTextBlock>(
        elysia::core::Rect{ 0,0,740,64 });
    raw_text->set_text_content(ui_raw_text(
        "RawText bypasses localization and is appropriate for stable identifiers."));
    raw_text->set_visual_role(UiTextBlockVisualRole::Muted);
    raw_text->set_padding(6);
    text_section->add_back(std::move(raw_text));

    UiListContainer* number_section = add_section(
        *page_content,
        "ui_component_gallery.media.number_title",
        "ui_component_gallery.media.number_description");

    // UiNumber formats values into individually positioned glyphs. Fixed advance
    // prevents columns from jittering when digits change between frames.
    auto numbers = horizontal_content_row(790.0f,58.0f);
    const std::array<double,4> values{ 0.0,-42.5,73.25,12345.678 };
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        auto number = std::make_unique<UiNumber>(
            elysia::core::Rect{ 0,0,180,48 });
        number->set_value(values[index]);
        number->set_decimal_places(index == 0 ? 0 : 2);
        number->set_horizontal_align(TextHorizontalAlign::Center);
        if (index == 2)
            number->set_suffix(UiNumberSuffix::Percent);
        if (index == 3)
        {
            number->set_trim_trailing_zeros(false);
            number->set_fixed_glyph_advance(18.0f);
        }
        numbers->add_back(std::move(number));
    }
    number_section->add_back(std::move(numbers));

    // UiBar accepts either a ranged value or normalized ratio and computes fill
    // geometry from the selected direction without owning interactive state.
    auto bars = horizontal_content_row(790.0f,132.0f);
    for (const BarFillDirection direction : {
            BarFillDirection::LeftToRight,
            BarFillDirection::RightToLeft,
            BarFillDirection::TopToBottom,
            BarFillDirection::BottomToTop})
    {
        const bool vertical = direction == BarFillDirection::TopToBottom
            || direction == BarFillDirection::BottomToTop;
        auto bar = std::make_unique<UiBar>(vertical
            ? elysia::core::Rect{ 0,0,32,120 }
            : elysia::core::Rect{ 0,0,240,24 });
        bar->set_ratio(0.58f);
        bar->set_fill_direction(direction);
        bars->add_back(std::move(bar));
    }
    number_section->add_back(std::move(bars));

    UiListContainer* effects_section = add_section(
        *page_content,
        "ui_component_gallery.media.effects_title",
        "ui_component_gallery.media.effects_description");

    // Opacity label variants retain normal UiLabel rendering and multiply its
    // command alpha from an Updatable playback core while their page is active.
    auto fade_in_label = std::make_unique<UiFadeLabel>(
        elysia::core::Rect{ 0,0,240,34 },0,
        ui_text_key("ui_component_gallery.media.fade_in"));
    UiFadeLabel* fade_in_label_ptr = fade_in_label.get();
    fade_in_label->configure_playback(
        effects::UiOpacityFadeMode::FadeIn,0.15,0.45,0.0);
    fade_in_label->play();
    effects_section->add_back(std::move(fade_in_label));

    auto fade_in_out_label = std::make_unique<UiFadeLabel>(
        elysia::core::Rect{ 0,0,240,34 },0,
        ui_text_key("ui_component_gallery.media.fade_in_out"));
    UiFadeLabel* fade_in_out_label_ptr = fade_in_out_label.get();
    fade_in_out_label->configure_playback(
        effects::UiOpacityFadeMode::FadeInOut,0.15,0.35,0.35);
    fade_in_out_label->play();
    effects_section->add_back(std::move(fade_in_out_label));

    auto blink_label = std::make_unique<UiBlinkLabel>(
        elysia::core::Rect{ 0,0,240,34 },0,
        ui_text_key("ui_component_gallery.media.blink_visible"));
    UiBlinkLabel* blink_label_ptr = blink_label.get();
    blink_label->configure_playback(
        effects::UiOpacityBlinkMode::VisibleFirst,0.0,0.25,0.25,3);
    blink_label->play();
    effects_section->add_back(std::move(blink_label));

    auto pulse_label = std::make_unique<UiPulseLabel>(
        elysia::core::Rect{ 0,0,240,34 },0,
        ui_text_key("ui_component_gallery.media.pulse_min"));
    UiPulseLabel* pulse_label_ptr = pulse_label.get();
    pulse_label->configure_playback(
        effects::UiOpacityPulseMode::MinToMax,0.0,0.35,0.35,3);
    pulse_label->play();
    effects_section->add_back(std::move(pulse_label));

    // UiImage borrows its texture from BuiltinResources; a source rectangle
    // selects atlas pixels without changing the widget's destination geometry.
    auto images = horizontal_content_row(790.0f,96.0f);
    images->add_back(std::make_unique<UiImage>(
        image_texture,elysia::core::Rect{ 0,0,110,80 }));
    auto cropped_image = std::make_unique<UiImage>(
        image_texture,elysia::core::Rect{ 0,0,110,80 });
    cropped_image->set_source_rect(elysia::core::Rect{ 0,0,32,32 });
    images->add_back(std::move(cropped_image));

    // Image opacity variants apply the same playback state machines to texture
    // commands; these samples cover the complementary enum modes used by labels.
    auto fade_out_image = std::make_unique<UiFadeImage>(
        image_texture,elysia::core::Rect{ 0,0,110,80 });
    UiFadeImage* fade_out_image_ptr = fade_out_image.get();
    fade_out_image->configure_playback(
        effects::UiOpacityFadeMode::FadeOut,0.15,0.0,0.45);
    fade_out_image->play();
    images->add_back(std::move(fade_out_image));

    auto blink_image = std::make_unique<UiBlinkImage>(
        image_texture,elysia::core::Rect{ 0,0,110,80 });
    UiBlinkImage* blink_image_ptr = blink_image.get();
    blink_image->configure_playback(
        effects::UiOpacityBlinkMode::HiddenFirst,0.0,0.25,0.25,3);
    blink_image->play();
    images->add_back(std::move(blink_image));

    auto pulse_image = std::make_unique<UiPulseImage>(
        image_texture,elysia::core::Rect{ 0,0,110,80 });
    UiPulseImage* pulse_image_ptr = pulse_image.get();
    pulse_image->configure_playback(
        effects::UiOpacityPulseMode::MaxToMin,0.0,0.35,0.35,3);
    pulse_image->play();
    images->add_back(std::move(pulse_image));
    effects_section->add_back(std::move(images));

    // The replay button borrows sibling pointers. All siblings and the callback
    // button share the page's lifetime, so no callback can outlive its targets.
    auto replay_effects = make_button("ui_component_gallery.actions.replay_effects");
    replay_effects->set_on_click([
        fade_in_label_ptr,
        fade_in_out_label_ptr,
        blink_label_ptr,
        pulse_label_ptr,
        fade_out_image_ptr,
        blink_image_ptr,
        pulse_image_ptr]()
    {
        fade_in_label_ptr->play();
        fade_in_out_label_ptr->play();
        blink_label_ptr->play();
        pulse_label_ptr->play();
        fade_out_image_ptr->play();
        blink_image_ptr->play();
        pulse_image_ptr->play();
    });
    effects_section->add_back(std::move(replay_effects));

    UiListContainer* animation_section = add_section(
        *page_content,
        "ui_component_gallery.media.animation_title",
        "ui_component_gallery.media.animation_description");

    // UiAnimation owns a playback instance but borrows persistent frame textures
    // from BuiltinResources. Hidden tab pages suspend its update calls.
    auto animation = std::make_unique<UiAnimation>(
        elysia::core::Rect{ 0,0,120,120 });
    if (!animation->set_engine_animation(
            *_builtin_resources,
            elysia::builtin::BuiltinAnimationId::EngineCharacterMove))
    {
        throw std::logic_error(
            "UiComponentGalleryScene could not bind the character move animation.");
    }
    UiAnimation* animation_ptr = animation.get();
    animation->play();
    animation_section->add_back(std::move(animation));

    // UiButtonGroup here provides keyboard-friendly transport controls; animation
    // commands mutate only the retained playback object, not the cached asset.
    auto transport = std::make_unique<UiButtonGroup>(
        elysia::core::Rect{ 0,0,790,52 });
    transport->set_direction(UiListDirection::Horizontal);
    transport->set_item_spacing(8.0f);
    auto play = compact_content_button("ui_component_gallery.actions.play");
    play->set_on_click([animation_ptr]() { animation_ptr->play(); });
    transport->add_button(std::move(play));
    auto pause = compact_content_button("ui_component_gallery.actions.pause");
    pause->set_on_click([animation_ptr]() { animation_ptr->pause(); });
    transport->add_button(std::move(pause));
    auto resume = compact_content_button("ui_component_gallery.actions.resume");
    resume->set_on_click([animation_ptr]() { animation_ptr->resume(); });
    transport->add_button(std::move(resume));
    auto reset = compact_content_button("ui_component_gallery.actions.reset_animation");
    reset->set_on_click([animation_ptr]() { animation_ptr->reset(); });
    transport->add_button(std::move(reset));
    animation_section->add_back(std::move(transport));
    return page;
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_containers_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);

    UiListContainer* list_section = add_section(
        *page_content,
        "ui_component_gallery.containers.list_title",
        "ui_component_gallery.containers.list_description");

    // UiListContainer uses one ordered child sequence for layout, rendering, and
    // primary-axis focus navigation in either vertical or horizontal direction.
    auto horizontal_list = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,790,52 });
    horizontal_list->set_direction(UiListDirection::Horizontal);
    horizontal_list->set_item_spacing(8.0f);
    horizontal_list->add_back(make_button("common.confirm"));
    horizontal_list->add_back(make_button("common.cancel"));
    horizontal_list->add_front(make_button("common.back"));
    list_section->add_back(std::move(horizontal_list));

    UiListContainer* layout_section = add_section(
        *page_content,
        "ui_component_gallery.containers.layout_title",
        "ui_component_gallery.containers.layout_description");

    // UiGridContainer calculates cells from column count and builds two-dimensional
    // focus neighbors from the same row-major child arrangement.
    auto grid = std::make_unique<UiGridContainer>(
        elysia::core::Rect{ 0,0,560,170 });
    grid->set_column_count(3);
    grid->set_cell_spacing(elysia::core::Vector2(8,8));
    for (int index = 0; index < 6; ++index)
    {
        grid->add_child(make_button(
            index % 2 ? "common.cancel" : "common.confirm"));
    }
    layout_section->add_back(std::move(grid));

    // UiPanel inserts each new child relative to the previous insertion point and
    // records matching directional focus links rather than imposing a list/grid.
    auto panel = std::make_unique<UiPanel>(
        elysia::core::Rect{ 0,0,620,180 });
    panel->add_child(make_button("common.confirm"),UiPanelInsertDirection::Down);
    panel->add_child(make_button("common.close"),UiPanelInsertDirection::Right);
    panel->add_child(make_button("common.cancel"),UiPanelInsertDirection::Down);
    panel->add_child(make_button("common.back"),UiPanelInsertDirection::Left);
    layout_section->add_back(std::move(panel));

    // UiChromeContainer owns action/title slots plus one body subtree; slot order
    // is independent from the focus scopes delegated by the inserted controls.
    auto chrome = std::make_unique<UiChromeContainer>(
        elysia::core::Rect{ 0,0,620,170 });
    chrome->add_left_action(make_button("common.back"));
    chrome->add_title_child(std::make_unique<UiLabel>(
        elysia::core::Rect{ 0,0,240,30 },0,
        ui_text_key("ui_component_gallery.containers.chrome")));
    chrome->add_right_action(make_button("common.close"));
    auto chrome_body = std::make_unique<UiPanel>(
        elysia::core::Rect{ 0,0,580,100 });
    chrome_body->add_child(make_button("ui_component_gallery.actions.replay"));
    chrome->set_body(std::move(chrome_body));
    layout_section->add_back(std::move(chrome));

    // UiTabContainer owns its internal UiTabBar and UiTabView. Selection changes
    // activate one page and deactivate every sibling page and delegated scope.
    auto nested_tabs = std::make_unique<UiTabContainer>(
        elysia::core::Rect{ 0,0,620,200 });
    auto first_page = std::make_unique<UiPanel>(
        elysia::core::Rect{ 0,0,580,140 });
    first_page->add_child(make_button("common.confirm"));
    (void)nested_tabs->add_tab(
        ui_text_key("ui_component_gallery.containers.nested_tab"),
        std::move(first_page));
    auto second_page = std::make_unique<UiPanel>(
        elysia::core::Rect{ 0,0,580,140 });
    second_page->add_child(make_button("common.cancel"));
    (void)nested_tabs->add_tab(
        ui_text_key("ui_component_gallery.containers.nested_tab_two"),
        std::move(second_page));
    layout_section->add_back(std::move(nested_tabs));

    UiListContainer* scroll_section = add_section(
        *page_content,
        "ui_component_gallery.containers.scroll_title",
        "ui_component_gallery.containers.scroll_description");

    // Auto shows scrollbars only for overflowing axes; Always reserves scrollbar
    // space unconditionally; Hidden preserves scrolling while suppressing chrome.
    for (const UiScrollBarVisibility visibility : {
            UiScrollBarVisibility::Auto,
            UiScrollBarVisibility::Always})
    {
        auto scroll = std::make_unique<UiScrollContainer>(
            elysia::core::Rect{ 0,0,500,110 });
        scroll->set_scroll_axis(UiScrollAxis::Vertical);
        scroll->set_scrollbar_visibility(visibility);
        auto scroll_content = std::make_unique<UiListContainer>(
            elysia::core::Rect{ 0,0,470,260 });
        scroll_content->set_item_spacing(6.0f);
        for (int index = 0; index < 5; ++index)
            scroll_content->add_back(make_button("common.close"));
        scroll->set_content(std::move(scroll_content));
        scroll_section->add_back(std::move(scroll));
    }

    auto hidden_scroll = std::make_unique<UiScrollContainer>(
        elysia::core::Rect{ 0,0,500,110 });
    hidden_scroll->set_scroll_axis(UiScrollAxis::Horizontal);
    hidden_scroll->set_scrollbar_visibility(UiScrollBarVisibility::Hidden);
    auto hidden_content = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,1240,90 });
    hidden_content->set_direction(UiListDirection::Horizontal);
    hidden_content->set_item_spacing(8.0f);
    for (int index = 0; index < 5; ++index)
        hidden_content->add_back(make_button("common.close"));
    hidden_scroll->set_content(std::move(hidden_content));
    scroll_section->add_back(std::move(hidden_scroll));
    return page;
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_appearance_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);
    UiListContainer* theme_section = add_section(
        *page_content,
        "ui_component_gallery.pages.appearance",
        "ui_component_gallery.sections.appearance");

    // UiThemeManager propagates a new theme through its registered root without
    // replacing per-widget style overrides or changing semantic visual roles.
    static constexpr std::array<const char*,7> theme_names{
        "Blue Glass Moon",
        "Elysia Light",
        "Elysia Dark",
        "EVA-00",
        "EVA-01",
        "EVA-02",
        "Quiet Slate"
    };
    static constexpr std::array<UiBuiltinTheme,7> themes{
        UiBuiltinTheme::BlueGlassMoon,
        UiBuiltinTheme::ElysiaLight,
        UiBuiltinTheme::ElysiaDark,
        UiBuiltinTheme::EvangelionUnit00,
        UiBuiltinTheme::EvangelionUnit01,
        UiBuiltinTheme::EvangelionUnit02,
        UiBuiltinTheme::QuietSlate
    };
    for (std::size_t index = 0; index < themes.size(); ++index)
    {
        auto theme_button = std::make_unique<UiButton>(
            elysia::core::Rect{ 0,0,240,40 },
            UiButtonConfig{ .content = ui_raw_text(theme_names[index]) },0);
        _theme_buttons[index] = theme_button.get();
        theme_button->set_on_click([this,theme = themes[index]]()
        {
            set_active_theme(theme);
        });
        theme_section->add_back(std::move(theme_button));
    }

    // RawText is rendered verbatim, while TextKey is resolved against the active
    // locale whenever cached text is refreshed.
    auto text_kinds = horizontal_content_row(790.0f,52.0f);
    auto localized = std::make_unique<UiLabel>(
        elysia::core::Rect{ 0,0,360,42 },0,
        ui_text_key("ui_component_gallery.appearance.localized_sample"));
    localized->set_visual_role(UiLabelVisualRole::Subtitle);
    text_kinds->add_back(std::move(localized));
    auto raw = std::make_unique<UiLabel>(
        elysia::core::Rect{ 0,0,360,42 },0,
        ui_raw_text("RawText: engine.theme.identifier"));
    raw->set_visual_role(UiLabelVisualRole::Muted);
    text_kinds->add_back(std::move(raw));
    theme_section->add_back(std::move(text_kinds));

    auto theme_note = std::make_unique<UiLabel>(
        elysia::core::Rect{ 0,0,680,32 },0,
        ui_text_key("ui_component_gallery.appearance.note"));
    theme_note->set_visual_role(UiLabelVisualRole::Muted);
    theme_section->add_back(std::move(theme_note));
    return page;
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_typography_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);
    UiListContainer* section = add_section(
        *page_content,
        "ui_component_gallery.typography.title",
        "ui_component_gallery.typography.description");

    struct TypographySample
    {
        const char* key;
        UiTypographyRole role;
        float height;
    };
    static constexpr std::array<TypographySample,7> samples{{
        { "ui_component_gallery.typography.sample_10",UiTypographyRole::Caption,26.0f },
        { "ui_component_gallery.typography.sample_20",UiTypographyRole::ButtonCompact,36.0f },
        { "ui_component_gallery.typography.sample_30",UiTypographyRole::Label,46.0f },
        { "ui_component_gallery.typography.sample_40",UiTypographyRole::Heading,56.0f },
        { "ui_component_gallery.typography.sample_50",UiTypographyRole::Subtitle,66.0f },
        { "ui_component_gallery.typography.sample_60",UiTypographyRole::DialogTitle,76.0f },
        { "ui_component_gallery.typography.sample_70",UiTypographyRole::Title,86.0f }
    }};

    // Typography roles select preloaded font resources and behavioral defaults;
    // disabling fit here exposes each role's native point-size progression.
    for (const TypographySample& sample : samples)
    {
        auto label = std::make_unique<UiLabel>(
            elysia::core::Rect{ 0,0,780,sample.height },0,
            ui_text_key(sample.key));
        label->set_typography_role(sample.role);
        label->set_text_fit_mode(UiLabelTextFitMode::None);
        section->add_back(std::move(label));
    }
    return page;
}
}
