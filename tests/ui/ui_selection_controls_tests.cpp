#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_labeled_radio_button.h"
#include "engine/ui/composites/ui_labeled_checkbox.h"
#include "engine/ui/composites/ui_tab_container.h"
#include "engine/ui/composites/ui_tab_bar.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/containers/ui_button_group.h"
#include "engine/ui/containers/ui_radio_group.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/containers/ui_tab_view.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_radio_button.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/window/ui_window.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using elysia::tests::require;

void test_group_repairs_selection_without_group_callback()
{
    elysia::ui::UiButtonGroup group(elysia::core::Rect{ 0,0,240,40 });
    group.add_button(std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 }));
    group.add_button(std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 }));
    require(group.selected_index() == 0,"button group should auto-select first");
    group.child_at(0)->destroy();

    std::vector<elysia::core::UiRenderCommand> commands;
    group.submit_ui_render_commands(commands);
    group.update(0.0);
    require(group.selected_index() == 0,"selection should repair after the selected child is removed");
}

void test_radio_render_defers_callback()
{
    elysia::ui::UiRadioGroup group(elysia::core::Rect{ 0,0,240,40 });
    group.add_back(std::make_unique<elysia::ui::UiLabeledRadioButton>(elysia::core::Rect{ 0,0,100,40 }));
    group.add_back(std::make_unique<elysia::ui::UiRadioButton>(elysia::core::Rect{ 0,0,100,40 }));
    group.update(0.0);
    int callback_count = 0;
    group.set_on_selection_changed([&](std::optional<std::size_t>) { ++callback_count; });
    group.child_at(0)->destroy();

    std::vector<elysia::core::UiRenderCommand> commands;
    group.submit_ui_render_commands(commands);
    require(callback_count == 0,"render must not notify radio group callback");
    group.update(0.0);
    require(callback_count == 1,"next update should deliver deferred radio callback once");
}

void test_group_preserves_button_override()
{
    elysia::ui::UiButtonStyleOverrides custom{};
    custom.chrome.draw_background = false;
    custom.chrome.draw_border = false;
    auto button = std::make_unique<elysia::ui::UiButton>(elysia::core::Rect{ 0,0,100,40 });
    button->set_style_overrides(custom);

    elysia::ui::UiButtonGroup group(elysia::core::Rect{ 0,0,120,40 });
    elysia::ui::UiButton* raw = group.add_button(std::move(button));
    require(raw && raw->has_style_overrides(),"group must retain explicit button style overrides");
    require(!raw->style().chrome.draw_background && !raw->style().chrome.draw_border,
        "selection role must not overwrite structural button style");

    elysia::ui::UiTabBar tabs(elysia::core::Rect{ 0,0,240,40 });
    elysia::ui::UiButton* tab = tabs.add_tab(elysia::ui::ui_raw_text("tab"));
    require(tab != nullptr,"tab should be created");
    tab->set_style_overrides(custom);
    tabs.set_selected_index(0);
    require(!tab->style().chrome.draw_background && !tab->style().chrome.draw_border,
        "tab selection must retain explicit button style override");
}

void test_button_group_preserves_button_callback_after_selection()
{
    using namespace elysia;
    ui::UiButtonGroup group(core::Rect{ 0,0,240,40 });
    auto first = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    auto second = std::make_unique<ui::UiButton>(core::Rect{ 0,0,100,40 });
    int callback_count = 0;
    second->set_on_click([&]()
    {
        ++callback_count;
        require(group.selected_index() == 1,"button callback must observe the new group selection");
    });
    group.add_button(std::move(first));
    ui::UiButton* second_raw = group.add_button(std::move(second));
    require(second_raw != nullptr,"group should adopt second button");
    second_raw->set_focused(true);
    (void)second_raw->on_ui_input_event({ .action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionPressed });
    (void)second_raw->on_ui_input_event({ .action=ui::UiAction::Confirm,.type=ui::UiInputEventType::ActionReleased });
    require(callback_count == 1,"group decoration must preserve the original button callback");
}

