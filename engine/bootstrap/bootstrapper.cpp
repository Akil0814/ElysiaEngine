#include "bootstrapper.h"

#include "../config/user_config_service.h"
#include "../io/loaders/content_registry_loader.h"
#include "../io/path/path_manager.h"

#include <utility>

namespace elysia::bootstrap
{
namespace
{
constexpr const char* USER_CONFIG_FILE_NAME = "user_config.json";
}

std::expected<BootstrapOutput,BootstrapFailure>
Bootstrapper::parse_runtime_settings(const std::filesystem::path& executable_path)
{
    _startup_preload_loader.reset();
    elysia::config::UserConfigService::instance()->shutdown();

    elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();
    if (!path_manager->init(executable_path))
        return std::unexpected(BootstrapFailure{ "Bootstrapper phase1 failed: path manager init failed."});

    if (!path_manager->ensure_runtime_dirs())
        return std::unexpected(BootstrapFailure{"Bootstrapper phase1 failed: ensure runtime dirs failed."});

    elysia::io::ContentRegistry content_registry;
    elysia::io::ContentRegistryLoader content_registry_loader;
    if (!content_registry_loader.load(path_manager->content_registry(), content_registry))
        return std::unexpected(BootstrapFailure{"Bootstrapper phase1 failed: content registry load failed."});

    const auto app_config_result =
        _app_config_loader.load(content_registry.bootstrap.app_config);
    if (!app_config_result)
        return std::unexpected(app_config_result.error());
    const std::filesystem::path user_config_path =
        path_manager->player_data() / USER_CONFIG_FILE_NAME;

    const auto config_result =
        elysia::config::UserConfigService::instance()->initialize(
            app_config_result->user_defaults,
            user_config_path
        );
    if (!config_result)
        return std::unexpected(BootstrapFailure{config_result.error().message});

    BootstrapOutput output;
    output.runtime_settings.window_title = app_config_result->window_title;
    output.runtime_settings.user = config_result->settings;
    output.i18n_manifest_path = content_registry.required.i18n;
    output.warning = config_result->warning;
    _startup_preload_loader.set_manifest_path(
        content_registry.bootstrap.preload_manifest);
    output.content_registry = std::move(content_registry);
    return output;
}

std::expected<void,BootstrapFailure>
Bootstrapper::preload_startup_resources(SDL_Renderer* renderer)
{
    return _startup_preload_loader.load(renderer);
}

void Bootstrapper::release_preload_textures() noexcept
{
    _startup_preload_loader.release_textures();
}

SDL_Texture* Bootstrapper::find_preload_texture(
    std::string_view key) const noexcept
{
    return _startup_preload_loader.find_texture(key);
}
}
