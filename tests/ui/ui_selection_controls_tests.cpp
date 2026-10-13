#define SDL_MAIN_HANDLED

#include "engine/ui/composites/ui_labeled_radio_button.h"
#include "engine/ui/composites/ui_labeled_checkbox.h"
#include "engine/ui/composites/ui_tab_bar.h"
#include "engine/ui/composites/ui_dropdown.h"
#include "engine/ui/containers/ui_button_group.h"
#include "engine/ui/containers/ui_radio_group.h"
#include "engine/ui/containers/ui_scroll_container.h"
#include "engine/ui/containers/ui_panel.h"
#include "engine/ui/style/ui_theme_manager.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/widgets/ui_radio_button.h"
#include "engine/ui/widgets/ui_slider.h"
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

elysia::ui::UiScrollContainer* settings_scroll(
    elysia::ui::SettingsPanel& panel)
{
    return dynamic_cast<elysia::ui::UiScrollContainer*>(panel.child_at(1));
}

elysia::ui::UiListContainer* settings_content(
    elysia::ui::SettingsPanel& panel)
{
    auto* scroll = settings_scroll(panel);
    return scroll
        ? dynamic_cast<elysia::ui::UiListContainer*>(
            const_cast<elysia::ui::UiElement*>(scroll->content()))
        : nullptr;
}

std::string content_key_at(
    const elysia::ui::UiListContainer& content,
    std::size_t index)
{
    const elysia::ui::UiElement* element = content.child_at(index);
    if (const auto* label = dynamic_cast<const elysia::ui::UiLabel*>(element))
        return label->text_content().value;

    const auto* row = dynamic_cast<const elysia::ui::UiListContainer*>(element);
    if (!row || row->direction() != elysia::ui::UiListDirection::Horizontal)
        return {};
    for (std::size_t child_index = 0;
         child_index < row->child_count(); ++child_index)
    {
        const auto* label = dynamic_cast<const elysia::ui::UiLabel*>(
            row->child_at(child_index));
        if (label && !label->text_content().empty())
            return label->text_content().value;
    }
    return {};
}

void collect_content_keys(const elysia::ui::UiChildHost& content,
    std::vector<std::string>& keys)
{
    for (std::size_t index = 0; index < content.child_count(); ++index)
    {
        const auto* element = content.child_at(index);
        if (const auto* list = dynamic_cast<const elysia::ui::UiListContainer*>(&content))
        {
            const auto key = content_key_at(*list,index);
            if (!key.empty())
            {
                keys.push_back(key);
                continue;
            }
        }
        if (const auto* host = dynamic_cast<const elysia::ui::UiChildHost*>(element))
            collect_content_keys(*host,keys);
    }
}

void require_content_keys(
    const elysia::ui::UiListContainer& content,
    const std::vector<std::string>& expected)
{
    std::vector<std::string> keys;
    collect_content_keys(content,keys);
    require(keys == expected,
        "settings sections must contain only visible fields in their built-in order");
}

elysia::ui::UiListContainer* find_field_row(
    elysia::ui::UiChildHost& content,
    std::string_view key)
{
    for (std::size_t index = 0; index < content.child_count(); ++index)
    {
        auto* row = dynamic_cast<elysia::ui::UiListContainer*>(
            content.child_at(index));
        if (row && row->child_count() > 0)
        {
            auto* label = dynamic_cast<elysia::ui::UiLabel*>(row->child_at(0));
            if (label && label->text_content().value == key)
                return row;
        }
        if (auto* host = dynamic_cast<elysia::ui::UiChildHost*>(content.child_at(index)))
            if (auto* found = find_field_row(*host,key))
                return found;
    }
    return nullptr;
}

template<class Control>
Control* field_control(
    elysia::ui::UiListContainer& content,
    std::string_view key)
{
    auto* row = find_field_row(content,key);
    return row && row->child_count() > 1
        ? dynamic_cast<Control*>(row->child_at(1))
        : nullptr;
}

elysia::ui::SettingsPanelVisibility only_visibility(
    std::string_view field)
{
    elysia::ui::SettingsPanelVisibility visibility{
        .window_mode = false,
        .target_fps = false,
        .vsync = false,
        .master_volume = false,
        .music_volume = false,
        .sound_volume = false,
        .language = false
    };
    if (field == "window_mode") visibility.window_mode = true;
    if (field == "target_fps") visibility.target_fps = true;
    if (field == "vsync") visibility.vsync = true;
    if (field == "master_volume") visibility.master_volume = true;
    if (field == "music_volume") visibility.music_volume = true;
    if (field == "sound_volume") visibility.sound_volume = true;
    if (field == "language") visibility.language = true;
    return visibility;
}

