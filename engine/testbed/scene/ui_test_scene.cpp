#include "ui_test_scene.h"

#include "../../builtin/resources/builtin_asset_cache.h"
#include "../../builtin/resources/builtin_asset_keys.h"
#include "../../ui/composites/ui_confirmation_dialog.h"
#include "../../ui/composites/ui_dialog.h"
#include "../../ui/composites/ui_dropdown.h"
#include "../../ui/composites/ui_labeled_checkbox.h"
#include "../../ui/composites/ui_labeled_radio_button.h"
#include "../../ui/composites/ui_tab_container.h"
#include "../../ui/composites/ui_tooltip.h"
#include "../../ui/containers/ui_button_group.h"
#include "../../ui/containers/ui_chrome_container.h"
#include "../../ui/containers/ui_grid_container.h"
#include "../../ui/containers/ui_list_container.h"
#include "../../ui/containers/ui_panel.h"
#include "../../ui/containers/ui_radio_group.h"
#include "../../ui/containers/ui_scroll_container.h"
#include "../../ui/widgets/image/ui_animation.h"
#include "../../ui/widgets/image/ui_blink_image.h"
#include "../../ui/widgets/image/ui_fade_image.h"
#include "../../ui/widgets/image/ui_image.h"
#include "../../ui/widgets/image/ui_pulse_image.h"
#include "../../ui/widgets/label/ui_blink_label.h"
#include "../../ui/widgets/label/ui_fade_label.h"
#include "../../ui/widgets/label/ui_label.h"
#include "../../ui/widgets/label/ui_pulse_label.h"
#include "../../ui/widgets/number/ui_number.h"
#include "../../ui/widgets/text/ui_text_block.h"
#include "../../ui/widgets/ui_bar.h"
#include "../../ui/widgets/ui_button.h"
#include "../../ui/widgets/ui_checkbox.h"
#include "../../ui/widgets/ui_drag_handle.h"
#include "../../ui/widgets/ui_radio_button.h"
#include "../../ui/widgets/ui_slider.h"
#include "../../ui/widgets/ui_text_input.h"
#include "../../ui/window/ui_window.h"
#include "../../scene/runtime/scene_runtime_context.h"
#include "../../input/raw_input_types.h"

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>

namespace elysia::testbed
{
using elysia::typography::UiTypographyRole;

namespace
{
using namespace elysia::ui;

UiLayoutChildOptions at(float x,float y,float width,float height)
{
    return UiLayoutChildOptions{ ._anchor = UiLayoutAnchor::TopLeft,
        ._margin = UiLayoutMargin{ x,y,0.0f,0.0f }, ._cross_align = UiLayoutAlign::Start,
        ._size_override = elysia::core::Vector2(width,height), ._use_custom_cross_align = false,
        ._fill_cross_axis = false, ._use_size_override = true };
}

std::unique_ptr<UiButton> button(const char* key)
{
    return std::make_unique<UiButton>(elysia::core::Rect{ 0,0,240,40 },
        UiButtonConfig{ .content = ui_text_key(key) },0);
}

std::unique_ptr<UiScrollContainer> page_scroll(UiListContainer*& content)
{
    auto page = std::make_unique<UiScrollContainer>(elysia::core::Rect{ 0,0,900,390 });
    page->set_scroll_axis(UiScrollAxis::Vertical);
    page->set_scrollbar_visibility(UiScrollBarVisibility::Auto);
    page->set_scroll_step(elysia::core::Vector2(0.0f,36.0f));
    auto list = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,870,0 });
    list->set_padding(UiLayoutPadding{ 12,12,12,12 });
    list->set_item_spacing(12.0f);
    list->set_cross_align(UiLayoutAlign::Start);
    content = list.get();
    page->set_content(std::move(list));
    return page;
}

UiListContainer* add_section(UiListContainer& page,const char* title,const char* description)
{
    auto chrome = std::make_unique<UiChromeContainer>(elysia::core::Rect{ 0,0,840,0 });
    chrome->set_header_height(42.0f);
    auto heading = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,420,32 },0,ui_text_key(title));
    heading->set_visual_role(UiLabelVisualRole::Title);
    chrome->add_title_child(std::move(heading));
    auto body = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,820,0 });
    body->set_padding(UiLayoutPadding{ 12,8,12,8 });
    body->set_item_spacing(8.0f);
    body->set_cross_align(UiLayoutAlign::Start);
    auto note = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,760,28 },0,ui_text_key(description));
    note->set_visual_role(UiLabelVisualRole::Muted);
    body->add_back(std::move(note));
    UiListContainer* body_ptr = body.get();
    chrome->set_body(std::move(body));
    page.add_back(std::move(chrome));
    return body_ptr;
}

