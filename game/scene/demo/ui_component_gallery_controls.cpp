#include "ui_component_gallery_scene.h"

#include "../../../engine/ui/composites/ui_labeled_checkbox.h"
#include "../../../engine/ui/composites/ui_labeled_radio_button.h"
#include "../../../engine/ui/containers/ui_button_group.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_panel.h"
#include "../../../engine/ui/containers/ui_radio_group.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/text/ui_text_block.h"
#include "../../../engine/ui/widgets/ui_bar.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/widgets/ui_checkbox.h"
#include "../../../engine/ui/widgets/ui_drag_handle.h"
#include "../../../engine/ui/widgets/ui_radio_button.h"
#include "../../../engine/ui/widgets/ui_slider.h"
#include "../../../engine/ui/widgets/ui_text_input.h"

#include <array>
#include <memory>
#include <optional>

namespace example::scene
{
namespace
{
using namespace elysia::ui;

std::unique_ptr<UiListContainer> horizontal_row(float width,float height)
{
    auto row = std::make_unique<UiListContainer>(
        elysia::core::Rect{ 0,0,width,height });
    row->set_direction(UiListDirection::Horizontal);
    row->set_cross_align(UiLayoutAlign::Center);
    row->set_item_spacing(10.0f);
    return row;
}

std::unique_ptr<UiLabel> sample_label(
    const char* key,
    float width = 180.0f,
    float height = 40.0f)
{
    return std::make_unique<UiLabel>(
        elysia::core::Rect{ 0,0,width,height },0,ui_text_key(key));
}

std::unique_ptr<UiButton> compact_button(const char* key)
{
    return std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,180,40 },
        UiButtonConfig{ .content = ui_text_key(key) },0);
}
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_overview_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);
    UiListContainer* section = add_section(
        *page_content,
        "ui_component_gallery.pages.overview",
        "ui_component_gallery.sections.overview");

    // Semantic button roles keep intent stable while each theme supplies its own
    // colors. The callbacks are owned by the buttons and borrow the live scene.
    auto primary = make_button("ui_component_gallery.actions.replay");
    primary->set_visual_role(UiButtonVisualRole::Primary);
    primary->set_on_click([this]()
    {
        set_status_key("ui_component_gallery.status.interaction");
    });
    section->add_back(std::move(primary));

    auto danger = make_button("ui_component_gallery.actions.reset");
    danger->set_visual_role(UiButtonVisualRole::Danger);
    danger->set_on_click([this]() { rebuild_ui(); });
    section->add_back(std::move(danger));

    auto disabled = make_button("common.close");
    disabled->set_enabled(false);
    section->add_back(std::move(disabled));

    // UiBar is display-only: it clamps a value or ratio into themed fill geometry
    // and never participates in focus or input routing.
    auto progress = std::make_unique<UiBar>(
        elysia::core::Rect{ 0,0,420,22 });
    progress->set_visual_role(UiBarVisualRole::Progress);
    progress->set_ratio(0.68f);
    section->add_back(std::move(progress));
    return page;
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_states_page(SDL_Texture* image_texture)
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);

    UiListContainer* button_section = add_section(
        *page_content,
        "ui_component_gallery.states.buttons_title",
        "ui_component_gallery.states.buttons_description");

    // UiButton switches rendering paths from the content variant: monostate keeps
    // chrome only, text resolves localization, icon draws one borrowed texture,
    // and a texture set replaces chrome for every interaction state.
    auto button_modes = horizontal_row(790.0f,52.0f);
    button_modes->add_back(std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,120,40 }));
    button_modes->add_back(make_button("ui_component_gallery.states.text_button"));
    button_modes->add_back(std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,90,40 },
        UiButtonConfig{ .content = UiButtonIconContent{ image_texture } },0));
    const UiButtonTextures shared_textures{
        .idle = image_texture,
        .focused = image_texture,
        .pushed = image_texture,
        .disabled = image_texture
    };
    button_modes->add_back(std::make_unique<UiButton>(
        elysia::core::Rect{ 0,0,120,40 },
        UiButtonConfig{
            .content = UiButtonTextureSetContent{ shared_textures }
        },0));
    button_section->add_back(std::move(button_modes));

    auto button_roles = horizontal_row(790.0f,52.0f);
    for (const UiButtonVisualRole role : {
            UiButtonVisualRole::Default,
            UiButtonVisualRole::Primary,
            UiButtonVisualRole::Danger})
    {
        auto role_button = compact_button("ui_component_gallery.states.role_button");
        role_button->set_visual_role(role);
        button_roles->add_back(std::move(role_button));
    }
    auto disabled_button = compact_button("ui_component_gallery.states.disabled");
    disabled_button->set_enabled(false);
    button_roles->add_back(std::move(disabled_button));
    button_section->add_back(std::move(button_roles));

    UiListContainer* selection_section = add_section(
        *page_content,
        "ui_component_gallery.states.selection_title",
        "ui_component_gallery.states.selection_description");

    // UiCheckbox stores an independent tri-state value. Mark style changes only
    // presentation; user toggles advance logical state and then notify callbacks.
    auto checkbox_states = horizontal_row(790.0f,52.0f);
    for (const UiCheckboxState state : {
            UiCheckboxState::Unchecked,
            UiCheckboxState::Checked,
            UiCheckboxState::Indeterminate})
    {
        auto checkbox = std::make_unique<UiCheckbox>(
            elysia::core::Rect{ 0,0,44,44 });
        checkbox->set_state(state);
        checkbox_states->add_back(std::move(checkbox));
    }
    for (const UiCheckboxMarkStyle mark : {
            UiCheckboxMarkStyle::Checkmark,
            UiCheckboxMarkStyle::FilledBox,
            UiCheckboxMarkStyle::RadioDot})
    {
        auto checkbox = std::make_unique<UiCheckbox>(
            elysia::core::Rect{ 0,0,44,44 });
        checkbox->set_state(UiCheckboxState::Checked);
        checkbox->set_mark_style(mark);
        checkbox_states->add_back(std::move(checkbox));
    }
    auto disabled_checkbox = std::make_unique<UiCheckbox>(
        elysia::core::Rect{ 0,0,44,44 });
    disabled_checkbox->set_checked(true);
    disabled_checkbox->set_enabled(false);
    checkbox_states->add_back(std::move(disabled_checkbox));
    selection_section->add_back(std::move(checkbox_states));

    // UiRadioButton exposes a single selected bit; UiRadioGroup is responsible
    // for clearing peer items when one child becomes selected.
    auto radio_states = horizontal_row(790.0f,52.0f);
    auto unselected_radio = std::make_unique<UiRadioButton>(
        elysia::core::Rect{ 0,0,44,44 });
    radio_states->add_back(std::move(unselected_radio));
    auto selected_radio = std::make_unique<UiRadioButton>(
        elysia::core::Rect{ 0,0,44,44 });
    selected_radio->set_selected(true);
    radio_states->add_back(std::move(selected_radio));
    auto disabled_radio = std::make_unique<UiRadioButton>(
        elysia::core::Rect{ 0,0,44,44 });
    disabled_radio->set_selected(true);
    disabled_radio->set_enabled(false);
    radio_states->add_back(std::move(disabled_radio));
    selection_section->add_back(std::move(radio_states));

    UiListContainer* presentation_section = add_section(
        *page_content,
        "ui_component_gallery.states.presentation_title",
        "ui_component_gallery.states.presentation_description");

    // UiLabel visual roles are semantic theme lookup keys; they change palette
    // and default emphasis without changing the localized text or layout contract.
    auto label_roles = horizontal_row(790.0f,50.0f);
    for (const UiLabelVisualRole role : {
            UiLabelVisualRole::Default,
            UiLabelVisualRole::Title,
            UiLabelVisualRole::Subtitle,
            UiLabelVisualRole::Muted})
    {
        auto label = sample_label(
            "ui_component_gallery.states.panel_sample",180.0f,40.0f);
        label->set_visual_role(role);
        label_roles->add_back(std::move(label));
    }
    presentation_section->add_back(std::move(label_roles));

    // UiLabel fit modes either preserve native glyph size, shrink oversized text,
    // or scale the text to consume the available single-line content rectangle.
    auto label_modes = horizontal_row(790.0f,58.0f);
    for (const UiLabelTextFitMode mode : {
            UiLabelTextFitMode::None,
            UiLabelTextFitMode::ShrinkToFit,
            UiLabelTextFitMode::ScaleToFit})
    {
        auto label = sample_label("ui_component_gallery.states.fit_sample",220.0f,50.0f);
        label->set_text_fit_mode(mode);
        label_modes->add_back(std::move(label));
    }
    presentation_section->add_back(std::move(label_modes));

    // UiTextBlock measures localized multi-line content and participates in its
    // parent's intrinsic height calculation; its visual role only changes style.
    auto text_roles = horizontal_row(790.0f,82.0f);
    for (const UiTextBlockVisualRole role : {
            UiTextBlockVisualRole::Default,
            UiTextBlockVisualRole::Muted})
    {
        auto block = std::make_unique<UiTextBlock>(
            elysia::core::Rect{ 0,0,360,72 });
        block->set_text_content(ui_text_key(
            "ui_component_gallery.states.text_block_sample"));
        block->set_visual_role(role);
        block->set_padding(6);
        text_roles->add_back(std::move(block));
    }
    presentation_section->add_back(std::move(text_roles));

    // UiPanel owns focusable children and derives directional navigation from
    // insertion links while semantic roles select themed surface treatments.
    auto panel_roles = horizontal_row(790.0f,100.0f);
    const std::array<UiPanelVisualRole,4> roles{
        UiPanelVisualRole::Default,
        UiPanelVisualRole::Screen,
        UiPanelVisualRole::Dialog,
        UiPanelVisualRole::List
    };
    for (const UiPanelVisualRole role : roles)
    {
        auto panel = std::make_unique<UiPanel>(
            elysia::core::Rect{ 0,0,180,90 });
        panel->set_visual_role(role);
        panel->add_child(sample_label(
            "ui_component_gallery.states.panel_sample",150.0f,36.0f));
        panel_roles->add_back(std::move(panel));
    }
    presentation_section->add_back(std::move(panel_roles));

    UiListContainer* input_section = add_section(
        *page_content,
        "ui_component_gallery.states.inputs_title",
        "ui_component_gallery.states.inputs_description");

    // UiTextInput claims text-input ownership only while focused. Placeholder,
    // committed text, caret, and disabled colors are rendered by separate paths.
    auto inputs = horizontal_row(790.0f,56.0f);
    auto placeholder = std::make_unique<UiTextInput>(
        elysia::core::Rect{ 0,0,240,44 });
    placeholder->set_placeholder_content(ui_text_key(
        "ui_component_gallery.controls.placeholder"));
    placeholder->set_max_length(16);
    inputs->add_back(std::move(placeholder));
    auto prefilled = std::make_unique<UiTextInput>(
        elysia::core::Rect{ 0,0,240,44 });
    prefilled->set_text("Prefilled UTF-8");
    inputs->add_back(std::move(prefilled));
    auto disabled_input = std::make_unique<UiTextInput>(
        elysia::core::Rect{ 0,0,240,44 });
    disabled_input->set_text("Disabled");
    disabled_input->set_enabled(false);
    inputs->add_back(std::move(disabled_input));
    input_section->add_back(std::move(inputs));
    return page;
}

