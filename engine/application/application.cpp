#include "application.h"

#include "lifecycle/application_event_boundary.h"
#include "presentation/application_window_settings.h"
#include "lifecycle/application_exit_policy.h"
#include "composition/application_scene_composition.h"
#include "presentation/application_sdl_presentation.h"
#include "lifecycle/application_termination_logging.h"

#include "../builtin/resources/builtin_asset_catalog.h"
#include "../audio/audio_service.h"
#include "../bootstrap/bootstrapper.h"
#include "../core/time.h"
#include "../effects/runtime/effect_manager.h"
#include "../loading/content_runtime_cleanup.h"
#include "../localization/localization_manager.h"
#include "../localization/localization_service.h"
#include "../io/path/path_manager.h"
#include "../resources/resource_service.h"
#include "../save/save_service.h"
#include "../tools/logger.h"
#include "../ui/style/ui_theme_defaults.h"

#include <exception>
#include <filesystem>
#include <utility>

#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

namespace elysia::application
{
Application::~Application()
{
    shutdown();
}

bool Application::check_startup_step(
    bool flag,
    std::string_view category,
    const char* err_msg,
    std::source_location location)
{
    if (flag)
        return true;

    const std::string error_message =
        err_msg ? err_msg : "Application runtime initialization failed.";
    return startup_fail(category,error_message,location);
}

bool Application::startup_fail(
    std::string_view category,
    const std::string& err_msg,
    std::source_location location)
{
    auto* logger = elysia::tools::Logger::instance();
    logger->error(category,err_msg,location);
    logger->terminating(
        "application",
        "Application terminating during startup after a fatal failure",
        location);
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        "Game Start Error",
        err_msg.c_str(),
        _window);
    shutdown();
    return false;
}

bool Application::init(
    int argc,
    char** argv,
    const IGameModule& game_module)
{
    _active = true;
    _normal_exit_requested = false;
    _has_shutdown = false;

    elysia::tools::TerminationManager::instance()->initialize_lifecycle();

    ApplicationDescriptor descriptor;
    try
    {
        descriptor = describe_game_module(game_module);
    }
    catch (const std::exception& error)
    {
        return startup_fail(
            "game_module",
            std::string("Game module descriptor failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "game_module",
            "Game module descriptor failed with an unknown exception.");
    }

    if (descriptor.logical_width <= 0 || descriptor.logical_height <= 0)
        return startup_fail("game_module","Game module logical viewport must be positive.");

    const auto resolved_font_settings =
        elysia::typography::resolve_font_settings(
            descriptor.presentation.fonts);
    if (!resolved_font_settings)
        return startup_fail("typography",resolved_font_settings.error());

    if (!elysia::ui::UiThemeDefaults::set_builtin_theme(
            descriptor.presentation.ui.default_theme))
    {
        return startup_fail(
            "ui",
            "Application default UI theme is invalid.");
    }

    const std::filesystem::path executable_path =
        argc > 0 && argv && argv[0]
            ? std::filesystem::path(argv[0])
            : std::filesystem::path{};
    auto parse_result =
        elysia::bootstrap::Bootstrapper::instance()->parse_runtime_settings(
            executable_path);

    if (!parse_result)
        return startup_fail("bootstrap",parse_result.error().message);

    elysia::bootstrap::BootstrapOutput bootstrap_output =
        std::move(*parse_result);
    _content_registry = std::move(bootstrap_output.content_registry);
    elysia::tools::Logger::instance()->initialize();

    if (const auto save_result = ELYSIA_SAVE->initialize(
            elysia::io::PathManager::instance()->saves());
        !save_result)
    {
        return startup_fail("save",save_result.error().message);
    }

    if (!bootstrap_output.warning.empty())
        ELYSIA_LOG_WARN(
            "application",
            "Startup warning: " << bootstrap_output.warning);

    elysia::bootstrap::RuntimeSettings runtime_settings =
        std::move(bootstrap_output.runtime_settings);
    if (!init_runtime(runtime_settings,descriptor))
        return false;

    const elysia::builtin::BuiltinAssetCatalog builtin_asset_catalog(
        *elysia::io::PathManager::instance());
    if (const auto builtin_asset_result = _builtin_asset_cache.initialize(
            _renderer,
            builtin_asset_catalog,
            resolved_font_settings->engine_point_sizes());
        !builtin_asset_result)
    {
        return startup_fail(
            "builtin",
            "Built-in asset initialization failed: " + builtin_asset_result.error());
    }
    _builtin_audio_player.bind(
        _builtin_asset_cache,
        runtime_settings.user.audio);

    if (!elysia::localization::LocalizationManager::instance()->init(
        _renderer,
        bootstrap_output.i18n_manifest_path,
        runtime_settings.user.language,
        &_font_resolver,
        &_builtin_asset_cache))
    {
        return startup_fail("localization","Localization initialization failed.");
    }

    if (const auto font_result = _font_resolver.configure(
            *resolved_font_settings,
            _builtin_asset_cache,
            *elysia::resources::ResourceService::instance(),
            ELYSIA_LOCALIZATION->supported_languages());
        !font_result)
    {
        return startup_fail("typography",font_result.error().message);
    }
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(
        _renderer,
        &_font_resolver);

    elysia::config::UserConfigService::instance()->register_user_config_change_handler(*this);
    _user_config_handler_registered = true;

    elysia::config::UserConfig& user_config =
        elysia::config::UserConfigService::instance()->user_config();
    if (user_config.language()
        != ELYSIA_LOCALIZATION->current_language())
    {
        const auto language_result = user_config.set_language(
            ELYSIA_LOCALIZATION->current_language());
        if (!language_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: normalize language in config failed: "
                << language_result.error().message);
        }
        else if (const auto save_result =
            elysia::config::UserConfigService::instance()->save_user_config();
            !save_result)
        {
            ELYSIA_LOG_WARN("application",
                "Localization warning: save normalized language failed: "
                << save_result.error().message);
        }
    }

    _input_system.init();
    _input_system.set_renderer(_renderer);

    if (const auto preload_result =
            elysia::bootstrap::Bootstrapper::instance()
                ->preload_startup_resources(_renderer);
        !preload_result)
        return startup_fail("loading",preload_result.error().message);

    _scene_runtime_context.emplace(
        _renderer,
        _content_registry,
        descriptor.logical_width,
        descriptor.logical_height,
        &_builtin_asset_cache,
        &_font_resolver,
        &_builtin_audio_player);
    _scene_manager.set_runtime_context(*_scene_runtime_context);

    return enter_initial_scene(game_module,descriptor);
}