bool is_valid_return_route(const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}
}

void UiTestScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type == elysia::input::RawInputEventType::ControlPressed)
        {
            return_to_caller();
            return;
        }
    }

    elysia::scene::Scene::on_input(input,events);
}

void UiTestScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const TestbedScenePayload* test_payload =
        elysia::scene::try_scene_payload<TestbedScenePayload>(payload);
    if (!test_payload || !is_valid_return_route(test_payload->return_route))
        throw std::logic_error("UiTestScene requires TestbedScenePayload with a valid return route.");
    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache || !cache->initialized())
        throw std::logic_error("UiTestScene requires an initialized BuiltinAssetCache.");
    _return_route = test_payload->return_route;
    _paused = false;
    rebuild_ui();
}

void UiTestScene::on_exit() { _paused = false; clear_ui(); }
void UiTestScene::reset() { _paused = false; clear_ui(); _return_route = {}; }

void UiTestScene::refresh_theme_preview_styles()
{
    if (!_root_window) return;
    auto overrides = _root_window->style_overrides();
    overrides.draw_background = true;
    overrides.draw_border = true;
    _root_window->set_style_overrides(overrides);
}

void UiTestScene::set_status_key(const char* key)
{
    if (_status_label)
        _status_label->set_text_content(ui_text_key(key));
}

void UiTestScene::set_active_theme(UiBuiltinTheme theme)
{
    _theme_manager.set_theme(theme);
    refresh_theme_preview_styles();
    sync_theme_switch_button_roles();
    set_status_key("engine.ui_test.status.interaction");
}

void UiTestScene::sync_theme_switch_button_roles() noexcept
{
    static constexpr std::array<UiBuiltinTheme,7> themes{ UiBuiltinTheme::BlueGlassMoon,
        UiBuiltinTheme::ElysiaLight,UiBuiltinTheme::ElysiaDark,UiBuiltinTheme::EvangelionUnit00,
        UiBuiltinTheme::EvangelionUnit01,UiBuiltinTheme::EvangelionUnit02,UiBuiltinTheme::QuietSlate };
    for (std::size_t index = 0; index < themes.size(); ++index)
        if (_theme_buttons[index])
            _theme_buttons[index]->set_visual_role(_theme_manager.current_builtin_theme() == themes[index]
                ? UiButtonVisualRole::Primary : UiButtonVisualRole::Default);
}

