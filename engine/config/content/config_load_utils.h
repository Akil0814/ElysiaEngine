#pragma once

#include "../config_types.h"
#include "../../io/path/path_manager.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace elysia::config
{
inline std::string config_project_relative(const std::filesystem::path& path)
{
    try
    {
        auto* paths = elysia::io::PathManager::instance();
        if (paths->is_initialized())
        {
            const auto relative = std::filesystem::relative(path,paths->root());
            if (!relative.empty() && *relative.begin() != "..") return relative.generic_string();
        }
    }
    catch (...) {}
    return path.generic_string();
}

inline std::string config_pointer_component(std::string_view value)
{
    std::string result;
    for (char character : value)
    {
        if (character == '~') result += "~0";
        else if (character == '/') result += "~1";
        else result += character;
    }
    return result;
}

inline std::string duplicate_config_property(std::string_view message)
{
    constexpr std::string_view prefix = "Duplicate JSON property '";
    if (!message.starts_with(prefix)) return {};
    const size_t end = message.find('\'',prefix.size());
    return end == std::string_view::npos ? std::string{} : std::string(message.substr(prefix.size(),end-prefix.size()));
}

inline ConfigLoadFailure make_config_load_failure(ConfigLoadError error,std::string message,
    ConfigOrigin first = {},ConfigOrigin second = {})
{
    return {error,std::move(message),std::move(first),std::move(second)};
}
}