bool Application::init_runtime(
    const elysia::bootstrap::RuntimeSettings& settings,
    const ApplicationDescriptor& descriptor)
{
    const elysia::config::UserConfigData& user_settings = settings.user;
    const bool sdl_initialized = SDL_Init(SDL_INIT_EVERYTHING) == 0;
    _sdl_initialized = sdl_initialized;
    if (!check_startup_step(sdl_initialized,"platform","SDL2 Error"))
        return false;

    if (const auto presentation_result =
            detail::configure_sdl_render_hints(descriptor.presentation.render);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    const int img_flags = IMG_INIT_JPG | IMG_INIT_PNG;
    const int initialized_img_flags = IMG_Init(img_flags);
    _image_initialized = initialized_img_flags != 0;
    if (!check_startup_step(
        (initialized_img_flags & img_flags) == img_flags,
        "platform",
        "SDL_image Error"))
    {
        return false;
    }

    const int mix_flags = MIX_INIT_MP3;
    const int initialized_mix_flags = Mix_Init(mix_flags);
    _mixer_initialized = initialized_mix_flags != 0;
    if (!check_startup_step(
        (initialized_mix_flags & mix_flags) == mix_flags,
        "platform",
        "SDL_mixer Error"))
    {
        return false;
    }

    const bool ttf_initialized = TTF_Init() == 0;
    _ttf_initialized = ttf_initialized;
    if (!check_startup_step(ttf_initialized,"platform","SDL_ttf Error"))
        return false;

    const bool audio_device_open =
        Mix_OpenAudio(44100,MIX_DEFAULT_FORMAT,2,2048) == 0;
    _audio_device_open = audio_device_open;
    if (!check_startup_step(audio_device_open,"audio","Mix_OpenAudio Error"))
        return false;
    if (!check_startup_step(
        elysia::audio::AudioService::instance()->init(user_settings.audio),
        "audio",
        "AudioService init failed"))
    {
        return false;
    }

    SDL_SetHint(SDL_HINT_IME_SHOW_UI,"1");

    _window = SDL_CreateWindow(
        settings.window_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        user_settings.window.windowed_size.width,
        user_settings.window.windowed_size.height,
        SDL_WINDOW_SHOWN);
    if (!check_startup_step(_window != nullptr,"platform","SDL_CreateWindow Error"))
        return false;

    if (user_settings.window.mode
            == elysia::config::WindowMode::BorderlessFullscreen
        && SDL_SetWindowFullscreen(_window,SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
    {
        ELYSIA_LOG_WARN(
            "application",
            "Failed to enter borderless fullscreen: " << SDL_GetError());
        SDL_ClearError();
        SDL_SetWindowSize(
            _window,
            user_settings.window.windowed_size.width,
            user_settings.window.windowed_size.height);
        SDL_SetWindowPosition(_window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
    }

    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;
    if (user_settings.vsync)
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;

    _renderer = SDL_CreateRenderer(_window,-1,renderer_flags);
    if (!check_startup_step(_renderer != nullptr,"platform","SDL_CreateRenderer Error"))
        return false;

    if (const auto presentation_result =
            detail::configure_sdl_renderer_presentation(
            _renderer,
            descriptor.logical_width,
            descriptor.logical_height);
        !presentation_result)
    {
        return startup_fail("platform",presentation_result.error());
    }

    _target_fps = user_settings.target_fps;
    return true;
}

bool Application::enter_initial_scene(
    const IGameModule& game_module,
    const ApplicationDescriptor& descriptor)
{
    _scene_manager.attach(this);

    try
    {
        compose_application_scenes(_scene_manager,game_module,descriptor);
    }
    catch (const std::exception& error)
    {
        return startup_fail(
            "scene",
            std::string("Scene composition failed: ") + error.what());
    }
    catch (...)
    {
        return startup_fail(
            "scene",
            "Scene composition failed with an unknown exception.");
    }

    return true;
}

ApplicationRunResult Application::run()
{
    Uint64 last_counter = SDL_GetPerformanceCounter();
    const Uint64 counter_freq = SDL_GetPerformanceFrequency();
    elysia::core::Time::instance()->reset();

    ApplicationRunResult run_result = ApplicationRunResult::NormalExit;
    auto resolve_exit = [this,&run_result]()
    {
        auto* termination_manager = elysia::tools::TerminationManager::instance();
        const ApplicationExitDecision decision =
            resolve_application_exit(_normal_exit_requested,*termination_manager);
        if (decision == ApplicationExitDecision::Continue)
            return false;

        _active = false;
        run_result = to_application_run_result(decision);
        log_fault_exit_if_needed(decision,termination_manager->termination_info());
        return true;
    };
    auto stop_after_boundary_failure = [this,&run_result,&resolve_exit]()
    {
        if (resolve_exit())
            return;

        _active = false;
        run_result = ApplicationRunResult::FaultExit;
        log_published_termination(std::nullopt);
    };

    while (_active)
    {
        _input_system.begin_frame();
        while (SDL_PollEvent(&_event))
        {
            _input_system.process_event(_event);
            if (_event.type == SDL_QUIT)
                _normal_exit_requested = true;
        }

        _input_system.end_frame();
        if (resolve_exit())
            break;

        if (!run_event_boundary("input",[this]()
        {
            _scene_manager.on_input(_input_system.frame(),_input_system.events());
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        const Uint64 current_counter = SDL_GetPerformanceCounter();
        const double delta =
            static_cast<double>(current_counter - last_counter) / counter_freq;
        last_counter = current_counter;
        elysia::core::Time::instance()->begin_frame(delta);

        if (delta * 1000.0 < 1000.0 / _target_fps)
        {
            SDL_Delay(static_cast<Uint32>(
                1000.0 / _target_fps - delta * 1000.0));
        }

        if (!run_event_boundary("update",[this]()
        {
            const double frame_delta = elysia::core::Time::instance()->delta();
            _scene_manager.on_update(frame_delta);
            elysia::audio::AudioService::instance()->update(frame_delta);
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        SDL_SetRenderDrawColor(_renderer,0,0,0,255);
        SDL_RenderClear(_renderer);

        if (!run_event_boundary("render",[this]()
        {
            _scene_manager.on_render(_renderer);
        }))
        {
            stop_after_boundary_failure();
            break;
        }
        if (resolve_exit())
            break;

        SDL_RenderPresent(_renderer);
        if (resolve_exit())
            break;
    }

    shutdown();
    return run_result;
}

void Application::shutdown()
{
    if (_has_shutdown)
        return;

    _has_shutdown = true;
    _active = false;

    _input_system.shutdown();
    _input_system.set_renderer(nullptr);
    _scene_manager.detach(this);
    _scene_manager.shutdown();
    _scene_runtime_context.reset();
    ELYSIA_SAVE->shutdown();

    elysia::localization::LocalizationManager::instance()->shutdown();
    elysia::bootstrap::Bootstrapper::instance()->release_preload_textures();
    _font_resolver.deactivate_project_fonts();
    elysia::effects::EffectManager::instance()->set_runtime_dependencies(nullptr,nullptr);
    elysia::audio::AudioService::instance()->shutdown();
    _builtin_audio_player.unbind();
    elysia::loading::clear_loaded_content();
    _font_resolver.shutdown();
    _builtin_asset_cache.shutdown();
    if (_user_config_handler_registered)
    {
        elysia::config::UserConfigService::instance()
            ->unregister_user_config_change_handler(*this);
        _user_config_handler_registered = false;
    }
    elysia::config::UserConfigService::instance()->shutdown();

    SDL_DestroyRenderer(_renderer);
    _renderer = nullptr;
    SDL_DestroyWindow(_window);
    _window = nullptr;

    if (_audio_device_open)
    {
        Mix_CloseAudio();
        _audio_device_open = false;
    }
    if (_ttf_initialized)
    {
        TTF_Quit();
        _ttf_initialized = false;
    }
    if (_mixer_initialized)
    {
        Mix_Quit();
        _mixer_initialized = false;
    }
    if (_image_initialized)
    {
        IMG_Quit();
        _image_initialized = false;
    }
    if (_sdl_initialized)
    {
        SDL_Quit();
        _sdl_initialized = false;
    }

    ELYSIA_LOG("application","Application shutdown complete");
    elysia::tools::Logger::instance()->shutdown();
}

void Application::on_scene_manager_quit_requested()
{
    _normal_exit_requested = true;
}

namespace
{
std::unexpected<elysia::config::UserConfigFailure> runtime_apply_failure(
    const char* setting,
    const std::string& message)
{
    return std::unexpected(elysia::config::UserConfigFailure{
        elysia::config::UserConfigError::RuntimeApplyFailed,
        setting,
        message
    });
}
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_master_volume(int value)
{
    elysia::audio::AudioService::instance()->set_master_volume(value);
    _builtin_audio_player.set_master_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_music_volume(int value)
{
    elysia::audio::AudioService::instance()->set_music_volume(value);
    _builtin_audio_player.set_music_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_sound_volume(int value)
{
    elysia::audio::AudioService::instance()->set_sound_volume(value);
    _builtin_audio_player.set_sound_volume(value);
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_language(std::string_view language)
{
    if (!ELYSIA_LOCALIZATION->set_language(std::string(language)))
    {
        return runtime_apply_failure("language","Runtime language change failed.");
    }

    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_target_fps(double value)
{
    if (value <= 0.0)
        return runtime_apply_failure("target_fps","Target FPS must be positive.");
    _target_fps = value;
    return {};
}

std::expected<void,elysia::config::UserConfigFailure>
Application::apply_window_settings(
    const elysia::config::WindowSettings& settings)
{
    if (!_window)
        return runtime_apply_failure("window_settings","Application window is unavailable.");

    const auto result = detail::apply_window_settings(settings,
        detail::ApplicationWindowOperations{
            .set_fullscreen = [this](Uint32 flags)
            {
                return SDL_SetWindowFullscreen(_window,flags);
            },
            .set_size = [this](int width,int height)
            {
                SDL_SetWindowSize(_window,width,height);
            },
            .center = [this]()
            {
                SDL_SetWindowPosition(
                    _window,
                    SDL_WINDOWPOS_CENTERED,
                    SDL_WINDOWPOS_CENTERED);
            },
            .error_message = []()
            {
                return std::string(SDL_GetError());
            }
        });
    if (!result)
    {
        return runtime_apply_failure(
            "window_settings",
            result.error());
    }
    return {};
}
}
