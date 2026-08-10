#include "../../tools/logger.h"
#include "texture_manifest_loader.h"

#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"
#include "../../resources/pipeline/resource_key_builder.h"
#include <utility>

namespace elysia::io
{
bool TextureManifestLoader::load(
	const std::filesystem::path& manifest_path,
	TextureManifest& manifest
) const
{
	manifest = TextureManifest{};
	if (has_duplicate_json_object_key(manifest_path)) return false;

	JsonLoader loader;
	JsonReadResult result = loader.open_file(manifest_path);
	if (!result)
	{
		ELYSIA_LOG_WARN("io","Load texture manifest failed: " << result.error);
		return false;
	}

	if (!loader.root().is_object())
	{
		ELYSIA_LOG_WARN("io","Load texture manifest failed: root is not an object: "
			<< manifest_path);
		return false;
	}

	if (!loader.root().contains("textures") || !loader.root().at("textures").is_object())
	{
		ELYSIA_LOG_WARN("io","Load texture manifest failed: textures is missing or not an object: "
			<< manifest_path);
		return false;
	}

	TextureManifest parsed_manifest;
	const json& textures = loader.root().at("textures");
	for (json::const_iterator texture = textures.begin();
		texture != textures.end();
		++texture)
	{
		std::string key_error;
		if (!elysia::resources::ResourceKeyBuilder::validate_key(texture.key(), key_error))
		{
			ELYSIA_LOG_WARN("io", "Load texture manifest failed: " << key_error);
			return false;
		}
		if (!texture.value().is_object())
		{
			ELYSIA_LOG_WARN("io","Load texture manifest failed: texture entry is not an object: "
				<< texture.key());
			return false;
		}

		const json& texture_node = texture.value();
		if (!texture_node.contains("path") || !texture_node.at("path").is_string())
		{
			ELYSIA_LOG_WARN("io","Load texture manifest failed: path is missing or not a string: "
				<< texture.key());
			return false;
		}

		TextureManifestEntry entry;
		entry.key = texture.key();
		entry.file_path = texture_node.at("path").get<std::string>();
		for (auto field = texture_node.begin(); field != texture_node.end(); ++field)
			if (field.key() != "path") return false;
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, "/textures/" + texture.key(), {}, "textures", {}, texture.key());
		parsed_manifest.textures.push_back(std::move(entry));
	}

	manifest = std::move(parsed_manifest);
	return true;
}

}