void test_settings_panel_keeps_draft_local_and_normalizes_options()
{
    using namespace elysia;
    require(
        ui::make_settings_window_size_options(
            ui::SettingsWindowSize{ 1600,900 },
            ui::SettingsWindowSize{ 2560,1440 })
            == std::vector<ui::SettingsWindowSize>{
                { 960,540 },
                { 1280,720 },
                { 1600,900 },
                { 2560,1440 }
            },
        "usable display bounds must filter presets while retaining the current value");
    require(
        ui::make_settings_window_size_options(
            std::nullopt,
            ui::SettingsWindowSize{ 1280,720 }).size() == 6,
        "failed display bounds queries must retain every preset without duplicates");
    require(
        ui::make_settings_target_fps_options(60.0)
            == std::vector<double>{ 30.0,60.0,120.0,240.0 },
        "built-in FPS presets must omit 144 FPS");
    require(
        ui::make_settings_target_fps_options(144.0)
            == std::vector<double>{ 30.0,60.0,120.0,144.0,240.0 },
        "a valid custom current FPS must remain selectable");

    ui::SettingsPanel panel(core::Rect{ 0,0,700,680 });
    const auto label_at = [&panel](std::size_t index)
    {
        return dynamic_cast<ui::UiLabel*>(panel.child_at(index));
    };
    const auto require_text_key = [](const ui::UiTextContent& content,
                                     std::string_view key)
    {
        require(content.kind == ui::UiTextContentKind::TextKey
                && content.value == key,
            "settings panel fixed copy must use the expected localization key");
    };
    require_text_key(label_at(0)->text_content(),"engine.settings.title");

    auto* tabs = dynamic_cast<ui::UiTabContainer*>(panel.child_at(1));
    require(tabs && tabs->tab_count() == 3 && tabs->page_count() == 3,
        "settings panel must expose exactly Display, Audio, and General tabs");
    auto* tab_bar = dynamic_cast<ui::UiTabBar*>(tabs->child_at(0));
    auto* tab_view = dynamic_cast<ui::UiTabView*>(tabs->child_at(1));
    require(tab_bar && tab_view,"settings tabs must own a tab bar and page view");
    std::vector<core::UiRenderCommand> settings_commands;
    panel.submit_ui_render_commands(settings_commands);
    require(tab_bar->screen_rect().height() > 0.0f
            && tab_view->screen_rect().top() >= tab_bar->screen_rect().bottom(),
        "settings tab bar must occupy visible space above the page viewport");
    const auto tab_button_at = [tab_bar](std::size_t index)
    {
        return dynamic_cast<ui::UiButton*>(tab_bar->child_at(index));
    };
    require(tab_button_at(0)->screen_rect().height() > 0.0f
            && tab_button_at(0)->screen_rect().top()
                >= tab_bar->screen_rect().top(),
        "settings tab buttons must be laid out inside the visible tab bar");
    const core::Rect initial_tab_button_rect =
        tab_button_at(0)->screen_rect();
    panel.set_position({ 290.0f,20.0f });
    settings_commands.clear();
    panel.submit_ui_render_commands(settings_commands);
    require(tab_button_at(0)->screen_rect().top()
                >= tab_bar->screen_rect().top()
            && tab_button_at(0)->screen_rect().left()
                >= tab_bar->screen_rect().left()
            && tab_button_at(0)->screen_rect().position()
                != initial_tab_button_rect.position(),
        "settings tab buttons must follow a position-only panel relocation");
    require_text_key(
        tab_button_at(0)->text_content(),
        "engine.settings.sections.display");
    require_text_key(
        tab_button_at(1)->text_content(),
        "engine.settings.sections.audio");
    require_text_key(
        tab_button_at(2)->text_content(),
        "engine.settings.sections.general");

    const auto page_list_at = [tab_view](std::size_t index)
    {
        auto* scroll = dynamic_cast<ui::UiScrollContainer*>(
            tab_view->child_at(index));
        return scroll
            ? dynamic_cast<ui::UiListContainer*>(
                const_cast<ui::UiElement*>(scroll->content()))
            : nullptr;
    };
    ui::UiListContainer* display_page = page_list_at(0);
    ui::UiListContainer* audio_page = page_list_at(1);
    ui::UiListContainer* general_page = page_list_at(2);
    require(display_page && audio_page && general_page,
        "every settings tab must own an independent scrollable list");
    require(tab_view->child_at(0)->is_visible()
            && !tab_view->child_at(1)->is_visible()
            && !tab_view->child_at(2)->is_visible(),
        "only the selected settings page may remain visible");
    require(tabs->set_selected_index(1)
            && panel.selected_section() == ui::SettingsPanelSection::Audio
            && !tab_view->child_at(0)->is_visible()
            && tab_view->child_at(1)->is_visible(),
        "selecting a settings tab must activate only its matching page");
    panel.reset_navigation_state();
    require(panel.selected_section() == ui::SettingsPanelSection::Display,
        "a fresh settings visit must return navigation to Display");
    const auto row_label_at = [](ui::UiListContainer& page,std::size_t index)
    {
        auto* row = dynamic_cast<ui::UiListContainer*>(page.child_at(index));
        return row ? dynamic_cast<ui::UiLabel*>(row->child_at(0)) : nullptr;
    };
    require_text_key(
        row_label_at(*display_page,0)->text_content(),
        "engine.settings.fields.window_mode");
    require_text_key(
        row_label_at(*display_page,1)->text_content(),
        "engine.settings.fields.target_fps");
    require_text_key(
        row_label_at(*display_page,2)->text_content(),
        "engine.settings.fields.vsync");
    require_text_key(
        row_label_at(*audio_page,0)->text_content(),
        "engine.settings.fields.master_volume");
    require_text_key(
        row_label_at(*audio_page,1)->text_content(),
        "engine.settings.fields.music_volume");
    require_text_key(
        row_label_at(*audio_page,2)->text_content(),
        "engine.settings.fields.sound_volume");
    require_text_key(
        row_label_at(*general_page,0)->text_content(),
        "engine.settings.fields.language");

    const ui::SettingsPanelDraft draft{
        .window_mode = ui::SettingsWindowMode::Windowed,
        .window_size = { 1366,768 },
        .target_fps = 144.0,
        .vsync = false,
        .master_volume = 80,
        .music_volume = 70,
        .sound_volume = 60,
        .language = "en"
    };
    panel.set_draft(draft);
    panel.set_options({
        .window_sizes = {
            { 1920,1080 },
            { 1280,720 },
            { 1920,1080 },
            { 0,0 }
        },
        .target_fps_values = { 240.0,60.0,60.0,0.0 },
        .languages = { "en","zh-Hans","en","" }
    });

    require(panel.draft() == draft,
        "settings panel option refresh must not apply or replace its local draft");
    require(panel.options().window_sizes.size() == 3
        && panel.options().window_sizes[0] == ui::SettingsWindowSize{ 1280,720 }
        && panel.options().window_sizes[1] == ui::SettingsWindowSize{ 1366,768 }
        && panel.options().window_sizes[2] == ui::SettingsWindowSize{ 1920,1080 },
        "settings panel must normalize window sizes and retain the active value");
    require(panel.options().target_fps_values
            == std::vector<double>{ 60.0,144.0,240.0 },
        "settings panel must normalize FPS values and retain the draft value");
    require(panel.options().languages == std::vector<std::string>{ "en","zh-Hans" },
        "settings panel must deduplicate language identifiers from LocalizationService");

    int save_count = 0;
    int back_count = 0;
    ui::SettingsPanelDraft saved_draft;
    panel.set_on_save([&](const ui::SettingsPanelDraft& value)
    {
        ++save_count;
        saved_draft = value;
    });
    panel.set_on_back([&]() { ++back_count; });
    const auto dropdown_at = [](ui::UiListContainer& page,std::size_t row_index)
    {
        auto* row = dynamic_cast<ui::UiListContainer*>(
            page.child_at(row_index));
        return row
            ? dynamic_cast<ui::UiDropdown*>(row->child_at(1))
            : nullptr;
    };
    ui::UiDropdown* window_dropdown = dropdown_at(*display_page,0);
    require(window_dropdown
        && window_dropdown->options().size()
            == panel.options().window_sizes.size() + 1u,
        "settings panel must expose window sizes and fullscreen in one dropdown");
    require_text_key(
        window_dropdown->options().back().content,
        "engine.settings.window_modes.borderless_fullscreen");
    auto* target_fps_dropdown = dropdown_at(*display_page,1);
    require(target_fps_dropdown
            && target_fps_dropdown->options().size() == 3,
        "settings panel must expose normalized FPS options");
    ui::UiWindow popup_window(core::Rect{ 0,0,800,600 });
    panel.register_with_window(popup_window);
    target_fps_dropdown->open();
    require(target_fps_dropdown->is_open(),
        "window registration must include the FPS dropdown popup");
    target_fps_dropdown->close();
    panel.unregister_from_window();
    target_fps_dropdown->open();
    require(!target_fps_dropdown->is_open(),
        "window unregistration must detach the FPS dropdown popup");
    auto* vsync_row = dynamic_cast<ui::UiListContainer*>(
        display_page->child_at(2));
    auto* vsync_checkbox = vsync_row
        ? dynamic_cast<ui::UiLabeledCheckbox*>(vsync_row->child_at(1))
        : nullptr;
    require(vsync_checkbox && !vsync_checkbox->is_checked(),
        "programmatic draft sync must update the VSync checkbox");
    auto* language_dropdown = dropdown_at(*general_page,0);
    require(language_dropdown && language_dropdown->options().size() == 2,
        "settings panel must expose every normalized language option");
    require_text_key(
        language_dropdown->options()[0].content,
        "engine.settings.languages.en");
    require_text_key(
        language_dropdown->options()[1].content,
        "engine.settings.languages.zh_hans");
    const std::size_t fullscreen_index =
        panel.options().window_sizes.size();
    require(window_dropdown->set_selected_index(fullscreen_index)
        && panel.draft().window_mode
            == ui::SettingsWindowMode::BorderlessFullscreen
        && panel.draft().window_size == draft.window_size,
        "selecting borderless fullscreen must preserve the windowed size draft");
    require(window_dropdown->set_selected_index(0)
        && panel.draft().window_mode == ui::SettingsWindowMode::Windowed
        && panel.draft().window_size
            == panel.options().window_sizes[0],
        "selecting a window size must switch the draft back to Windowed");

    require(target_fps_dropdown->set_selected_index(2)
            && panel.draft().target_fps == 240.0,
        "selecting an FPS option must update only the local draft");
    vsync_checkbox->toggle();
    require(panel.draft().vsync,
        "toggling VSync must update only the local draft");

    auto* actions = dynamic_cast<ui::UiListContainer*>(panel.child_at(3));
    auto* save = actions
        ? dynamic_cast<ui::UiButton*>(actions->child_at(0))
        : nullptr;
    auto* back = actions
        ? dynamic_cast<ui::UiButton*>(actions->child_at(1))
        : nullptr;
    require(save && back,"settings panel must expose Save and Back actions");
    require_text_key(save->text_content(),"engine.settings.actions.save");
    require_text_key(back->text_content(),"engine.settings.actions.back");

    for (const auto device : {
            elysia::input::InputDevice::Keyboard,
            elysia::input::InputDevice::Gamepad })
    {
        panel.reset_navigation_state();
        panel.set_scope_focused(true);
        require(panel.focus_first_available(),
            "settings focus must begin at the selected tab");
        const auto navigate_down = [&panel,device]()
        {
            return panel.on_ui_input_event({
                .action = ui::UiAction::NavigateDown,
                .type = ui::UiInputEventType::ActionPressed,
                .device = device
            });
        };
        bool reached_save = false;
        for (int attempt = 0; attempt < 8 && !reached_save; ++attempt)
        {
            (void)navigate_down();
            reached_save = panel.focused_target() == save;
        }
        require(reached_save,
            "keyboard and gamepad focus must leave the scrolled page for fixed Save/Back actions");
    }

    auto* status = label_at(2);
    panel.set_status_content(
        ui::ui_text_key("engine.settings.status.saved"),false);
    require(status && status->is_visible(),
        "localized settings status must become visible");
    require_text_key(status->text_content(),"engine.settings.status.saved");
    panel.set_status_message("diagnostic detail",true);
    require(status->text_content().kind == ui::UiTextContentKind::RawText
            && status->text_content().value == "diagnostic detail",
        "settings failure diagnostics must remain raw text");
    const auto activate = [](ui::UiButton& button)
    {
        button.set_focused(true);
        (void)button.on_ui_input_event({
            .action = ui::UiAction::Confirm,
            .type = ui::UiInputEventType::ActionPressed
        });
        (void)button.on_ui_input_event({
            .action = ui::UiAction::Confirm,
            .type = ui::UiInputEventType::ActionReleased
        });
    };
    activate(*save);
    require(save_count == 1 && saved_draft == panel.draft(),
        "the Save button must emit the complete local draft exactly once");

    activate(*back);
    require(back_count == 1,
        "the Back button must invoke its navigation callback exactly once");

    ui::SettingsPanel compact_panel(core::Rect{ 0,0,700,360 });
    auto* compact_tabs = dynamic_cast<ui::UiTabContainer*>(
        compact_panel.child_at(1));
    auto* compact_view = compact_tabs
        ? dynamic_cast<ui::UiTabView*>(compact_tabs->child_at(1))
        : nullptr;
    auto* compact_scroll = compact_view
        ? dynamic_cast<ui::UiScrollContainer*>(compact_view->child_at(0))
        : nullptr;
    compact_panel.update_layout_if_dirty();
    require(compact_scroll && compact_scroll->max_scroll_offset().y > 0.0f,
        "a compact settings panel must scroll only its selected page content");
    const auto title_position = compact_panel.child_at(0)->position();
    const auto actions_position = compact_panel.child_at(3)->position();
    compact_scroll->scroll_to_bottom();
    compact_panel.update_layout_if_dirty();
    require(compact_panel.child_at(0)->position() == title_position
            && compact_panel.child_at(3)->position() == actions_position,
        "page scrolling must not move the settings title or actions");
}
}

int main()
{
    test_group_repairs_selection_without_group_callback();
    test_radio_render_defers_callback();
    test_group_preserves_button_override();
    test_button_group_preserves_button_callback_after_selection();
    test_settings_panel_keeps_draft_local_and_normalizes_options();
    std::cout << "ui selection control tests passed\n";
    return EXIT_SUCCESS;
}

