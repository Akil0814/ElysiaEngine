#pragma once

#include "application_run_result.h"
#include "game_module.h"

#include "../builtin/resources/builtin_asset_cache.h"
#include "../builtin/audio/builtin_audio_player.h"
#include "../bootstrap/bootstrap_types.h"
#include "../config/user_config_service.h"
#include "../input/input_system.h"
#include "../io/loaders/asset_config_types.h"
#include "../scene/scene_manager.h"
#include "../scene/scene_manager_observer.h"
#include "../scene/runtime/scene_runtime_context.h"
#include "../tools/singleton.h"
#include "../typography/font_resolver.h"

#include <SDL.h>

#include <expected>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>


#define ELYSIA_APP (elysia::application::Application::instance())

#define ELYSIA_INIT_APP(argc, argv, game_module) \
    (elysia::application::Application::instance()->init((argc), (argv), (game_module)))

#define ELYSIA_RUN_APP \
    (elysia::application::Application::instance()->run())

namespace elysia::application
{
class Application final
    : public elysia::tools::Singleton<Application>
    , public elysia::scene::SceneManagerObserver
    , public elysia::config::IUserConfigChangeHandler
{
    friend elysia::tools::Singleton<Application>;

public:
    ~Application();

    bool init(int argc,char** argv,const IGameModule& game_module);
    ApplicationRunResult run();

    std::expected<void,elysia::config::UserConfigFailure> apply_master_volume(int value) override;
    std::expected<void,elysia::config::UserConfigFailure> apply_music_volume(int value) override;
    std::expected<void,elysia::config::UserConfigFailure> apply_sound_volume(int value) override;
    std::expected<void,elysia::config::UserConfigFailure> apply_language(std::string_view language) override;
    std::expected<void,elysia::config::UserConfigFailure> apply_target_fps(double value) override;
    std::expected<void,elysia::config::UserConfigFailure> apply_window_settings(
        const elysia::config::WindowSettings& settings) override;

private:
    Application() = default;

    bool init_runtime(
        const elysia::bootstrap::RuntimeSettings& settings,
        const ApplicationDescriptor& descriptor);
    bool enter_initial_scene(
        const IGameModule& game_module,
        const ApplicationDescriptor& descriptor);
    void shutdown();

    void on_scene_manager_quit_requested() override;

    bool check_startup_step(
        bool flag,
        std::string_view category,
        const char* err_msg,
        std::source_location location = std::source_location::current());
    bool startup_fail(
        std::string_view category,
        const std::string& err_msg,
        std::source_location location = std::source_location::current());

private:
    double _target_fps = 60.0;

    SDL_Event _event{};
    SDL_Window* _window = nullptr;
    SDL_Renderer* _renderer = nullptr;

    elysia::input::InputSystem _input_system;
    elysia::scene::SceneManager _scene_manager;
    elysia::io::ContentRegistry _content_registry;
    elysia::builtin::BuiltinAssetCache _builtin_asset_cache;
    elysia::builtin::BuiltinAudioPlayer _builtin_audio_player;
    elysia::typography::FontResolver _font_resolver;
    std::optional<elysia::scene::SceneRuntimeContext> _scene_runtime_context;

    bool _active = true;
    bool _normal_exit_requested = false;
    bool _has_shutdown = false;
    bool _user_config_handler_registered = false;
    bool _sdl_initialized = false;
    bool _image_initialized = false;
    bool _mixer_initialized = false;
    bool _ttf_initialized = false;
    bool _audio_device_open = false;
};
}
