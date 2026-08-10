#include "texture_manifest_loader.h"

#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
std::expected<TextureManifest,ManifestLoadFailure> TextureManifestLoader::load(
    const std::filesystem::path& manifest_path) const
{
    const auto fail = [&manifest_path](ManifestLoadError error,std::string message,
        std::string key = {},std::source_location origin = std::source_location::current())
        -> std::expected<TextureManifest,ManifestLoadFailure>
    {
        return std::unexpected(make_manifest_load_failure(
            error,std::move(message),std::move(key),manifest_path,origin));
    };
    if (has_duplicate_json_object_key(manifest_path))
        return fail(ManifestLoadError::DuplicateKey,
            "Load texture manifest failed: duplicate JSON object key.");

    JsonLoader loader;
    const JsonReadResult read = loader.open_file(manifest_path);
    if (!read)
        return fail(ManifestLoadError::OpenFailed,
            "Load texture manifest failed: " + read.error);
    if (!loader.root().is_object())
        return fail(ManifestLoadError::InvalidSchema,
            "Load texture manifest failed: root is not an object.");
    if (!loader.root().contains("textures") || !loader.root().at("textures").is_object())
        return fail(ManifestLoadError::MissingField,
            "Load texture manifest failed: textures is missing or not an object.");

    TextureManifest manifest;
    for (auto texture = loader.root().at("textures").begin();
        texture != loader.root().at("textures").end();++texture)
    {
        if (auto key_result = elysia::resources::ResourceKeyBuilder::validate_key(texture.key());
            !key_result)
            return fail(ManifestLoadError::InvalidValue,
                "Load texture manifest failed: " + key_result.error().message,texture.key());
        if (!texture.value().is_object())
            return fail(ManifestLoadError::InvalidSchema,
                "Load texture manifest failed: texture entry is not an object.",texture.key());
        const json& node = texture.value();
        if (!node.contains("path") || !node.at("path").is_string())
            return fail(ManifestLoadError::MissingField,
                "Load texture manifest failed: path is missing or not a string.",texture.key());
        for (auto field = node.begin();field != node.end();++field)
            if (field.key() != "path")
                return fail(ManifestLoadError::UnknownField,
                    "Load texture manifest failed: unknown field: " + field.key(),texture.key());

        TextureManifestEntry entry;
        entry.key = texture.key();
        entry.file_path = node.at("path").get<std::string>();
        entry.origin = elysia::resources::make_resource_origin(
            manifest_path,"/textures/" + texture.key(),{},"textures",{},texture.key());
        manifest.textures.push_back(std::move(entry));
    }
    return manifest;
}
}
