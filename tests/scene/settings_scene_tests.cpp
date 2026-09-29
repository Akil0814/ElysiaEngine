#include "tests/support/input_snapshot_builder.h"
#define SDL_MAIN_HANDLED

#include "engine/config/user_config_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "tests/support/test_assertions.h"
#include "tests/support/scene_test_access.h"

#include "engine/ui/window/ui_window.h"
#include "engine/ui/presets/settings_panel.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/tools/logger.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using elysia::tests::require;

struct ReturnPayload
{
    int marker = 0;
};

template <int Id>
class ReturnScene final : public elysia::scene::Scene
{
public:
    void on_enter(const elysia::scene::ScenePayload& payload) override
    {
        const ReturnPayload* return_payload =
            elysia::scene::try_scene_payload<ReturnPayload>(payload);
        if (!return_payload)
            throw std::logic_error(
                "ReturnScene requires ReturnPayload.");
        marker = return_payload->marker;
        last_instance = this;
    }

    void on_exit() override {}
    void on_reset() override {}

    void open_settings(
        const elysia::scene::SceneRoute& return_route,
        elysia::ui::SettingsPanelVisibility visibility = {})
    {
        request_scene_switch(
            elysia::builtin::SceneKeys::Settings,
            elysia::builtin::SettingsScenePayload{
                .return_route = return_route,
                .visibility = visibility
            });
    }

    static inline ReturnScene* last_instance = nullptr;
    static inline int marker = 0;
};

using FirstReturnScene = ReturnScene<1>;
using SecondReturnScene = ReturnScene<2>;

class ConfigHandler final : public elysia::config::IUserConfigChangeHandler
{
public:
    std::expected<void,elysia::config::UserConfigFailure>
        apply_master_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_music_volume(int value) override
    {
        if (reject_music && value == 13)
            return std::unexpected(elysia::config::make_user_config_failure(
                elysia::config::UserConfigError::RuntimeApplyFailed,"music_volume","primary-music-failure"));
        return {};
    }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_sound_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_language(std::string_view) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_target_fps(double value) override
    {
        if (reject_fps_rollback && value == 60.0)
            return std::unexpected(elysia::config::make_user_config_failure(
                elysia::config::UserConfigError::RuntimeApplyFailed,"target_fps","rollback-fps-failure"));
        target_fps_values.push_back(value);
        return {};
    }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_window_settings(
            const elysia::config::WindowSettings&) override { return {}; }

    bool reject_music = false,reject_fps_rollback = false;
    std::vector<double> target_fps_values;
};

bool throws_logic_error_containing(
    const std::function<void()>& operation,
    std::string_view expected)
{
    try
    {
        operation();
    }
    catch (const std::logic_error& error)
    {
        return std::string(error.what()).find(expected) != std::string::npos;
    }
    return false;
}

void send_cancel(elysia::scene::SceneManager& scene_manager)
{
    const elysia::input::RawInputFrame frame{};
    const std::vector<elysia::input::RawInputEvent> events{
        elysia::input::RawInputEvent{
            .control = elysia::input::RawInputControl::KeyEscape,
            .type = elysia::input::RawInputEventType::ControlPressed,
            .device = elysia::input::InputDevice::Keyboard
        }
    };
    scene_manager.on_input(elysia::tests::events_snapshot(events));
}

void send_control(
    elysia::scene::SceneManager& scene_manager,
    elysia::input::RawInputControl control,
    bool release = false)
{
    const elysia::input::RawInputFrame frame{};
    const std::vector<elysia::input::RawInputEvent> events{
        elysia::input::RawInputEvent{
            .control = control,
            .type = release
                ? elysia::input::RawInputEventType::ControlReleased
                : elysia::input::RawInputEventType::ControlPressed,
            .device = elysia::input::InputDevice::Keyboard
        }
    };
    scene_manager.on_input(elysia::tests::events_snapshot(events));
}

void activate_focused_control(elysia::scene::SceneManager& scene_manager)
{
    send_control(
        scene_manager,elysia::input::RawInputControl::KeyEnter);
    send_control(
        scene_manager,elysia::input::RawInputControl::KeyEnter,true);
}

elysia::ui::SettingsPanelVisibility settings_visibility(
    bool target_fps,
    bool vsync)
{
    return elysia::ui::SettingsPanelVisibility{
        .window_mode = false,
        .target_fps = target_fps,
        .vsync = vsync,
        .master_volume = false,
        .music_volume = false,
        .sound_volume = false,
        .language = false
    };
}

void test_settings_payload_contract_names_the_scene()
{
    elysia::builtin::SettingsScene scene;
    require(throws_logic_error_containing(
        [&scene] { elysia::scene::SceneTestAccess::enter(scene); },
        "SettingsScene"),
        "missing Settings payload must fail with the built-in scene name");

    const elysia::scene::ScenePayload invalid_payload =
        elysia::builtin::SettingsScenePayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
        [&scene,&invalid_payload] {
            elysia::scene::SceneTestAccess::enter(scene, invalid_payload);
        },
        "SettingsScene"),
        "an invalid return route must fail with the built-in scene name");
}