void activate(elysia::ui::UiButton& button)
{
    button.set_focused(true);
    (void)button.on_ui_input_event({
        .action = elysia::ui::UiAction::Confirm,
        .type = elysia::ui::UiInputEventType::ActionPressed
    });
    (void)button.on_ui_input_event({
        .action = elysia::ui::UiAction::Confirm,
        .type = elysia::ui::UiInputEventType::ActionReleased
    });
}

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

    elysia::ui::UiTabBar tabs(elysia::core::Rect{ 0,0,240,40 });
    elysia::ui::UiButton* tab = tabs.add_tab(elysia::ui::ui_raw_text("tab"));
    require(tab != nullptr,"tab should be created");
    tab->set_style_overrides(custom);
    tabs.set_selected_index(0);
    require(!tab->style().chrome.draw_background
            && !tab->style().chrome.draw_border,
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
    activate(*second_raw);
    require(callback_count == 1,"selection must preserve the original button callback");
}

void test_settings_panel_single_page_draft_and_actions()
{
    using namespace elysia;
    require(ui::make_settings_window_size_options(
            ui::SettingsWindowSize{ 1600,900 },
            ui::SettingsWindowSize{ 2560,1440 })
            == std::vector<ui::SettingsWindowSize>{
                { 960,540 },{ 1280,720 },{ 1600,900 }
            },
        "display bounds must filter presets and an oversized saved value");
    require(ui::make_settings_window_size_options(
            ui::SettingsWindowSize{ 800,600 }, ui::SettingsWindowSize{ 1920,1080 })
            == std::vector<ui::SettingsWindowSize>{{800,600}},
        "small displays must retain a fitting window choice");
    require(ui::make_settings_window_size_options(
            ui::SettingsWindowSize{ 1600,900 }, ui::SettingsWindowSize{ 1366,768 })
            == std::vector<ui::SettingsWindowSize>{{960,540},{1280,720},{1366,768},{1600,900}},
        "custom sizes that fit the display must remain selectable");
    require(ui::make_settings_target_fps_options(60.0)
            == std::vector<double>{ 30.0,60.0,120.0,240.0 },
        "built-in FPS presets must omit 144 FPS");
    require(ui::make_settings_target_fps_options(144.0)
            == std::vector<double>{ 30.0,60.0,120.0,144.0,240.0 },
        "a valid custom current FPS must remain selectable");

    {
        ui::SettingsPanel filtered_panel;
        ui::SettingsPanelDraft oversized;
        oversized.window_size = {3840,2160};
        oversized.target_fps = 300.0;
        filtered_panel.set_draft(oversized);
        filtered_panel.set_options({
            .window_sizes = {{1280,720},{1920,1080},{3840,2160}},
            .target_fps_values = {30,60,120,240},
            .usable_window_size = ui::SettingsWindowSize{1600,900}
        });
        require(filtered_panel.options().window_sizes
                == std::vector<ui::SettingsWindowSize>{{1280,720}},
            "panel normalization must not reinsert oversized saved resolutions");
        require(filtered_panel.draft().window_size == ui::SettingsWindowSize{1280,720},
            "the visible draft must select a fitting resolution");
        filtered_panel.set_draft(oversized);
        require(filtered_panel.draft().window_size == ui::SettingsWindowSize{1280,720}
                && filtered_panel.options().window_sizes.size() == 1,
            "later draft refreshes must still respect display bounds");
        require(filtered_panel.draft().target_fps == 300.0
                && filtered_panel.options().target_fps_values.back() == 300.0,
            "display constraints must not cap the target FPS");

        ui::SettingsPanelVisibility hidden;
        hidden.window_mode = false;
        ui::SettingsPanel hidden_panel(core::Rect{0,0,700,680}, hidden);
        hidden_panel.set_draft(oversized);
        hidden_panel.set_options(filtered_panel.options());
        require(hidden_panel.draft() == oversized,
            "display filtering must preserve settings excluded by the host");
    }

    ui::SettingsPanel panel(core::Rect{ 0,0,700,680 });
    require(panel.visibility() == ui::SettingsPanelVisibility{},
        "the compatibility constructor must enable every built-in field");
    auto* title = dynamic_cast<ui::UiLabel*>(panel.child_at(0));
    auto* content = settings_content(panel);
    require(title && content && settings_scroll(panel),
        "settings panel must expose one title and one scrolling content list");
    require(title->text_content().value == "engine.settings.title",
        "settings title must retain its localization key");
    require_content_keys(*content,{
        "engine.settings.sections.display",
        "engine.settings.fields.window_mode",
        "engine.settings.fields.window_size",
        "engine.settings.fields.target_fps",
        "engine.settings.fields.vsync",
        "engine.settings.hints.vsync_restart",
        "engine.settings.sections.audio",
        "engine.settings.fields.master_volume",
        "engine.settings.fields.music_volume",
        "engine.settings.fields.sound_volume",
        "engine.settings.sections.general",
        "engine.settings.fields.language"
    });

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
        .window_sizes = { { 1920,1080 },{ 1280,720 },{ 1920,1080 },{ 0,0 } },
        .target_fps_values = { 240.0,60.0,60.0,0.0 },
        .languages = { "en","zh-Hans","en","" }
    });
    require(panel.draft() == draft,
        "programmatic option refresh must not edit the complete draft");
    require(panel.options().window_sizes == std::vector<ui::SettingsWindowSize>{
            { 1280,720 },{ 1366,768 },{ 1920,1080 } },
        "window options must normalize while retaining the active value");
    require(panel.options().target_fps_values
            == std::vector<double>{ 60.0,144.0,240.0 },
        "FPS options must normalize while retaining the active value");

    auto* window_dropdown = field_control<ui::UiDropdown>(
        *content,"engine.settings.fields.window_mode");
    auto* size_dropdown = field_control<ui::UiDropdown>(
        *content,"engine.settings.fields.window_size");
    auto* fps_dropdown = field_control<ui::UiDropdown>(
        *content,"engine.settings.fields.target_fps");
    auto* vsync = field_control<ui::UiLabeledCheckbox>(
        *content,"engine.settings.fields.vsync");
    auto* language_dropdown = field_control<ui::UiDropdown>(
        *content,"engine.settings.fields.language");
    auto* master_volume = field_control<ui::UiSlider>(
        *content,"engine.settings.fields.master_volume");
    require(window_dropdown && size_dropdown && fps_dropdown && vsync && language_dropdown
            && master_volume,
        "all-visible settings must construct every interactive field");
    require(window_dropdown->options().size() == 2
            && size_dropdown->options().size() == 3
            && fps_dropdown->options().size() == 3
            && language_dropdown->options().size() == 2
            && !vsync->is_checked(),
        "control state must mirror normalized options and the complete draft");
    require(master_volume->value_target_height() == 20.0f,
        "settings volume sliders must use a compact percentage readout");
    require(window_dropdown->set_selected_index(1)
            && panel.draft().window_mode == ui::SettingsWindowMode::BorderlessFullscreen
            && panel.draft().window_size == draft.window_size
            && !size_dropdown->is_enabled(),
        "fullscreen must disable window size while retaining the windowed draft");
    require(window_dropdown->set_selected_index(0)
            && panel.draft().window_mode == ui::SettingsWindowMode::Windowed
            && size_dropdown->is_enabled()
            && size_dropdown->selected_index() == 1,
        "returning to windowed mode must restore the previous size selection");
    require(size_dropdown->set_selected_index(2)
            && panel.draft().window_size == ui::SettingsWindowSize{1920,1080}
            && panel.draft().window_mode == ui::SettingsWindowMode::Windowed,
        "window size must be editable independently of the mode");

    ui::UiWindow popup_window(core::Rect{ 0,0,800,600 });
    panel.register_with_window(popup_window);
    fps_dropdown->open();
    require(fps_dropdown->is_open(),
        "visible FPS dropdown must participate in window popup registration");
    fps_dropdown->close();
    size_dropdown->open();
    require(size_dropdown->is_open(),
        "the separate window-size dropdown must be registered with the window");
    size_dropdown->close();
    panel.unregister_from_window();
    fps_dropdown->open();
    require(!fps_dropdown->is_open(),
        "window unregistration must detach visible dropdown popups");

    require(fps_dropdown->set_selected_index(2)
            && panel.draft().target_fps == 240.0,
        "visible FPS edits must update only the local draft");
    vsync->toggle();
    require(panel.draft().vsync,
        "visible VSync edits must update only the local draft");

    auto* status = dynamic_cast<ui::UiLabel*>(panel.child_at(2));
    auto* actions = dynamic_cast<ui::UiListContainer*>(panel.child_at(3));
    auto* save = actions
        ? dynamic_cast<ui::UiButton*>(actions->child_at(1)) : nullptr;
    auto* back = actions
        ? dynamic_cast<ui::UiButton*>(actions->child_at(0)) : nullptr;
    require(status && save && back,
        "status and Save/Back must remain fixed outside the scroll viewport");

    int save_count = 0;
    int back_count = 0;
    ui::SettingsPanelDraft saved_draft;
    panel.set_on_save([&](const ui::SettingsPanelDraft& value)
    {
        ++save_count;
        saved_draft = value;
    });
    panel.set_on_back([&]() { ++back_count; });
    panel.set_status_content(ui::ui_text_key("engine.settings.status.saved"),false);
    require(status->is_visible(),"localized status content must become visible");
    activate(*save);
    require(save_count == 1 && saved_draft == panel.draft(),
        "Save must emit the complete local draft exactly once");
    activate(*back);
    require(back_count == 1,"Back must invoke its callback exactly once");

    for (const auto device : {
            input::InputDevice::Keyboard,
            input::InputDevice::Gamepad })
    {
        panel.reset_navigation_state();
        panel.set_scope_focused(true);
        require(panel.focus_first_available(),
            "settings focus must begin at the first visible field");
        std::vector<ui::UiControl*> expected_targets{
            window_dropdown,size_dropdown,fps_dropdown,vsync,master_volume,
            field_control<ui::UiSlider>(*content,"engine.settings.fields.music_volume"),
            field_control<ui::UiSlider>(*content,"engine.settings.fields.sound_volume"),
            language_dropdown,back
        };
        require(panel.focused_target() == expected_targets.front(),
            "settings navigation must start at the first field");
        for (std::size_t index = 1; index < expected_targets.size(); ++index)
        {
            (void)panel.on_ui_input_event({
                .action = ui::UiAction::NavigateDown,
                .type = ui::UiInputEventType::ActionPressed,
                .device = device
            });
            require(panel.focused_target() == expected_targets[index],
                "Down must visit every settings field without skipping at hint rows");
        }
        for (std::size_t index = expected_targets.size() - 1; index > 0; --index)
        {
            (void)panel.on_ui_input_event({
                .action = ui::UiAction::NavigateUp,
                .type = ui::UiInputEventType::ActionPressed,
                .device = device
            });
            require(panel.focused_target() == expected_targets[index - 1],
                "Up must visit every settings field without stopping at hint rows");
        }
    }
}

