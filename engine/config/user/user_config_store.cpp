#include "user_config_store.h"

#include "user_config_json_fields.h"
#include "../../io/json/strict_json.h"

#include <chrono>
#include <fstream>
#include <set>

namespace elysia::config
{
namespace
{
using Data = UserConfigData;
using Json = elysia::io::json;
constexpr int k_schema_version = 2;

enum class ParseKind { Valid, Invalid, Future };
struct ParseResult { ParseKind kind = ParseKind::Invalid; Data data; std::string error; };

bool allowed_fields(const Json& object,std::initializer_list<std::string_view> fields,
    bool require_all,std::string_view path,std::string& error)
{
    if (!object.is_object()) { error = std::string(path) + " must be an object."; return false; }
    std::set<std::string> allowed;
    for (auto field : fields) allowed.emplace(field);
    for (const auto& [key,value] : object.items())
        if (!allowed.contains(key)) { error = "Unknown UserConfig field: " + std::string(path) + "." + key; return false; }
    if (require_all) for (const auto& key : allowed)
        if (!object.contains(key)) { error = "Missing UserConfig field: " + std::string(path) + "." + key; return false; }
    return true;
}

ParseResult parse(const std::filesystem::path& path,const Data& defaults)
{
    const auto loaded = elysia::io::load_strict_json(path);
    if (!loaded) return {ParseKind::Invalid,{},loaded.error()};
    const Json& root = *loaded;
    if (!root.is_object()) return {ParseKind::Invalid,{},"UserConfig root must be an object."};
    if (!root.contains("schema_version")
        || !root.at("schema_version").is_number_integer())
        return {ParseKind::Invalid,{},"UserConfig schema_version must be an integer."};
    const int version = root.at("schema_version").get<int>();
    if (version > k_schema_version)
        return {ParseKind::Future,{},"UserConfig schema_version is newer than this application."};
    if (version != k_schema_version)
        return {ParseKind::Invalid,{},"Unsupported UserConfig schema_version."};
    Data data = defaults;
    std::string error;
    if (!allowed_fields(
            root,
            {"schema_version","window","render","audio","localization"},
            true,
            "root",
            error))
        return {ParseKind::Invalid,{},error};
    auto section = [&](const char* name,const Json*& out)
    {
        if (!root.at(name).is_object()) { error = std::string(name) + " must be an object."; return false; }
        out = &root.at(name); return true;
    };
    const Json* node = nullptr;
    if (!section("window",node)) return {ParseKind::Invalid,{},error};
    if (!allowed_fields(*node,{"mode","windowed_size"},true,"window",error)
        || !detail::parse_window_mode(
            node->at("mode"),
            data.window.mode,
            error)
        || !allowed_fields(
            node->at("windowed_size"),
            {"width","height"},
            true,
            "window.windowed_size",
            error)
        || !detail::parse_positive_int(
            node->at("windowed_size"),
            "width",
            data.window.windowed_size.width,
            error)
        || !detail::parse_positive_int(
            node->at("windowed_size"),
            "height",
            data.window.windowed_size.height,
            error))
        return {ParseKind::Invalid,{},error};
    node = nullptr;
    if (!section("render",node)) return {ParseKind::Invalid,{},error};
    if (!allowed_fields(*node,{"fps","vsync"},true,"render",error))
        return {ParseKind::Invalid,{},error};
    if (!detail::parse_positive_number(
            *node,
            "fps",
            data.target_fps,
            error)
        || !detail::parse_boolean(*node,"vsync",data.vsync,error))
        return {ParseKind::Invalid,{},error};
    node = nullptr;
    if (!section("audio",node)) return {ParseKind::Invalid,{},error};
    if (!allowed_fields(*node,{"master_volume","music_volume","sound_volume"},true,"audio",error)
        || !detail::parse_volume(*node,"master_volume",data.audio.master_volume,error)
        || !detail::parse_volume(*node,"music_volume",data.audio.music_volume,error)
        || !detail::parse_volume(*node,"sound_volume",data.audio.sound_volume,error))
        return {ParseKind::Invalid,{},error};
    node = nullptr;
    if (!section("localization",node)) return {ParseKind::Invalid,{},error};
    if (!allowed_fields(*node,{"language"},true,"localization",error))
        return {ParseKind::Invalid,{},error};
    if (!detail::parse_non_empty_string(
            *node,
            "language",
            data.language,
            error))
        return {ParseKind::Invalid,{},error};
    return {ParseKind::Valid,std::move(data),{}};
}

Json serialize(const Data& data)
{
    const char* mode = "invalid";
    switch (data.window.mode)
    {
    case WindowMode::Windowed:
        mode = "windowed";
        break;
    case WindowMode::BorderlessFullscreen:
        mode = "borderless_fullscreen";
        break;
    }
    return {{"schema_version",k_schema_version},
        {"window",{
            {"mode",mode},
            {"windowed_size",{
                {"width",data.window.windowed_size.width},
                {"height",data.window.windowed_size.height}
            }}
        }},
        {"render",{{"fps",data.target_fps},{"vsync",data.vsync}}},
        {"audio",{{"master_volume",data.audio.master_volume},{"music_volume",data.audio.music_volume},{"sound_volume",data.audio.sound_volume}}},
        {"localization",{{"language",data.language}}}};
}

UserConfigFailure failure(UserConfigError kind,std::string message)
{ return {kind,{},std::move(message)}; }
}

std::expected<void,UserConfigFailure> UserConfigStore::save(const std::filesystem::path& path,const Data& data) const
{
    const auto tmp = path.string() + ".tmp";
    const auto bak = path.string() + ".bak";
    try
    {
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        { std::ofstream output(tmp,std::ios::trunc); if (!output) return std::unexpected(failure(UserConfigError::SaveFailed,"Open temporary UserConfig failed: " + tmp)); output << serialize(data).dump(2); if (!output.good()) return std::unexpected(failure(UserConfigError::SaveFailed,"Write temporary UserConfig failed: " + tmp)); }
        const auto verified = parse(tmp,data);
        if (verified.kind != ParseKind::Valid) return std::unexpected(failure(UserConfigError::SaveFailed,"Temporary UserConfig verification failed: " + verified.error));
        if (std::filesystem::exists(bak)) std::filesystem::remove(bak);
        const bool had_primary = std::filesystem::exists(path);
        if (had_primary) std::filesystem::rename(path,bak);
        try { std::filesystem::rename(tmp,path); }
        catch (...)
        {
            if (had_primary && std::filesystem::exists(bak) && !std::filesystem::exists(path)) std::filesystem::rename(bak,path);
            throw;
        }
        return {};
    }
    catch (const std::exception& exception)
    { return std::unexpected(failure(UserConfigError::SaveFailed,"Atomic UserConfig save failed: " + std::string(exception.what()))); }
}

std::expected<UserConfigLoadResult,UserConfigFailure> UserConfigStore::load(
    const std::filesystem::path& path,const Data& defaults) const
{
    UserConfigLoadResult result;
    const std::filesystem::path tmp = path.string() + ".tmp";
    const std::filesystem::path bak = path.string() + ".bak";
    if (std::filesystem::exists(path))
    {
        const auto primary = parse(path,defaults);
        if (primary.kind == ParseKind::Future) return std::unexpected(failure(UserConfigError::LoadFailed,primary.error));
        if (primary.kind == ParseKind::Valid)
        {
            result.settings = primary.data;
            return result;
        }
        result.warning = primary.error;
        try
        {
            const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            std::filesystem::rename(path,path.string() + "." + std::to_string(stamp) + ".corrupt");
        }
        catch (const std::exception& exception) { result.warning += " Failed to archive corrupt file: " + std::string(exception.what()); }
    }
    for (const auto& candidate : {tmp,bak})
    {
        if (!std::filesystem::exists(candidate)) continue;
        const auto recovered = parse(candidate,defaults);
        if (recovered.kind == ParseKind::Future) return std::unexpected(failure(UserConfigError::LoadFailed,recovered.error));
        if (recovered.kind == ParseKind::Valid)
        {
            result.settings = recovered.data; result.recovered = true;
            if (auto saved = save(path,result.settings); !saved) return std::unexpected(saved.error());
            return result;
        }
    }
    result.settings = defaults; result.rebuilt = true;
    if (auto saved = save(path,result.settings); !saved) return std::unexpected(saved.error());
    return result;
}
}
