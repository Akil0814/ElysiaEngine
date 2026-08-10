#include "animation_effect_manifest_loader.h"

#include "../json/json_duplicate_key_checker.h"
#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<AnimationEffectManifest,ManifestLoadFailure>
AnimationEffectManifestLoader::load(const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::source_location origin = std::source_location::current())
        -> std::expected<AnimationEffectManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),std::move(key),manifest_path,origin));
    };
    if (has_duplicate_json_object_key(manifest_path))
        return fail(ManifestLoadError::DuplicateKey,
            "Load effect manifest failed: duplicate JSON object key.");
    JsonLoader loader;
    const JsonReadResult read = loader.open_file(manifest_path);
    if (!read)
        return fail(ManifestLoadError::OpenFailed,"Load effect manifest failed: " + read.error);
    if (!loader.root().is_object() || !loader.root().contains("effects")
        || !loader.root().at("effects").is_array())
        return fail(ManifestLoadError::MissingField,
            "Load effect manifest failed: effects is missing or not an array.");

    AnimationEffectManifest manifest;
    std::size_t index = 0;
    for (const json& node : loader.root().at("effects"))
    {
        if (!node.is_object() || !node.contains("key") || !node.at("key").is_string()
            || !node.contains("animation_key") || !node.at("animation_key").is_string())
            return fail(ManifestLoadError::InvalidSchema,
                "Load effect manifest failed: invalid effect entry.");
        AnimationEffectManifestEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.animation_key = node.at("animation_key").get<std::string>();
        if (node.contains("default_width"))
        {
            if (!node.at("default_width").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_width is invalid.",entry.key);
            entry.default_width = node.at("default_width").get<float>();
        }
        if (node.contains("default_height"))
        {
            if (!node.at("default_height").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_height is invalid.",entry.key);
            entry.default_height = node.at("default_height").get<float>();
        }
        if (node.contains("default_angle_degrees"))
        {
            if (!node.at("default_angle_degrees").is_number())
                return fail(ManifestLoadError::InvalidField,
                    "Load effect manifest failed: default_angle_degrees is invalid.",entry.key);
            entry.default_angle_degrees = node.at("default_angle_degrees").get<double>();
        }
        if (entry.default_width < 0 || entry.default_height < 0
            || ((entry.default_width == 0) != (entry.default_height == 0)))
            return fail(ManifestLoadError::InvalidValue,
                "Load effect manifest failed: default size must provide positive width and height.",entry.key);
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "key" && field.key() != "animation_key"
                && field.key() != "default_width" && field.key() != "default_height"
                && field.key() != "default_angle_degrees")
                return fail(ManifestLoadError::UnknownField,
                    "Load effect manifest failed: unknown field: " + field.key(),entry.key);
        if (entry.key.empty() || entry.animation_key.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load effect manifest failed: effect values must not be empty.",entry.key);
        std::string key_error;
        if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key,key_error)
            || !elysia::resources::ResourceKeyBuilder::validate_key(entry.animation_key,key_error))
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load effect manifest failed: " + key_error,entry.key);
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,"/effects/" + std::to_string(index++),{},"effects",{},entry.key);
        manifest.effects.push_back(std::move(entry));
    }
    return manifest;
}
}