void test_settings_panel_visibility_and_single_section_headings()
{
    using namespace elysia;
    struct VisibilityCase
    {
        std::string_view field;
        std::vector<std::string> keys;
    };
    const std::vector<VisibilityCase> cases{
        { "window_mode",{ "engine.settings.fields.window_mode",
            "engine.settings.fields.window_size" } },
        { "target_fps",{ "engine.settings.fields.target_fps" } },
        { "vsync",{
            "engine.settings.fields.vsync",
            "engine.settings.hints.vsync_restart" } },
        { "master_volume",{ "engine.settings.fields.master_volume" } },
        { "music_volume",{ "engine.settings.fields.music_volume" } },
        { "sound_volume",{ "engine.settings.fields.sound_volume" } },
        { "language",{ "engine.settings.fields.language" } }
    };
    for (const VisibilityCase& item : cases)
    {
        ui::SettingsPanel panel(
            core::Rect{ 0,0,700,680 },only_visibility(item.field));
        auto* content = settings_content(panel);
        require(content != nullptr,
            "every visibility profile must retain the shared scroll content");
        require_content_keys(*content,item.keys);
    }

    ui::SettingsPanelVisibility mixed = only_visibility("target_fps");
    mixed.language = true;
    ui::SettingsPanel mixed_panel(core::Rect{ 0,0,700,680 },mixed);
    require_content_keys(*settings_content(mixed_panel),{
        "engine.settings.sections.display",
        "engine.settings.fields.target_fps",
        "engine.settings.sections.general",
        "engine.settings.fields.language"
    });

    ui::SettingsPanelVisibility none{
        .window_mode = false,
        .target_fps = false,
        .vsync = false,
        .master_volume = false,
        .music_volume = false,
        .sound_volume = false,
        .language = false
    };
    ui::SettingsPanel empty_panel(core::Rect{ 0,0,700,680 },none);
    auto* empty_content = settings_content(empty_panel);
    auto* actions = dynamic_cast<ui::UiListContainer*>(empty_panel.child_at(3));
    auto* back = actions
        ? dynamic_cast<ui::UiButton*>(actions->child_at(0)) : nullptr;
    require(empty_content && empty_content->child_count() == 0 && back,
        "an all-hidden profile must keep an empty scroll and fixed actions");
    empty_panel.set_scope_focused(true);
    require(empty_panel.focus_first_available()
            && empty_panel.focused_target() == back,
        "an all-hidden profile must focus Back first");

    ui::UiWindow window(core::Rect{ 0,0,800,600 });
    empty_panel.register_with_window(window);
    empty_panel.unregister_from_window();
}