std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_controls_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);
    UiListContainer* section = add_section(
        *page_content,
        "ui_component_gallery.pages.controls",
        "ui_component_gallery.sections.controls");

    // UiButtonGroup adopts buttons, installs group-owned selection callbacks, and
    // applies the Primary role to exactly one selected member.
    auto group = std::make_unique<UiButtonGroup>(
        elysia::core::Rect{ 0,0,760,52 });
    group->set_direction(UiListDirection::Horizontal);
    group->set_item_spacing(8.0f);
    group->add_button(make_button("common.confirm"));
    group->add_button(make_button("common.cancel"));
    group->add_button(make_button("common.close"));
    section->add_back(std::move(group));

    // UiLabeledCheckbox routes input from its entire control rect to an embedded
    // checkbox while independently positioning the owned label on either side.
    auto labeled_checks = horizontal_row(790.0f,54.0f);
    UiLabeledCheckboxConfig left_check_config{};
    left_check_config.text_content = ui_text_key(
        "ui_component_gallery.controls.labeled_check");
    left_check_config.label_placement = UiLabeledCheckboxLabelPlacement::Left;
    left_check_config.text_placement = UiLabeledCheckboxTextPlacement::FarEdge;
    labeled_checks->add_back(std::make_unique<UiLabeledCheckbox>(
        elysia::core::Rect{ 0,0,360,42 },left_check_config));
    UiLabeledCheckboxConfig right_check_config{};
    right_check_config.text_content = ui_text_key(
        "ui_component_gallery.controls.labeled_check");
    labeled_checks->add_back(std::make_unique<UiLabeledCheckbox>(
        elysia::core::Rect{ 0,0,360,42 },right_check_config));
    section->add_back(std::move(labeled_checks));

    // UiRadioGroup discovers UiRadioItem children, enforces mutual exclusion, and
    // emits one optional selected index when a bare or labeled item changes.
    auto radios = std::make_unique<UiRadioGroup>(
        elysia::core::Rect{ 0,0,760,104 });
    radios->set_direction(UiListDirection::Horizontal);
    radios->set_item_spacing(12.0f);
    radios->set_on_selection_changed([this](std::optional<std::size_t>)
    {
        set_status_key("ui_component_gallery.status.interaction");
    });
    radios->add_back(std::make_unique<UiRadioButton>(
        elysia::core::Rect{ 0,0,42,42 }));
    UiLabeledRadioButtonConfig near_radio_config{};
    near_radio_config.text_content = ui_text_key(
        "ui_component_gallery.controls.labeled_radio");
    radios->add_back(std::make_unique<UiLabeledRadioButton>(
        elysia::core::Rect{ 0,0,300,42 },near_radio_config));
    UiLabeledRadioButtonConfig far_radio_config{};
    far_radio_config.text_content = ui_text_key(
        "ui_component_gallery.controls.labeled_radio_far");
    far_radio_config.label_placement = UiLabeledRadioLabelPlacement::Left;
    far_radio_config.text_placement = UiLabeledRadioTextPlacement::FarEdge;
    radios->add_back(std::make_unique<UiLabeledRadioButton>(
        elysia::core::Rect{ 0,0,340,42 },far_radio_config));
    section->add_back(std::move(radios));

    // UiSlider composes a bar, numeric value, and drag handle. Pointer dragging
    // and keyboard adjustment share range clamping and optional step snapping.
    auto sliders = horizontal_row(790.0f,150.0f);
    auto stepped_slider = std::make_unique<UiSlider>(
        elysia::core::Rect{ 0,0,520,48 });
    stepped_slider->set_range(-10.0f,10.0f);
    stepped_slider->set_step(2.0f);
    stepped_slider->set_value_display(UiSliderValueDisplay::Value);
    stepped_slider->set_value(4.0f);
    stepped_slider->set_on_value_changed([this](float)
    {
        set_status_key("ui_component_gallery.status.interaction");
    });
    sliders->add_back(std::move(stepped_slider));
    auto vertical_slider = std::make_unique<UiSlider>(
        elysia::core::Rect{ 0,0,64,130 });
    vertical_slider->set_orientation(UiSliderOrientation::Vertical);
    vertical_slider->set_value_display(UiSliderValueDisplay::Percent);
    vertical_slider->set_value(0.72f);
    sliders->add_back(std::move(vertical_slider));
    section->add_back(std::move(sliders));

    // UiTextInput edits UTF-8 by codepoint, owns IME composition only while
    // focused, clamps committed text to max length, and releases ownership on blur.
    auto input = std::make_unique<UiTextInput>(
        elysia::core::Rect{ 0,0,520,44 });
    input->set_placeholder_content(ui_text_key(
        "ui_component_gallery.controls.placeholder"));
    input->set_max_length(16);
    input->set_on_text_changed([this](std::string_view)
    {
        set_status_key("ui_component_gallery.status.interaction");
    });
    input->set_on_submit([this](std::string_view)
    {
        set_status_key("ui_component_gallery.status.submitted");
    });
    section->add_back(std::move(input));

    // UiDragHandle captures the primary pointer, preserves the grab offset, and
    // clamps movement to the configured axis and optional screen-space bounds.
    auto handles = horizontal_row(790.0f,74.0f);
    for (const UiDragAxis axis : {
            UiDragAxis::Free,
            UiDragAxis::Horizontal,
            UiDragAxis::Vertical})
    {
        UiDragHandleConfig config{};
        config.axis = axis;
        config.drag_bounds = elysia::core::Rect{ 0,0,760,64 };
        auto handle = std::make_unique<UiDragHandle>(
            elysia::core::Rect{ 0,0,32,32 },config);
        handle->set_on_dragged([this](const elysia::core::Vector2&)
        {
            set_status_key("ui_component_gallery.status.interaction");
        });
        handles->add_back(std::move(handle));
    }
    section->add_back(std::move(handles));
    return page;
}
}
