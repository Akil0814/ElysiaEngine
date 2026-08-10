#include "fonts_manifest_loader.h"

#include "../json/json_duplicate_key_checker.h"
#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<FontManifest,ManifestLoadFailure> FontsManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::source_location origin = std::source_location::current())
        -> std::expected<FontManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),std::move(key),manifest_path,origin));
    };
    if (has_duplicate_json_object_key(manifest_path))
        return fail(ManifestLoadError::DuplicateKey,
            "Load fonts manifest failed: duplicate JSON object key.");
    JsonLoader loader;
    const JsonReadResult read = loader.open_file(manifest_path);
    if (!read)
        return fail(ManifestLoadError::OpenFailed,
            "Load fonts manifest failed: " + read.error);
    if (!loader.root().is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load fonts manifest failed: root is not an object.");
    if (!loader.root().contains("fonts") || !loader.root().at("fonts").is_array())
        return fail(ManifestLoadError::MissingField,
            "Load fonts manifest failed: fonts is missing or not an array.");
    for (auto field = loader.root().begin();field != loader.root().end();++field)
        if (field.key() != "fonts")
            return fail(ManifestLoadError::UnknownField,
                "Load fonts manifest failed: unknown root field: " + field.key());

    FontManifest manifest;
    std::size_t index = 0;
    for (const json& node : loader.root().at("fonts"))
    {
        if (!node.is_object())
            return fail(ManifestLoadError::InvalidSchema,
                "Load fonts manifest failed: font entry is not an object.");
        if (!node.contains("key") || !node.at("key").is_string()
            || !node.contains("file") || !node.at("file").is_string())
            return fail(ManifestLoadError::MissingField,
                "Load fonts manifest failed: key or file is missing or invalid.");
        FontManifestEntry entry;
        entry.key = node.at("key").get<std::string>();
        entry.file_path = node.at("file").get<std::string>();
        if (entry.key.empty() || entry.file_path.empty())
            return fail(ManifestLoadError::InvalidValue,
                "Load fonts manifest failed: font key and file must not be empty.",entry.key);
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "key" && field.key() != "file")
                return fail(ManifestLoadError::UnknownField,
                    "Load fonts manifest failed: unknown field: " + field.key(),entry.key);
        std::string key_error;
        if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key,key_error))
            return fail(ManifestLoadError::InvalidResourceKey,
                "Load fonts manifest failed: " + key_error,entry.key);
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,"/fonts/" + std::to_string(index++),{},"fonts",{},entry.key);
        manifest.fonts.push_back(std::move(entry));
    }
    if (manifest.fonts.empty())
        return fail(ManifestLoadError::MissingContent,
            "Load fonts manifest failed: fonts must not be empty.");
    return manifest;
}
}