void test_cancel_returns_to_each_callers_full_route()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / "elysia_settings_scene_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    elysia::config::UserConfigData defaults;
    defaults.language = "en";
    const std::filesystem::path config_path = directory / "user_config.json";
    auto* config_service = elysia::config::UserConfigService::instance();
    require(config_service->initialize(defaults,config_path).has_value(),
        "settings scene test must initialize UserConfigService");
    ConfigHandler handler;
    config_service->register_user_config_change_handler(handler);
    const elysia::config::UserConfigData original =
        config_service->user_config().snapshot();

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr,registry,1280,720);
    elysia::scene::SceneManager scene_manager;
    scene_manager.initialize(context);
    scene_manager.register_engine_scene<
        elysia::builtin::SettingsScene>(
            elysia::builtin::SceneKeys::Settings);
    scene_manager.register_game_scene<FirstReturnScene>(1);
    scene_manager.register_game_scene<SecondReturnScene>(2);

    scene_manager.start(elysia::scene::SceneRoute{
        .target = elysia::builtin::SceneKeys::Settings,
        .payload = elysia::builtin::SettingsScenePayload{
            .return_route = elysia::scene::SceneRoute{
                .target = 1,
                .payload = ReturnPayload{ .marker = 11 },
                .reload_mode = elysia::scene::SceneReloadMode::Reuse
            }
        }
    });
    send_cancel(scene_manager);
    require(scene_manager.current_scene_key() == 1
        && FirstReturnScene::marker == 11,
        "Cancel must return the first caller's key and payload");
    require(config_service->user_config().snapshot() == original,
        "Cancel must not mutate runtime settings");

    FirstReturnScene::last_instance->open_settings(
        elysia::scene::SceneRoute{
            .target = 2,
            .payload = ReturnPayload{ .marker = 22 },
            .reload_mode = elysia::scene::SceneReloadMode::Reset
        });
    scene_manager.on_update(0.0);
    require(scene_manager.current_scene_key()
        == elysia::builtin::SceneKeys::Settings,
        "the cached SettingsScene must be reusable from another caller");

    send_cancel(scene_manager);
    require(scene_manager.current_scene_key() == 2
        && SecondReturnScene::marker == 22,
        "the reused SettingsScene must replace the old return route completely");
    require(config_service->user_config().snapshot() == original,
        "Cancel after a cached re-entry must still leave runtime settings unchanged");

    scene_manager.shutdown();
    config_service->unregister_user_config_change_handler(handler);
    config_service->shutdown();
    std::filesystem::remove_all(directory);
}

void test_save_failure_shows_short_localized_status_and_logs_details()
{
    using namespace elysia;
    const auto directory = std::filesystem::temp_directory_path()/"elysia_settings_short_failure_tests";
    std::filesystem::remove_all(directory);
    auto* service = config::UserConfigService::instance();
    config::UserConfigData defaults;
    defaults.language = "en";
    defaults.target_fps = 60.0;
    require(service->initialize(defaults,directory/"settings.json").has_value(),"failure test initializes config");
    ConfigHandler handler;
    handler.reject_music = handler.reject_fps_rollback = true;
    service->register_user_config_change_handler(handler);
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(nullptr,registry,1280,720);
    builtin::SettingsScene settings;
    scene::SceneTestAccess::bind(settings,context);
    scene::SceneTestAccess::enter(settings,builtin::SettingsScenePayload{.return_route = {.target = 1}});
    auto* window = dynamic_cast<ui::UiWindow*>(scene::SceneTestAccess::ui_root(settings,0));
    auto* panel = window ? dynamic_cast<ui::SettingsPanel*>(window->child_at(0)) : nullptr;
    require(panel,"settings scene owns panel");
    auto* status = dynamic_cast<ui::UiLabel*>(panel->child_at(2));
    auto* actions = dynamic_cast<ui::UiListContainer*>(panel->child_at(3));
    auto* save = actions ? dynamic_cast<ui::UiButton*>(actions->child_at(0)) : nullptr;
    require(status && save,"fixed status and save controls exist");
    auto draft = panel->draft();
    draft.target_fps = 120.0;
    draft.music_volume = 13;
    panel->set_draft(draft);
    std::ostringstream logs;
    auto* previous = std::clog.rdbuf(logs.rdbuf());
    try
    {
        save->set_focused(true);
        (void)save->on_ui_input_event({.action = ui::UiAction::Confirm,.type = ui::UiInputEventType::ActionPressed});
        (void)save->on_ui_input_event({.action = ui::UiAction::Confirm,.type = ui::UiInputEventType::ActionReleased});
    }
    catch (...) { std::clog.rdbuf(previous); throw; }
    std::clog.rdbuf(previous);
    require(status->is_visible() && status->text_content().kind == ui::UiTextContentKind::TextKey
        && status->text_content().value == "engine.settings.status.save_failed",
        "failure label uses a short localized key rather than a multiline report");
    require(logs.str().find("primary-music-failure") != std::string::npos
        && logs.str().find("rollback-fps-failure") != std::string::npos,
        "full primary and rollback diagnostics remain in the log");
    require(panel->draft().target_fps == service->user_config().target_fps()
        && panel->draft().music_volume == service->user_config().music_volume()
        && panel->draft().target_fps == 120.0,"draft follows actual state after partial rollback");
    scene::SceneTestAccess::exit(settings);
    scene::SceneTestAccess::reset(settings);
    service->unregister_user_config_change_handler(handler);
    service->shutdown();
    std::filesystem::remove_all(directory);
}

