#pragma once
#include "../../tools/singleton.h"

#include <filesystem>
#include <optional>

namespace elysia::io
{
class PathManager : public elysia::tools::Singleton<PathManager>
{
    friend elysia::tools::Singleton<PathManager>;

public:
    bool init();
    bool init(const std::filesystem::path& start_path);
    bool ensure_runtime_dirs() const;

    const std::filesystem::path& root() const;


    std::filesystem::path assets() const;
    std::filesystem::path logs() const;
    std::filesystem::path player_data() const;

    std::filesystem::path configs() const;
    std::filesystem::path fonts() const;
    std::filesystem::path preload() const;
    std::filesystem::path audio() const;
    std::filesystem::path textures() const;

    std::filesystem::path saves() const;

    std::filesystem::path content_registry() const;
    std::filesystem::path to_project_path(const std::filesystem::path& path) const;
    std::filesystem::path to_asset_path(const std::filesystem::path& path) const;
    std::filesystem::path to_config_path(const std::filesystem::path& path) const;

    bool is_initialized() const { return _is_init; };

private:
    PathManager() = default;

    bool validate_core_asset_dirs() const;

    bool path_starts_with(
        const std::filesystem::path& path,
        const std::string& first_part
    ) const;
    std::optional<std::filesystem::path> find_project_root(const std::filesystem::path& start_path) const;

private:
    std::filesystem::path _root;
    bool _is_init = false;
};

}
