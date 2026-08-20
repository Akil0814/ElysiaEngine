#pragma once

#include "../containers/ui_list_container.h"
#include "../text/ui_text_content.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace elysia::ui
{
class UiButton;
class UiDropdown;
class UiLabeledCheckbox;
class UiLabel;
class UiScrollContainer;
class UiSlider;
class UiTabContainer;
class UiWindow;

enum class SettingsWindowMode
{
    Windowed,
    BorderlessFullscreen
};

enum class SettingsPanelSection
{
    Display,
    Audio,
    General
};

struct SettingsWindowSize
{
    int width = 0;
    int height = 0;

    friend bool operator==(const SettingsWindowSize&,const SettingsWindowSize&) = default;
};

struct SettingsPanelDraft
{
    SettingsWindowMode window_mode = SettingsWindowMode::Windowed;
    SettingsWindowSize window_size{};
    double target_fps = 60.0;
    bool vsync = true;
    int master_volume = 100;
    int music_volume = 100;
    int sound_volume = 100;
    std::string language;

    friend bool operator==(const SettingsPanelDraft&,const SettingsPanelDraft&) = default;
};

struct SettingsPanelOptions
{
    std::vector<SettingsWindowSize> window_sizes;
    std::vector<double> target_fps_values;
    std::vector<std::string> languages;
};

[[nodiscard]] std::vector<SettingsWindowSize>
make_settings_window_size_options(
    std::optional<SettingsWindowSize> usable_size,
    SettingsWindowSize current_size);

[[nodiscard]] std::vector<double>
make_settings_target_fps_options(double current_fps);

using SettingsPanelSaveCallback = std::function<void(const SettingsPanelDraft&)>;
using SettingsPanelBackCallback = std::function<void()>;

// Reusable settings form. It owns only draft state; applying and persisting the
// draft remains the responsibility of its host.
class SettingsPanel final : public UiListContainer
{
public:
    explicit SettingsPanel(const elysia::core::Rect& rect = elysia::core::Rect{ 0,0,700,680 },int order = 0);
    ~SettingsPanel() override;

    void reset() noexcept override;

    void set_options(SettingsPanelOptions options);
    [[nodiscard]] const SettingsPanelOptions& options() const noexcept;

    void set_draft(const SettingsPanelDraft& draft);
    [[nodiscard]] const SettingsPanelDraft& draft() const noexcept;

    // Starts a fresh settings-page visit without disturbing draft state,
    // options, or callbacks.
    void reset_navigation_state();
    [[nodiscard]] SettingsPanelSection selected_section() const noexcept;

    void set_on_save(SettingsPanelSaveCallback on_save);
    void set_on_back(SettingsPanelBackCallback on_back);

    void set_status_content(UiTextContent content,bool is_error);
    void set_status_message(std::string message,bool is_error);
    void clear_status_message();

    // Dropdown popups are rendered by the owning window and must be registered
    // after this panel has been adopted into that window.
    void register_with_window(UiWindow& window);
    void unregister_from_window() noexcept;

private:
    void build_controls();
    void rebuild_window_options();
    void rebuild_target_fps_options();
    void rebuild_language_options();
    void sync_controls_from_draft();
    [[nodiscard]] std::size_t find_window_size_index(const SettingsWindowSize& window_size) const noexcept;
    [[nodiscard]] std::size_t find_target_fps_index(double target_fps) const noexcept;
    [[nodiscard]] std::size_t find_language_index(const std::string& language) const noexcept;

private:
    SettingsPanelOptions _options;
    SettingsPanelDraft _draft;
    SettingsPanelSaveCallback _on_save;
    SettingsPanelBackCallback _on_back;
    UiTabContainer* _tab_container = nullptr;
    UiScrollContainer* _display_scroll = nullptr;
    UiScrollContainer* _audio_scroll = nullptr;
    UiScrollContainer* _general_scroll = nullptr;
    UiDropdown* _window_option_dropdown = nullptr;
    UiDropdown* _target_fps_dropdown = nullptr;
    UiLabeledCheckbox* _vsync_checkbox = nullptr;
    UiSlider* _master_volume_slider = nullptr;
    UiSlider* _music_volume_slider = nullptr;
    UiSlider* _sound_volume_slider = nullptr;
    UiDropdown* _language_dropdown = nullptr;
    UiLabel* _status_label = nullptr;
    UiWindow* _window = nullptr;
    bool _syncing_controls = false;
};
}
