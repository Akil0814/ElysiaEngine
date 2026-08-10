#include "../../tools/logger.h"
#include "audio_manifest_loader.h"

#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"
#include "../../resources/pipeline/resource_key_builder.h"
#include <string>
#include <utility>

namespace elysia::io
{
namespace
{
bool append_audio_entries(
	const json& node,
	const char* group_name,
	const std::filesystem::path& manifest_path,
	std::vector<AudioManifestEntry>& out_entries
)
{
	if (!node.is_object())
	{
		ELYSIA_LOG_WARN("io","Load audio manifest failed: " << group_name
			<< " is not an object.");
		return false;
	}

	for (json::const_iterator item = node.begin(); item != node.end(); ++item)
	{
		std::string key_error;
		if (!elysia::resources::ResourceKeyBuilder::validate_key(item.key(), key_error))
		{
			ELYSIA_LOG_WARN("io", "Load audio manifest failed: " << key_error);
			return false;
		}
		if (!item.value().is_object())
		{
			ELYSIA_LOG_WARN("io","Load audio manifest failed: entry is not an object: "
				<< item.key());
			return false;
		}

		const json& entry_node = item.value();
		if (!entry_node.contains("path") || !entry_node.at("path").is_string())
		{
			ELYSIA_LOG_WARN("io","Load audio manifest failed: path is missing or not a string: "
				<< item.key());
			return false;
		}

		AudioManifestEntry entry;
		entry.key = item.key();
		entry.file_path = entry_node.at("path").get<std::string>();
		for (auto field = entry_node.begin(); field != entry_node.end(); ++field)
			if (field.key() != "path") return false;
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, "/" + std::string(group_name) + "/" + item.key(), {}, group_name, {}, item.key());
		out_entries.push_back(std::move(entry));
	}

	return true;
}
}

bool AudioManifestLoader::load(
	const std::filesystem::path& manifest_path,
	AudioManifest& manifest
) const
{
	manifest = AudioManifest{};
	if (has_duplicate_json_object_key(manifest_path)) return false;

	JsonLoader loader;
	JsonReadResult result = loader.open_file(manifest_path);
	if (!result)
	{
		ELYSIA_LOG_WARN("io","Load audio manifest failed: " << result.error);
		return false;
	}

	if (!loader.root().is_object())
	{
		ELYSIA_LOG_WARN("io","Load audio manifest failed: root is not an object: "
			<< manifest_path);
		return false;
	}

	if (!loader.root().contains("sounds"))
	{
		ELYSIA_LOG_WARN("io","Load audio manifest failed: sounds is missing: "
			<< manifest_path);
		return false;
	}

	if (!loader.root().contains("music"))
	{
		ELYSIA_LOG_WARN("io","Load audio manifest failed: music is missing: "
			<< manifest_path);
		return false;
	}

	AudioManifest parsed_manifest;
	if (!append_audio_entries(loader.root().at("sounds"), "sounds", manifest_path, parsed_manifest.sounds))
		return false;

	if (!append_audio_entries(loader.root().at("music"), "music", manifest_path, parsed_manifest.music))
		return false;

	manifest = std::move(parsed_manifest);
	return true;
}

}
