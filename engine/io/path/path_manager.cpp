#include "../../tools/logger.h"
#include "path_manager.h"
#include <string>

namespace elysia::io
{
bool PathManager::init()
{
    return init({});
}

bool PathManager::init(const std::filesystem::path& start_path)
{
    std::optional<std::filesystem::path> root_path;
    try
    {
        root_path = find_project_root(
            start_path.empty()
                ? std::filesystem::current_path()
                : start_path);
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        ELYSIA_LOG_ERROR("path","Path manager init failed while reading the current working directory: "
            << error.what());
        return false;
    }

    if (!root_path.has_value())
    {
        ELYSIA_LOG_ERROR("path","Path manager init failed: project root was not found from the current working directory.");
        return false;
    }

    _root = root_path.value();
    if (!validate_core_asset_dirs())
        return false;

    _is_init = true;

    return true;
}

bool PathManager::ensure_runtime_dirs() const
{
    try
    {
        std::filesystem::create_directories(player_data());
        std::filesystem::create_directories(saves());
        std::filesystem::create_directories(logs());
        return true;
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        ELYSIA_LOG_ERROR("path","Path manager runtime directory setup failed: " << error.what());
        return false;
    }
}

bool PathManager::validate_core_asset_dirs() const
{
    const auto validate_dir = [](const std::filesystem::path& dir_path, const char* dir_name)
    {
        if (std::filesystem::is_directory(dir_path))
            return true;

        ELYSIA_LOG_ERROR("path","Path manager init failed: required asset directory does not exist: "
            << dir_name << " -> " << dir_path);
        return false;
    };

    try
    {
        return validate_dir(audio(), "audio")
            && validate_dir(textures(), "textures")
            && validate_dir(fonts(), "fonts")
            && validate_dir(configs(), "configs");
    }
    catch (const std::filesystem::filesystem_error&)
    {
        ELYSIA_LOG_ERROR("path","Path manager init failed: filesystem error while validating asset directories.");
        return false;
    }
}


const std::filesystem::path& PathManager::root() const
{
    return _root;
}

std::filesystem::path PathManager::assets() const
{
    return _root / "assets";
}

std::filesystem::path PathManager::configs() const
{
    return assets() / "configs";
}

std::filesystem::path PathManager::fonts() const
{
    return assets() / "fonts";
}

std::filesystem::path PathManager::preload() const
{
    return assets() / "preload";
}

std::filesystem::path PathManager::audio() const
{
    return assets() / "audio";
}

std::filesystem::path PathManager::textures() const
{
    return assets() / "textures";
}

std::filesystem::path PathManager::player_data() const
{
    return _root / "player_data";
}

std::filesystem::path PathManager::saves() const
{
    return player_data() / "saves";
}

std::filesystem::path PathManager::logs() const
{
    return _root / "logs";
}

std::filesystem::path PathManager::content_registry() const
{
    return assets() / "content_registry.json";
}

std::filesystem::path PathManager::to_project_path(const std::filesystem::path& path) const
{
    if (path.is_absolute())
        return path.lexically_normal();

    return (_root / path).lexically_normal();
}

std::filesystem::path PathManager::to_asset_path(const std::filesystem::path& path) const
{
    if (path.is_absolute())
        return path.lexically_normal();

    if (path_starts_with(path, "assets"))
        return to_project_path(path);

    return (assets() / path).lexically_normal();
}

std::filesystem::path PathManager::to_config_path(const std::filesystem::path& path) const
{
    if (path.is_absolute())
        return path.lexically_normal();

    if (path_starts_with(path, "assets"))
        return to_project_path(path);

    if (path_starts_with(path, "configs"))
        return to_asset_path(path);

    return (configs() / path).lexically_normal();
}

bool PathManager::path_starts_with(
    const std::filesystem::path& path,
    const std::string& first_part
) const
{
    std::filesystem::path::iterator iterator = path.begin();
    if (iterator == path.end())
        return false;

    return iterator->string() == first_part;
}

std::optional<std::filesystem::path> PathManager::find_project_root(const std::filesystem::path& start_path) const
{
    constexpr int MAX_SEARCH_DEPTH = 8;

    try
    {
        std::filesystem::path current = std::filesystem::absolute(start_path);
        if (!std::filesystem::is_directory(current))
            current = current.parent_path();

        std::optional<std::filesystem::path> magic_root;
        for (int depth = 0; depth < MAX_SEARCH_DEPTH; ++depth)
        {
            const std::filesystem::path assets_dir_path = current / "assets";
            const std::filesystem::path assets_magic_file_path = assets_dir_path / ".elysia_root";
            const std::filesystem::path content_registry_path = assets_dir_path / "content_registry.json";

            const bool assets_is_dir = std::filesystem::is_directory(assets_dir_path);
            const bool has_assets_magic_file = std::filesystem::exists(assets_magic_file_path);
            const bool has_content_registry = std::filesystem::exists(content_registry_path);

            // Prefer a directory that contains a real content registry (source project root).
            if (assets_is_dir && has_content_registry)
                return current;

            // Remember a candidate root identified by the magic file, but keep looking upward
            // in case we started from a build/output folder that also copied the magic file.
            if (assets_is_dir && has_assets_magic_file && !magic_root.has_value())
                magic_root = current;

            if (current == current.root_path())
                break;

            current = current.parent_path();
        }

        if (magic_root.has_value())
            return magic_root;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return std::nullopt;
    }

    return std::nullopt;
}

}
