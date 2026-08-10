#include "app_config_loader.h"

#include "../io/json/strict_json.h"
#include "../config/user/user_config_json_fields.h"

#include <set>

namespace elysia::bootstrap
{
namespace
{
using Json = elysia::io::json;

bool exact_fields(const Json& node,std::initializer_list<std::string_view> expected,
    std::string_view path,std::string& error)
{
    if (!node.is_object()) { error = std::string(path) + " must be an object."; return false; }
    std::set<std::string> allowed;
    for (const auto key : expected) allowed.emplace(key);
    for (const auto& [key,value] : node.items())
        if (!allowed.contains(key)) { error = "Unknown AppConfig field: " + std::string(path) + "." + key; return false; }
    for (const auto& key : allowed)
        if (!node.contains(key)) { error = "Missing AppConfig field: " + std::string(path) + "." + key; return false; }
    return true;
}

}

std::expected<AppConfig,BootstrapFailure> AppConfigLoader::load(
    const std::filesystem::path& path) const
{
    const auto parsed = elysia::io::load_strict_json(path);
    if (!parsed) return std::unexpected(BootstrapFailure{parsed.error()});
    const Json& root = *parsed;
    std::string error;
    if (!exact_fields(root,{"schema_version","window","render","audio","localization"},"root",error))
        return std::unexpected(BootstrapFailure{error});
    if (!root.at("schema_version").is_number_integer()
        || root.at("schema_version").get<int>() != 2)
        return std::unexpected(BootstrapFailure{"AppConfig schema_version must be 2."});
    if (!exact_fields(
            root.at("window"),
            {"title","mode","windowed_size"},
            "window",
            error)
        || !exact_fields(
            root.at("window").at("windowed_size"),
            {"width","height"},
            "window.windowed_size",
            error)
        || !exact_fields(root.at("render"),{"fps","vsync"},"render",error)
        || !exact_fields(root.at("audio"),{"master_volume","music_volume","sound_volume"},"audio",error)
        || !exact_fields(root.at("localization"),{"language"},"localization",error))
        return std::unexpected(BootstrapFailure{error});

    AppConfig result;
    const Json& window = root.at("window");
    if (!elysia::config::detail::parse_non_empty_string(
            window,
            "title",
            result.window_title,
            error))
        return std::unexpected(BootstrapFailure{"AppConfig " + error});
    if (!elysia::config::detail::parse_window_mode(
            window.at("mode"),
            result.user_defaults.window.mode,
            error))
        return std::unexpected(BootstrapFailure{"AppConfig " + error});
    const Json& windowed_size = window.at("windowed_size");
    if (!elysia::config::detail::parse_positive_int(
            windowed_size,
            "width",
            result.user_defaults.window.windowed_size.width,
            error)
        || !elysia::config::detail::parse_positive_int(
            windowed_size,
            "height",
            result.user_defaults.window.windowed_size.height,
            error))
        return std::unexpected(BootstrapFailure{"AppConfig " + error});

    const Json& render = root.at("render");
    if (!elysia::config::detail::parse_positive_number(
            render,
            "fps",
            result.user_defaults.target_fps,
            error)
        || !elysia::config::detail::parse_boolean(
            render,
            "vsync",
            result.user_defaults.vsync,
            error))
        return std::unexpected(BootstrapFailure{"AppConfig render." + error});

    const Json& audio = root.at("audio");
    if (!elysia::config::detail::parse_volume(
            audio,"master_volume",result.user_defaults.audio.master_volume,error)
        || !elysia::config::detail::parse_volume(
            audio,"music_volume",result.user_defaults.audio.music_volume,error)
        || !elysia::config::detail::parse_volume(
            audio,"sound_volume",result.user_defaults.audio.sound_volume,error))
        return std::unexpected(BootstrapFailure{"AppConfig audio." + error});
    const Json& localization = root.at("localization");
    if (!elysia::config::detail::parse_non_empty_string(
            localization,
            "language",
            result.user_defaults.language,
            error))
        return std::unexpected(BootstrapFailure{"AppConfig localization." + error});
    return result;
}
}