void test_compact_single_page_scroll_keeps_chrome_fixed()
{
    using namespace elysia;
    ui::SettingsPanel panel(core::Rect{ 0,0,700,360 });
    auto* scroll = settings_scroll(panel);
    panel.update_layout_if_dirty();
    require(scroll && scroll->max_scroll_offset().y > 0.0f,
        "a compact settings panel must scroll its one shared content page");
    const auto title_position = panel.child_at(0)->position();
    const auto status_position = panel.child_at(2)->position();
    const auto actions_position = panel.child_at(3)->position();
    scroll->scroll_to_bottom();
    panel.update_layout_if_dirty();
    require(panel.child_at(0)->position() == title_position
            && panel.child_at(2)->position() == status_position
            && panel.child_at(3)->position() == actions_position,
        "single-page scrolling must not move title, status, or actions");
    panel.reset_navigation_state();
    require(scroll->scroll_offset().y == 0.0f,
        "fresh settings navigation must reset the shared scroll to the top");
}

void test_settings_sections_follow_theme_and_fit_narrow_panel()
{
    using namespace elysia;
    ui::SettingsPanel panel(core::Rect{0,0,320,360});
    ui::UiThemeManager themes;
    auto registration = themes.register_root(panel);
    auto* content = settings_content(panel);
    require(content && content->child_count() == 3,
        "all-visible settings must have three bounded sections");
    for (const auto theme : {ui::UiBuiltinTheme::BlueGlassMoon,
            ui::UiBuiltinTheme::ElysiaLight,ui::UiBuiltinTheme::ElysiaDark})
    {
        themes.set_theme(theme);
        panel.update_layout_if_dirty();
        for (std::size_t i = 0; i < content->child_count(); ++i)
        {
            auto* section = dynamic_cast<ui::UiListContainer*>(content->child_at(i));
            auto* surface = section ? dynamic_cast<ui::UiPanel*>(section->child_at(1)) : nullptr;
            require(surface && surface->style().background == themes.current_theme().panel().background
                    && !surface->style_overrides().background
                    && !surface->style_overrides().border,
                "settings section colors must follow the existing theme without overrides");
            auto* fields = dynamic_cast<ui::UiListContainer*>(surface->child_at(0));
            require(fields && fields->size() == surface->size(),
                "section fields must fill their surface for layout and pointer navigation");
            for (std::size_t j = 0; j < fields->child_count(); ++j)
            {
                const auto* row = fields->child_at(j);
                require(row->screen_rect().x() >= surface->screen_rect().x()
                        && row->screen_rect().right() <= surface->screen_rect().right(),
                    "narrow settings rows must stay within their section surface");
            }
        }
        const auto* actions = panel.child_at(3);
        require(actions->screen_rect().right() <= panel.screen_rect().right()
                && actions->screen_rect().bottom() <= panel.screen_rect().bottom(),
            "fixed actions must fit inside a narrow settings panel");
    }
}
}

int main()
{
    test_group_repairs_selection_without_group_callback();
    test_radio_render_defers_callback();
    test_group_preserves_button_override();
    test_button_group_preserves_button_callback_after_selection();
    test_settings_panel_single_page_draft_and_actions();
    test_settings_panel_visibility_and_single_section_headings();
    test_compact_single_page_scroll_keeps_chrome_fixed();
    test_settings_sections_follow_theme_and_fit_narrow_panel();
    std::cout << "ui selection control tests passed\n";
    return EXIT_SUCCESS;
}