void UiTestScene::rebuild_ui()
{
    clear_ui();
    const auto* cache = runtime_context().builtin_asset_cache();
    if (!cache) throw std::logic_error("UiTestScene requires BuiltinAssetCache while building UI.");
    SDL_Texture* image_texture = cache->find_texture(
        elysia::builtin::asset_keys::ElysiaDefaultTexture);
    if (!image_texture) throw std::logic_error("UiTestScene requires engine.brand.elysia.default.");

    _root_window = create_and_add_object<UiWindow>(elysia::core::Rect{ 80,52,1120,616 },100);
    _root_window->set_padding(UiLayoutPadding{ 16,16,16,16 });
    _root_window->set_on_cancel([this]() { return_to_caller(); });
    _theme_registrations.push_back(_theme_manager.register_root(*_root_window));
    refresh_theme_preview_styles();
    auto status = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,780,30 },0,ui_text_key("engine.ui_test.status.ready"));
    status->set_visual_role(UiLabelVisualRole::Subtitle); _status_label = status.get();
    _root_window->add_child(std::move(status),at(16,12,850,30));

    auto workbench = std::make_unique<UiTabContainer>(elysia::core::Rect{ 0,0,1080,530 });
    UiTabContainer* tabs = workbench.get();

    UiListContainer* overview_list = nullptr; auto overview = page_scroll(overview_list);
    auto* overview_section = add_section(*overview_list,"engine.ui_test.pages.overview","engine.ui_test.sections.overview");
    auto primary = button("engine.ui_test.actions.replay"); primary->set_visual_role(UiButtonVisualRole::Primary);
    primary->set_on_click([this]() { set_status_key("engine.ui_test.status.interaction"); }); overview_section->add_back(std::move(primary));
    auto danger = button("engine.ui_test.actions.reset"); danger->set_visual_role(UiButtonVisualRole::Danger);
    danger->set_on_click([this]() { rebuild_ui(); }); overview_section->add_back(std::move(danger));
    auto disabled = button("engine.common.close"); disabled->set_enabled(false); overview_section->add_back(std::move(disabled));
    auto progress = std::make_unique<UiBar>(elysia::core::Rect{ 0,0,420,22 }); progress->set_visual_role(UiBarVisualRole::Progress); progress->set_ratio(0.68f); overview_section->add_back(std::move(progress));
    (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.overview"),std::move(overview));

    UiListContainer* controls_list = nullptr; auto controls = page_scroll(controls_list);
    auto* controls_section = add_section(*controls_list,"engine.ui_test.pages.controls","engine.ui_test.sections.controls");
    auto group = std::make_unique<UiButtonGroup>(elysia::core::Rect{ 0,0,320,120 });
    group->add_button(button("engine.common.confirm")); group->add_button(button("engine.common.cancel")); group->add_button(button("engine.common.close")); controls_section->add_back(std::move(group));
    auto check = std::make_unique<UiCheckbox>(elysia::core::Rect{ 0,0,44,44 }); check->set_mark_style(UiCheckboxMarkStyle::Checkmark);
    check->set_on_toggled([this](UiCheckboxState) { set_status_key("engine.ui_test.status.interaction"); }); controls_section->add_back(std::move(check));
    UiLabeledCheckboxConfig check_config{}; check_config.text_content = ui_text_key("engine.ui_test.controls.labeled_check"); controls_section->add_back(std::make_unique<UiLabeledCheckbox>(elysia::core::Rect{ 0,0,360,42 },check_config));
    auto radios = std::make_unique<UiRadioGroup>(elysia::core::Rect{ 0,0,360,130 }); radios->set_on_selection_changed([this](std::optional<std::size_t>) { set_status_key("engine.ui_test.status.interaction"); });
    radios->add_back(std::make_unique<UiRadioButton>(elysia::core::Rect{ 0,0,42,42 })); UiLabeledRadioButtonConfig radio_config{}; radio_config.text_content = ui_text_key("engine.ui_test.controls.labeled_radio"); radios->add_back(std::make_unique<UiLabeledRadioButton>(elysia::core::Rect{ 0,0,300,42 },radio_config)); controls_section->add_back(std::move(radios));
    auto slider = std::make_unique<UiSlider>(elysia::core::Rect{ 0,0,360,48 }); slider->set_value_display(UiSliderValueDisplay::Percent); slider->set_value(0.5f); slider->set_on_value_changed([this](float) { set_status_key("engine.ui_test.status.interaction"); }); controls_section->add_back(std::move(slider));
    auto vertical_slider = std::make_unique<UiSlider>(elysia::core::Rect{ 0,0,64,130 }); vertical_slider->set_orientation(UiSliderOrientation::Vertical); vertical_slider->set_value_display(UiSliderValueDisplay::Value); vertical_slider->set_value(0.72f); controls_section->add_back(std::move(vertical_slider));
    auto input = std::make_unique<UiTextInput>(elysia::core::Rect{ 0,0,360,44 }); input->set_placeholder_content(ui_text_key("engine.ui_test.controls.placeholder")); input->set_max_length(16); input->set_on_submit([this](std::string_view) { set_status_key("engine.ui_test.status.interaction"); }); controls_section->add_back(std::move(input));
    UiDragHandleConfig drag_config{}; drag_config.axis = UiDragAxis::Horizontal; drag_config.drag_bounds = elysia::core::Rect{ 0,0,400,50 }; auto drag = std::make_unique<UiDragHandle>(elysia::core::Rect{ 0,0,28,28 },drag_config); drag->set_on_dragged([this](const elysia::core::Vector2&) { set_status_key("engine.ui_test.status.interaction"); }); controls_section->add_back(std::move(drag));
    (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.controls"),std::move(controls));

    UiListContainer* media_list = nullptr; auto media = page_scroll(media_list);
    auto* media_section = add_section(*media_list,"engine.ui_test.pages.media","engine.ui_test.sections.media");
    auto fade_label = std::make_unique<UiFadeLabel>(elysia::core::Rect{ 0,0,360,34 },0,ui_text_key("engine.ui_test.actions.replay")); fade_label->configure_playback(effects::UiOpacityFadeMode::FadeInOut,0.0,0.25,0.25); fade_label->play(); media_section->add_back(std::move(fade_label));
    auto blink_label = std::make_unique<UiBlinkLabel>(elysia::core::Rect{ 0,0,360,34 },0,ui_text_key("engine.ui_test.pages.media")); blink_label->configure_playback(effects::UiOpacityBlinkMode::VisibleFirst,0.0,0.2,0.2,2); blink_label->play(); media_section->add_back(std::move(blink_label));
    auto pulse_label = std::make_unique<UiPulseLabel>(elysia::core::Rect{ 0,0,360,34 },0,ui_text_key("engine.ui_test.sections.media")); pulse_label->configure_playback(effects::UiOpacityPulseMode::MinToMax,0.0,0.25,0.25,2); pulse_label->play(); media_section->add_back(std::move(pulse_label));
    auto text = std::make_unique<UiTextBlock>(elysia::core::Rect{ 0,0,700,84 }); text->set_text_content(ui_text_key("engine.ui_test.media.long_text")); text->set_padding(6); media_section->add_back(std::move(text));
    auto number = std::make_unique<UiNumber>(elysia::core::Rect{ 0,0,240,42 }); number->set_value(73.25); number->set_decimal_places(2); number->set_suffix(UiNumberSuffix::Percent); media_section->add_back(std::move(number));
    for (const auto direction : { BarFillDirection::LeftToRight,BarFillDirection::RightToLeft,BarFillDirection::TopToBottom,BarFillDirection::BottomToTop }) { auto bar = std::make_unique<UiBar>(elysia::core::Rect{ 0,0,300,20 }); bar->set_ratio(0.58f); bar->set_fill_direction(direction); media_section->add_back(std::move(bar)); }
    media_section->add_back(std::make_unique<UiImage>(image_texture,elysia::core::Rect{ 0,0,120,80 })); auto fade = std::make_unique<UiFadeImage>(image_texture,elysia::core::Rect{ 0,0,120,80 }); fade->configure_playback(effects::UiOpacityFadeMode::FadeInOut,0.1,0.35,0.35); fade->play(); media_section->add_back(std::move(fade)); auto blink = std::make_unique<UiBlinkImage>(image_texture,elysia::core::Rect{ 0,0,120,80 }); blink->configure_playback(effects::UiOpacityBlinkMode::VisibleFirst,0.0,0.25,0.25,2); blink->play(); media_section->add_back(std::move(blink)); auto pulse = std::make_unique<UiPulseImage>(image_texture,elysia::core::Rect{ 0,0,120,80 }); pulse->configure_playback(effects::UiOpacityPulseMode::MinToMax,0.0,0.3,0.3,2); pulse->play(); media_section->add_back(std::move(pulse));
    auto animation = std::make_unique<UiAnimation>(elysia::core::Rect{ 0,0,120,120 }); if (!animation->set_engine_animation(*cache,elysia::builtin::asset_keys::EngineCharacterMoveAnimation)) throw std::logic_error("UiTestScene could not bind the character move animation."); animation->play(); media_section->add_back(std::move(animation));
    (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.media"),std::move(media));

    UiListContainer* containers_list = nullptr; auto containers = page_scroll(containers_list); auto* container_section = add_section(*containers_list,"engine.ui_test.pages.containers","engine.ui_test.sections.containers");
    auto grid = std::make_unique<UiGridContainer>(elysia::core::Rect{ 0,0,560,170 }); grid->set_column_count(3); grid->set_cell_spacing(elysia::core::Vector2(8,8)); for (int index = 0; index < 6; ++index) grid->add_child(button(index % 2 ? "engine.common.cancel" : "engine.common.confirm")); container_section->add_back(std::move(grid));
    auto chrome = std::make_unique<UiChromeContainer>(elysia::core::Rect{ 0,0,620,170 }); chrome->add_left_action(button("engine.common.back")); chrome->add_title_child(std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,240,30 },0,ui_text_key("engine.ui_test.containers.chrome"))); chrome->add_right_action(button("engine.common.close")); auto chrome_body = std::make_unique<UiPanel>(elysia::core::Rect{ 0,0,580,100 }); chrome_body->add_child(button("engine.ui_test.actions.replay")); chrome->set_body(std::move(chrome_body)); container_section->add_back(std::move(chrome));
    auto nested_tabs = std::make_unique<UiTabContainer>(elysia::core::Rect{ 0,0,620,200 }); auto nested_page = std::make_unique<UiPanel>(elysia::core::Rect{ 0,0,580,140 }); nested_page->add_child(button("engine.common.confirm")); (void)nested_tabs->add_tab(ui_text_key("engine.ui_test.containers.nested_tab"),std::move(nested_page)); container_section->add_back(std::move(nested_tabs));
    auto hidden_scroll = std::make_unique<UiScrollContainer>(elysia::core::Rect{ 0,0,500,110 }); hidden_scroll->set_scroll_axis(UiScrollAxis::Horizontal); hidden_scroll->set_scrollbar_visibility(UiScrollBarVisibility::Hidden); auto hidden_content = std::make_unique<UiListContainer>(elysia::core::Rect{ 0,0,900,90 }); hidden_content->set_direction(UiListDirection::Horizontal); for (int index = 0; index < 5; ++index) hidden_content->add_back(button("engine.common.close")); hidden_scroll->set_content(std::move(hidden_content)); container_section->add_back(std::move(hidden_scroll));
    (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.containers"),std::move(containers));

    UiListContainer* overlays_list = nullptr; auto overlays = page_scroll(overlays_list); auto* overlay_section = add_section(*overlays_list,"engine.ui_test.pages.overlays","engine.ui_test.sections.overlays");
    auto overlay = std::make_unique<UiPanel>(elysia::core::Rect{ 0,0,320,140 }); UiPanel* overlay_ptr = overlay.get(); overlay->add_child(button("engine.common.close")); _root_window->add_child(std::move(overlay),at(760,76,320,140)); (void)_root_window->register_overlay(*overlay_ptr,UiOverlayOptions{ .open=false,.modal=false,.close_on_cancel=true,.close_on_outside_click=true,.placement=UiOverlayPlacement::Center,.transition=UiOverlayTransition::Slide,.fallback_size=elysia::core::Vector2(320,140),.order=900 });
    auto open_overlay = button("engine.ui_test.overlays.open_overlay"); open_overlay->set_on_click([this,overlay_ptr]() { _root_window->open_overlay(*overlay_ptr); set_status_key("engine.ui_test.status.interaction"); }); overlay_section->add_back(std::move(open_overlay));
    auto dialog = std::make_unique<UiDialog>(elysia::core::Rect{ 0,0,480,300 }); UiDialog* dialog_ptr = dialog.get(); dialog->set_title_content(ui_text_key("engine.ui_test.dialog.title")); dialog->set_body_content(ui_text_key("engine.ui_test.dialog.body")); dialog->set_action_content(ui_text_key("engine.common.close")); _root_window->add_child(std::move(dialog),at(760,230,480,300)); (void)dialog_ptr->register_with_window(*_root_window); auto open_dialog = button("engine.ui_test.overlays.open_dialog"); open_dialog->set_on_click([dialog_ptr]() { dialog_ptr->open(); }); overlay_section->add_back(std::move(open_dialog));
    auto confirm = std::make_unique<UiConfirmationDialog>(elysia::core::Rect{ 0,0,440,220 }); UiConfirmationDialog* confirm_ptr = confirm.get(); confirm->set_config(UiConfirmationDialogConfig{ .title=ui_text_key("engine.ui_test.confirm.title"),.message=ui_text_key("engine.ui_test.confirm.message"),.confirm=ui_text_key("engine.common.confirm"),.cancel=ui_text_key("engine.common.cancel"),.close=ui_text_key("engine.common.close"),.confirm_visual_role=UiButtonVisualRole::Danger }); _root_window->add_child(std::move(confirm),at(760,230,440,220)); (void)confirm_ptr->register_with_window(*_root_window); auto open_confirm = button("engine.ui_test.overlays.open_confirm"); open_confirm->set_on_click([confirm_ptr]() { confirm_ptr->open(); }); overlay_section->add_back(std::move(open_confirm));
    auto dropdown = std::make_unique<UiDropdown>(elysia::core::Rect{ 0,0,320,42 }); dropdown->set_options({ UiDropdownOption{ui_text_key("engine.common.confirm")},UiDropdownOption{ui_text_key("engine.common.cancel")},UiDropdownOption{ui_text_key("engine.common.close")} }); dropdown->register_with_window(*_root_window); overlay_section->add_back(std::move(dropdown)); auto tooltip_trigger = button("engine.ui_test.overlays.tooltip"); UiButton* tooltip_trigger_ptr = tooltip_trigger.get(); overlay_section->add_back(std::move(tooltip_trigger)); auto* tooltip = _root_window->create_child<UiTooltip>(0); auto tooltip_content = std::make_unique<UiTextBlock>(elysia::core::Rect{ 0,0,280,72 }); tooltip_content->set_text_content(ui_text_key("engine.ui_test.overlays.tooltip_text")); tooltip_content->set_padding(8); tooltip->bind_trigger(*tooltip_trigger_ptr); tooltip->set_content(std::move(tooltip_content)); tooltip->register_with_window(*_root_window);
    (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.overlays"),std::move(overlays));

    UiListContainer* theme_list = nullptr; auto themes_page = page_scroll(theme_list); auto* theme_section = add_section(*theme_list,"engine.ui_test.pages.appearance","engine.ui_test.sections.appearance");
    static constexpr std::array<const char*,7> theme_names{ "Blue Glass Moon","Elysia Light","Elysia Dark","EVA-00","EVA-01","EVA-02","Quiet Slate" };
    static constexpr std::array<UiBuiltinTheme,7> themes{ UiBuiltinTheme::BlueGlassMoon,UiBuiltinTheme::ElysiaLight,UiBuiltinTheme::ElysiaDark,UiBuiltinTheme::EvangelionUnit00,UiBuiltinTheme::EvangelionUnit01,UiBuiltinTheme::EvangelionUnit02,UiBuiltinTheme::QuietSlate };
    for (std::size_t index = 0; index < themes.size(); ++index) { auto theme_button = std::make_unique<UiButton>(elysia::core::Rect{ 0,0,240,40 },UiButtonConfig{ .content = ui_raw_text(theme_names[index]) },0); _theme_buttons[index] = theme_button.get(); theme_button->set_on_click([this,theme=themes[index]]() { set_active_theme(theme); }); theme_section->add_back(std::move(theme_button)); }
    auto theme_note = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,680,32 },0,ui_text_key("engine.ui_test.appearance.note")); theme_note->set_visual_role(UiLabelVisualRole::Muted); theme_section->add_back(std::move(theme_note)); (void)tabs->add_tab(ui_text_key("engine.ui_test.pages.appearance"),std::move(themes_page));

    UiListContainer* typography_list = nullptr; auto typography_page = page_scroll(typography_list); auto* typography_section = add_section(*typography_list,"engine.ui_test.typography.title","engine.ui_test.typography.description");
    struct TypographySample { const char* key; UiTypographyRole role; float height; };
    static constexpr std::array<TypographySample,7> typography_samples{{ { "engine.ui_test.typography.sample_10",UiTypographyRole::Caption,26.0f },{ "engine.ui_test.typography.sample_20",UiTypographyRole::ButtonCompact,36.0f },{ "engine.ui_test.typography.sample_30",UiTypographyRole::Label,46.0f },{ "engine.ui_test.typography.sample_40",UiTypographyRole::Heading,56.0f },{ "engine.ui_test.typography.sample_50",UiTypographyRole::Subtitle,66.0f },{ "engine.ui_test.typography.sample_60",UiTypographyRole::DialogTitle,76.0f },{ "engine.ui_test.typography.sample_70",UiTypographyRole::Title,86.0f } }};
    for (const auto& sample : typography_samples) { auto label = std::make_unique<UiLabel>(elysia::core::Rect{ 0,0,780,sample.height },0,ui_text_key(sample.key)); label->set_typography_role(sample.role); label->set_text_fit_mode(UiLabelTextFitMode::None); typography_section->add_back(std::move(label)); }
    (void)tabs->add_tab(ui_text_key("engine.ui_test.typography.tab"),std::move(typography_page));

    _root_window->add_child(std::move(workbench),at(16,48,1080,530)); _root_window->register_focus_scope(*tabs); _root_window->focus_first_available_scope(); sync_theme_switch_button_roles();
}

void UiTestScene::clear_ui()
{
    _theme_registrations.clear(); _theme_buttons.fill(nullptr); _status_label = nullptr;
    if (_root_window) { _root_window->destroy(); _root_window = nullptr; }
}

void UiTestScene::return_to_caller()
{
    if (is_valid_return_route(_return_route)) request_scene_switch(_return_route);
}
}