void test_save_applies_fps_and_tracks_vsync_restart_state()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path()
        / "elysia_settings_scene_save_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    elysia::config::UserConfigData defaults;
    defaults.target_fps = 60.0;
    defaults.vsync = true;
    defaults.audio.master_volume = 73;
    defaults.audio.music_volume = 62;
    defaults.audio.sound_volume = 51;
    defaults.language = "en";
    const std::filesystem::path config_path = directory / "user_config.json";
    auto* config_service = elysia::config::UserConfigService::instance();
    require(config_service->initialize(defaults,config_path).has_value(),
        "settings save test must initialize UserConfigService");
    ConfigHandler handler;
    config_service->register_user_config_change_handler(handler);

    elysia::io::ContentRegistry registry;
    elysia::scene::SceneRuntimeContext context(nullptr,registry,1280,720);
    elysia::scene::SceneManager scene_manager;
    scene_manager.initialize(context);
    scene_manager.register_engine_scene<elysia::builtin::SettingsScene>(
        elysia::builtin::SceneKeys::Settings);
    scene_manager.register_game_scene<FirstReturnScene>(1);
    scene_manager.start(elysia::scene::SceneRoute{
        .target = elysia::builtin::SceneKeys::Settings,
        .payload = elysia::builtin::SettingsScenePayload{
            .return_route = elysia::scene::SceneRoute{
                .target = 1,
                .payload = ReturnPayload{ .marker = 33 }
            },
            .visibility = settings_visibility(true,true)
        }
    });

    activate_focused_control(scene_manager);
    send_control(scene_manager,elysia::input::RawInputControl::KeyDown);
    activate_focused_control(scene_manager);
    send_control(scene_manager,elysia::input::RawInputControl::KeyDown);
    activate_focused_control(scene_manager);
    send_control(scene_manager,elysia::input::RawInputControl::KeyDown);
    activate_focused_control(scene_manager);

    require(config_service->user_config().target_fps() == 120.0
        && !config_service->user_config().vsync()
        && config_service->user_config().master_volume() == 73
        && config_service->user_config().music_volume() == 62
        && config_service->user_config().sound_volume() == 51
        && config_service->user_config().language() == "en"
        && config_service->user_config().restart_required()
        && !config_service->user_config().is_dirty()
        && handler.target_fps_values
            == std::vector<double>{ 120.0 },
        "SettingsScene Save must apply FPS immediately, persist VSync, and retain its restart requirement");

    send_control(scene_manager,elysia::input::RawInputControl::KeyUp);
    activate_focused_control(scene_manager);
    send_control(scene_manager,elysia::input::RawInputControl::KeyDown);
    activate_focused_control(scene_manager);

    require(config_service->user_config().vsync()
        && !config_service->user_config().restart_required()
        && !config_service->user_config().is_dirty(),
        "saving the startup VSync value again must clear the restart requirement");

    send_cancel(scene_manager);
    require(scene_manager.current_scene_key() == 1,
        "leaving the customized SettingsScene must restore its caller");
    FirstReturnScene::last_instance->open_settings(
        elysia::scene::SceneRoute{
            .target = 1,
            .payload = ReturnPayload{ .marker = 44 }
        },
        settings_visibility(false,true));
    scene_manager.on_update(0.0);
    require(scene_manager.current_scene_key()
            == elysia::builtin::SceneKeys::Settings,
        "the cached SettingsScene must accept a different visibility profile");

    activate_focused_control(scene_manager);
    send_control(scene_manager,elysia::input::RawInputControl::KeyDown);
    activate_focused_control(scene_manager);
    require(config_service->user_config().target_fps() == 120.0
            && !config_service->user_config().vsync()
            && config_service->user_config().master_volume() == 73
            && config_service->user_config().music_volume() == 62
            && config_service->user_config().sound_volume() == 51,
        "rebuilding for a new visibility profile must preserve every hidden setting");

    scene_manager.shutdown();
    config_service->unregister_user_config_change_handler(handler);
    config_service->shutdown();
    std::filesystem::remove_all(directory);
}
}

int main()
{
    test_settings_payload_contract_names_the_scene();
    test_cancel_returns_to_each_callers_full_route();
    test_save_failure_shows_short_localized_status_and_logs_details();
    test_save_applies_fps_and_tracks_vsync_restart_state();
    return EXIT_SUCCESS;
}
