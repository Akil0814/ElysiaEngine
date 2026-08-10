#include "../../tools/logger.h"
#include "animation_manifest_loader.h"

#include "../json/json_duplicate_key_checker.h"
#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"
#include <utility>

namespace elysia::io
{
bool AnimationManifestLoader::load(
	const std::filesystem::path& manifest_path,
	AnimationManifest& manifest
) const
{
	manifest = AnimationManifest{};
	if (has_duplicate_json_object_key(manifest_path))
	{
		ELYSIA_LOG_WARN("io", "Load animation manifest failed: duplicate JSON object key: " << manifest_path);
		return false;
	}

	JsonLoader loader;
	JsonReadResult result = loader.open_file(manifest_path);
	if (!result)
	{
		ELYSIA_LOG_WARN("io","Load animation manifest failed: " << result.error);
		return false;
	}

	if (!loader.root().is_object()
		|| !loader.root().contains("animations")
		|| !loader.root().at("animations").is_array())
	{
		ELYSIA_LOG_WARN("io","Load animation manifest failed: animations is missing or not an array: "
			<< manifest_path);
		return false;
	}

	AnimationManifest parsed_manifest;
	size_t animation_index = 0;
	for (const json& animation_node : loader.root().at("animations"))
	{
		const size_t current_index = animation_index++;
		if (!animation_node.is_object()
			|| !animation_node.contains("key") || !animation_node.at("key").is_string()
			|| !animation_node.contains("path") || !animation_node.at("path").is_string()
			|| !animation_node.contains("frame_count") || !animation_node.at("frame_count").is_number_integer()
			|| !animation_node.contains("fps") || !animation_node.at("fps").is_number()
			|| !animation_node.contains("loop") || !animation_node.at("loop").is_boolean())
		{
			ELYSIA_LOG_WARN("io","Load animation manifest failed: invalid animation entry: "
				<< manifest_path);
			return false;
		}
		if (animation_node.contains("horizontal_strip")
			&& !animation_node.at("horizontal_strip").is_boolean())
		{
			ELYSIA_LOG_WARN("io","Load animation manifest failed: horizontal_strip is not a boolean: "
				<< manifest_path);
			return false;
		}
		for (auto field = animation_node.begin(); field != animation_node.end(); ++field)
			if (field.key() != "key" && field.key() != "path" && field.key() != "frame_count"
				&& field.key() != "fps" && field.key() != "loop" && field.key() != "horizontal_strip"
				&& field.key() != "frame_prefix")
			{
				ELYSIA_LOG_WARN("io", "Load animation manifest failed: unknown field: " << field.key());
				return false;
			}

		AnimationManifestEntry entry;
		entry.key = animation_node.at("key").get<std::string>();
		entry.source_path = animation_node.at("path").get<std::string>();
		const int frame_count = animation_node.at("frame_count").get<int>();
		if (frame_count <= 0) return false;
		entry.frame_count = static_cast<size_t>(frame_count);
		entry.fps = animation_node.at("fps").get<double>();
		entry.loop = animation_node.at("loop").get<bool>();
		entry.horizontal_strip = animation_node.value("horizontal_strip", false);
		const bool has_frame_prefix = animation_node.contains("frame_prefix");
		if (has_frame_prefix)
		{
			if (!animation_node.at("frame_prefix").is_string()) return false;
			entry.frame_prefix = animation_node.at("frame_prefix").get<std::string>();
		}

		if (entry.key.empty() || entry.source_path.empty() || entry.frame_count == 0 || entry.fps <= 0.0)
		{
			ELYSIA_LOG_WARN("io","Load animation manifest failed: invalid animation values: "
				<< manifest_path);
			return false;
		}

		std::string key_error;
		if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key, key_error))
		{
			ELYSIA_LOG_WARN("io", "Load animation manifest failed: " << key_error);
			return false;
		}
		if ((entry.horizontal_strip && has_frame_prefix)
			|| (!entry.horizontal_strip && (!has_frame_prefix || entry.frame_prefix.empty())))
		{
			ELYSIA_LOG_WARN("io", "Load animation manifest failed: frame_prefix is required only for frame_directory: " << entry.key);
			return false;
		}
		if (!entry.frame_prefix.empty()
			&& (entry.frame_prefix.find('/') != std::string::npos
				|| entry.frame_prefix.find('\\') != std::string::npos
				|| entry.frame_prefix.find("..") != std::string::npos))
		{
			ELYSIA_LOG_WARN("io", "Load animation manifest failed: frame_prefix must not be a path: " << entry.key);
			return false;
		}
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, "/animations/" + std::to_string(current_index), {}, "animations", {}, entry.key);

		parsed_manifest.animations.push_back(std::move(entry));
	}

	manifest = std::move(parsed_manifest);
	return true;
}
}
