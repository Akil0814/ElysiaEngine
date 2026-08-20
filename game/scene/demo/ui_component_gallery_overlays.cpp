#include "ui_component_gallery_scene.h"

#include "../../../engine/ui/composites/ui_confirmation_dialog.h"
#include "../../../engine/ui/composites/ui_dialog.h"
#include "../../../engine/ui/composites/ui_dropdown.h"
#include "../../../engine/ui/composites/ui_tooltip.h"
#include "../../../engine/ui/containers/ui_button_group.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/containers/ui_panel.h"
#include "../../../engine/ui/containers/ui_scroll_container.h"
#include "../../../engine/ui/widgets/text/ui_text_block.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace example::scene
{
std::unique_ptr<elysia::ui::UiScrollContainer>
UiComponentGalleryScene::build_overlays_page()
{
    using namespace elysia::ui;
    UiListContainer* page_content = nullptr;
    auto page = make_page_scroll(page_content);

    UiListContainer* placement_section = add_section(
        *page_content,
        "ui_component_gallery.overlays.placement_title",
        "ui_component_gallery.overlays.placement_description");

    // UiPanel remains owned by UiWindow as a normal child. Overlay registration
    // borrows the pointer and temporarily moves its rendering/input above base UI.
    auto overlay = std::make_unique<UiPanel>(
        elysia::core::Rect{ 0,0,320,180 });
    UiPanel* overlay_ptr = overlay.get();
    auto close_overlay = make_button("common.close");
    close_overlay->set_on_click([this,overlay_ptr]()
    {
        _root_window->close_overlay(*overlay_ptr);
    });
    overlay->add_child(std::move(close_overlay));
    _root_window->add_child(std::move(overlay));
    if (!_root_window->register_overlay(*overlay_ptr,UiOverlayOptions{
            .open = false,
            .modal = false,
            .close_on_cancel = true,
            .close_on_outside_click = true,
            .placement = UiOverlayPlacement::Center,
            .transition = UiOverlayTransition::None,
            .fallback_size = elysia::core::Vector2(320,180),
            .order = 900
        }))
    {
        throw std::logic_error(
            "UiComponentGalleryScene could not register the placement overlay.");
    }

    // Overlay options are mutable registration state. Opening after mutation
    // reapplies placement and transition while preserving the owned panel subtree.
    const auto open_at = [this,overlay_ptr](
        UiOverlayPlacement placement,
        UiOverlayTransition transition,
        bool modal)
    {
        if (UiOverlayOptions* options = _root_window->overlay_options(*overlay_ptr))
        {
            options->placement = placement;
            options->transition = transition;
            options->modal = modal;
            options->close_on_cancel = true;
            options->close_on_outside_click = true;
        }
        _root_window->open_overlay(*overlay_ptr);
        set_status_key("ui_component_gallery.status.overlay_opened");
    };

    // UiButtonGroup makes placement choices keyboard navigable and visually marks
    // the most recently selected placement without owning the overlay itself.
    auto placement_buttons = std::make_unique<UiButtonGroup>(
        elysia::core::Rect{ 0,0,360,228 });
    auto center = make_button("ui_component_gallery.overlays.open_center");
    center->set_on_click([open_at]()
    {
        open_at(UiOverlayPlacement::Center,UiOverlayTransition::None,false);
    });
    placement_buttons->add_button(std::move(center));
    auto left = make_button("ui_component_gallery.overlays.open_left");
    left->set_on_click([open_at]()
    {
        open_at(UiOverlayPlacement::LeftDrawer,UiOverlayTransition::Slide,true);
    });
    placement_buttons->add_button(std::move(left));
    auto right = make_button("ui_component_gallery.overlays.open_right");
    right->set_on_click([open_at]()
    {
        open_at(UiOverlayPlacement::RightDrawer,UiOverlayTransition::Slide,true);
    });
    placement_buttons->add_button(std::move(right));
    auto top = make_button("ui_component_gallery.overlays.open_top");
    top->set_on_click([open_at]()
    {
        open_at(UiOverlayPlacement::TopSheet,UiOverlayTransition::Slide,true);
    });
    placement_buttons->add_button(std::move(top));
    auto bottom = make_button("ui_component_gallery.overlays.open_bottom");
    bottom->set_on_click([open_at]()
    {
        open_at(UiOverlayPlacement::BottomSheet,UiOverlayTransition::Slide,true);
    });
    placement_buttons->add_button(std::move(bottom));
    placement_section->add_back(std::move(placement_buttons));

    UiListContainer* dialog_section = add_section(
        *page_content,
        "ui_component_gallery.overlays.dialog_title",
        "ui_component_gallery.overlays.dialog_description");

    // UiDialog composes chrome, a scrollable reading body, and one action. Window
    // registration supplies modal focus capture and restores the previous scope.
    auto dialog = std::make_unique<UiDialog>(
        elysia::core::Rect{ 0,0,480,300 });
    UiDialog* dialog_ptr = dialog.get();
    dialog->set_title_content(ui_text_key(
        "ui_component_gallery.dialog.title"));
    dialog->set_body_content(ui_text_key(
        "ui_component_gallery.dialog.body"));
    dialog->set_action_content(ui_text_key("common.close"));
    _root_window->add_child(std::move(dialog));
    (void)dialog_ptr->register_with_window(*_root_window);
    auto open_dialog = make_button(
        "ui_component_gallery.overlays.open_dialog");
    open_dialog->set_on_click([dialog_ptr]() { dialog_ptr->open(); });
    dialog_section->add_back(std::move(open_dialog));

    // UiConfirmationDialog owns three actions and resolves the result callbacks
    // before closing its registered modal overlay and restoring prior focus.
    auto confirmation = std::make_unique<UiConfirmationDialog>(
        elysia::core::Rect{ 0,0,440,220 });
    UiConfirmationDialog* confirmation_ptr = confirmation.get();
    confirmation->set_config(UiConfirmationDialogConfig{
        .title = ui_text_key("ui_component_gallery.confirm.title"),
        .message = ui_text_key("ui_component_gallery.confirm.message"),
        .confirm = ui_text_key("common.confirm"),
        .cancel = ui_text_key("common.cancel"),
        .close = ui_text_key("common.close"),
        .confirm_visual_role = UiButtonVisualRole::Danger
    });
    _root_window->add_child(std::move(confirmation));
    (void)confirmation_ptr->register_with_window(*_root_window);
    auto open_confirmation = make_button(
        "ui_component_gallery.overlays.open_confirm");
    open_confirmation->set_on_click([confirmation_ptr]()
    {
        confirmation_ptr->open();
    });
    dialog_section->add_back(std::move(open_confirmation));

    UiListContainer* popup_section = add_section(
        *page_content,
        "ui_component_gallery.overlays.popup_title",
        "ui_component_gallery.overlays.popup_description");

    // UiDropdown owns its trigger and scrollable option rows, but it must register
    // with UiWindow so the transient popup renders above ordinary children. Disabled
    // options stay visible and are skipped by directional focus navigation.
    auto dropdown = std::make_unique<UiDropdown>(
        elysia::core::Rect{ 0,0,360,42 });
    dropdown->set_options({
        UiDropdownOption{ ui_text_key("common.confirm") },
        UiDropdownOption{ ui_text_key("common.cancel") },
        UiDropdownOption{ ui_text_key("common.close"),false },
        UiDropdownOption{ ui_text_key("ui_component_gallery.overlays.option_four") },
        UiDropdownOption{ ui_text_key("ui_component_gallery.overlays.option_five") },
        UiDropdownOption{ ui_text_key("ui_component_gallery.overlays.option_six") },
        UiDropdownOption{ ui_text_key("ui_component_gallery.overlays.option_seven") },
        UiDropdownOption{ ui_text_key("ui_component_gallery.overlays.option_eight") }
    });
    (void)dropdown->set_selected_index(0);
    dropdown->set_on_selection_changed([this](std::size_t)
    {
        set_status_key("ui_component_gallery.status.selection_changed");
    });
    dropdown->register_with_window(*_root_window);
    popup_section->add_back(std::move(dropdown));

    // UiTooltip borrows its trigger and registers passive content with UiWindow.
    // Visibility follows pointer hover or focus without stealing the active scope.
    auto tooltip_trigger = make_button(
        "ui_component_gallery.overlays.tooltip");
    UiButton* tooltip_trigger_ptr = tooltip_trigger.get();
    popup_section->add_back(std::move(tooltip_trigger));
    auto* tooltip = _root_window->create_child<UiTooltip>(0);
    auto tooltip_content = std::make_unique<UiTextBlock>(
        elysia::core::Rect{ 0,0,280,72 });
    tooltip_content->set_text_content(ui_text_key(
        "ui_component_gallery.overlays.tooltip_text"));
    tooltip_content->set_padding(8);
    tooltip->bind_trigger(*tooltip_trigger_ptr);
    tooltip->set_content(std::move(tooltip_content));
    tooltip->register_with_window(*_root_window);
    return page;
}
}
