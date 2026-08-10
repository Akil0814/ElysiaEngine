#include "animation_manifest_loader.h"

#include "../json/json_duplicate_key_checker.h"
#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<AnimationManifest,ManifestLoadFailure> AnimationManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::source_location origin = std::source_location::current())
        -> std::expected<AnimationManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),std::move(key),manifest_path,origin));
    };
    if (has_duplicate_json_object_key(manifest_path))
        return fail(ManifestLoadError::DuplicateKey,
            "Load animation manifest failed: duplicate JSON object key.");
    JsonLoader loader;
    const JsonReadResult read = loader.open_file(manifest_path);
    if (!read)
        return fail(ManifestLoadError::OpenFailed,"Load animation manifest failed: " + read.error);
    if (!loader.root().is_object() || !loader.root().contains("animations")
        || !loader.root().at("animations").is_array())
        return fail(ManifestLoadError::MissingField,
            "Load animation manifest failed: animations is missing or not an array.");

    AnimationManifest manifest;
    std::size_t index = 0;
    for (const json& node : loader.root().at("animations"))
    {
        if (!node.is_object() || !node.contains("key") || !node.at("key").is_string()
            || !node.contains("path") || !node.at("path").is_string()
            || !node.contains("frame_count") || !node.at("frame_count").is_number_integer()
            || !node.contains("fps") || !node.at("fps").is_number()
            || !node.contains("loop") || !node.at("loop").is_boolean())
            return fail(ManifestLoadError::InvalidSchema,
                "Load animation manifest failed: invalid animation entry.");
        if (node.contains("horizontal_strip") && !node.at("horizontal_strip").is_boolean())
            return fail(ManifestLoadError::InvalidField,
                "Load animation manifest failed: horizontal_strip is not a boolean.");
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "key" && field.key() != "path"
                && field.key() != "frame_count" && field.key() != "fps"
                && field.key() != "loop" && field.key() != "horizontal_strip"
                && field.key() != "frame_prefix")
                return fail(ManifestLoadError::UnknownField,
                    "Load animation manifest failed: unknown field: " + field.key());

        AnimationManifestEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.source_path = node.at("path").get<std::string>();
        const int frame_count = node.at("frame_count").get<int>();
        if (frame_count <= 0)
            return fail(ManifestLoadError::InvalidValue,
                "Load animation manifest failed: frame_count must be positive.",entry.key);
        entry.frame_count = static_cast<std::size_t>(frame_count);
        entry.fps = node.at("fps").get<double>();
        entry.loop = node.at("loop").get<bool>();
        entry.horizontal_strip = node.value("horizontal_strip",false);
        const bool has_prefix = node.contains("frame_prefix");
        if (has_prefix)
        {
            if (!node.at("frame_prefix").is_string())
                return fail(ManifestLoadError::InvalidField,
                    "Load animation manifest failed: frame_prefix is not a string.",entry.key);
            entry.frame_prefix = node.at("frame_prefix").get<std::string>();
        }
        if (entry.key.empty() || entry.source_path.empty() || entry.fps <= 0.0)
            return fail(ManifestLoadError::InvalidValue,
                "Load animation manifest failed: invalid animation values.",entry.key);
        std::string key_error;
        if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key,key_error))
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load animation manifest failed: " + key_error,entry.key);
        if ((entry.horizontal_strip && has_prefix)
            || (!entry.horizontal_strip && (!has_prefix || entry.frame_prefix.empty())))
            return fail(ManifestLoadError::InvalidField,
                "Load animation manifest failed: frame_prefix is required only for frame_directory.",entry.key);
        if (!entry.frame_prefix.empty() && (entry.frame_prefix.contains('/')
            || entry.frame_prefix.contains('\\') || entry.frame_prefix.contains("..")))
            return fail(ManifestLoadError::InvalidValue,
                "Load animation manifest failed: frame_prefix must not be a path.",entry.key);
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,"/animations/" + std::to_string(index++),{},"animations",{},entry.key);
        manifest.animations.push_back(std::move(entry));
    }
    return manifest;
}
}
