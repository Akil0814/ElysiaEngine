#include "config_manifest_loader.h"

#include "config_load_utils.h"
#include "../../core/validation/dotted_key_validator.h"
#include "../../io/json/strict_json.h"

namespace elysia::config
{
std::expected<ConfigManifest,ConfigLoadFailure> ConfigManifestLoader::load(
    const std::filesystem::path& path) const
{
    const auto parsed = elysia::io::load_strict_json(path);
    const std::string source = config_project_relative(path);
    if (!parsed)
    {
        const std::string duplicate = duplicate_config_property(parsed.error());
        if (!duplicate.empty())
        {
            ConfigOrigin origin{source,"/"+config_pointer_component(duplicate),{},duplicate};
            return std::unexpected(make_config_load_failure(ConfigLoadError::DuplicateKey,parsed.error(),origin,origin));
        }
        return std::unexpected(make_config_load_failure(ConfigLoadError::OpenFailed,parsed.error()));
    }
    const auto& root = *parsed;
    if (!root.is_object() || root.size() != 2 || !root.contains("schema_version") || !root.contains("configs"))
        return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidSchema,
            "Config manifest must contain only schema_version and configs."));
    if (!root.at("schema_version").is_number_integer() || root.at("schema_version").get<int>() != 1)
        return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidSchema,
            "Config manifest schema_version must be 1."));
    if (!root.at("configs").is_object())
        return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidSchema,
            "Config manifest configs must be an object."));
    auto* paths = elysia::io::PathManager::instance();
    if (!paths->is_initialized())
        return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidValue,
            "PathManager must be initialized before loading game configs."));

    ConfigManifest manifest;
    for (const auto& [key_namespace,path_value] : root.at("configs").items())
    {
        const std::string pointer = "/configs/" + config_pointer_component(key_namespace);
        ConfigOrigin origin{source,pointer,key_namespace,key_namespace};
        std::string key_error;
        if (!elysia::core::DottedKeyValidator::validate_key(key_namespace,key_error))
            return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidKey,
                "Invalid config namespace '" + key_namespace + "': " + key_error,origin));
        if (!path_value.is_string() || path_value.get<std::string>().empty())
            return std::unexpected(make_config_load_failure(ConfigLoadError::InvalidSchema,
                "Config document path must be a non-empty string: " + key_namespace,origin));
        const auto document_path = paths->to_asset_path(path_value.get<std::string>());
        if (!std::filesystem::is_regular_file(document_path))
            return std::unexpected(make_config_load_failure(ConfigLoadError::OpenFailed,
                "Config document does not exist: " + config_project_relative(document_path),origin));
        manifest.entries.push_back({key_namespace,document_path,std::move(origin)});
    }
    return manifest;
}
}
