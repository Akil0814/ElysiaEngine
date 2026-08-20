#define SDL_MAIN_HANDLED

#include "engine/config/user_config_service.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/builtin/builtin_scene_keys.h"
#include "engine/builtin/scenes/settings_scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/scene/runtime/scene_runtime_context.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
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
    void reset() override {}

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
        apply_music_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_sound_volume(int) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_language(std::string_view) override { return {}; }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_target_fps(double value) override
    {
        target_fps_values.push_back(value);
        return {};
    }
    std::expected<void,elysia::config::UserConfigFailure>
        apply_window_settings(
            const elysia::config::WindowSettings&) override { return {}; }

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
    scene_manager.on_input(frame,events);
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
    scene_manager.on_input(frame,events);
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
        [&scene] { scene.on_enter({}); },
        "SettingsScene"),
        "missing Settings payload must fail with the built-in scene name");

    const elysia::scene::ScenePayload invalid_payload =
        elysia::builtin::SettingsScenePayload{
            .return_route = elysia::scene::SceneRoute{ .target = 1000 }
        };
    require(throws_logic_error_containing(
        [&scene,&invalid_payload] { scene.on_enter(invalid_payload); },
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
    scene_manager.set_runtime_context(context);
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
    scene_manager.set_runtime_context(context);
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
    test_save_applies_fps_and_tracks_vsync_restart_state();
    return EXIT_SUCCESS;
}
