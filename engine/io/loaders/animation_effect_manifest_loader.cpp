#include "../../tools/logger.h"
#include "animation_effect_manifest_loader.h"

#include "../json/json_duplicate_key_checker.h"
#include "../json/json_loader.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <utility>

namespace elysia::io
{
bool AnimationEffectManifestLoader::load(
	const std::filesystem::path& manifest_path,
	AnimationEffectManifest& manifest
) const
{
	manifest = AnimationEffectManifest{};
	if (has_duplicate_json_object_key(manifest_path))
	{
		ELYSIA_LOG_WARN("io", "Load effect manifest failed: duplicate JSON object key: " << manifest_path);
		return false;
	}

	JsonLoader loader;
	JsonReadResult result = loader.open_file(manifest_path);
	if (!result)
	{
		ELYSIA_LOG_WARN("io","Load effect manifest failed: " << result.error);
		return false;
	}

	if (!loader.root().is_object()
		|| !loader.root().contains("effects")
		|| !loader.root().at("effects").is_array())
	{
		ELYSIA_LOG_WARN("io","Load effect manifest failed: effects is missing or not an array: "
			<< manifest_path);
		return false;
	}

	AnimationEffectManifest parsed_manifest;
	size_t effect_index = 0;
	for (const json& effect_node : loader.root().at("effects"))
	{
		const size_t current_index = effect_index++;
		if (!effect_node.is_object()
			|| !effect_node.contains("key") || !effect_node.at("key").is_string()
			|| !effect_node.contains("animation_key") || !effect_node.at("animation_key").is_string())
		{
			ELYSIA_LOG_WARN("io","Load effect manifest failed: invalid effect entry: "
				<< manifest_path);
			return false;
		}

		AnimationEffectManifestEntry entry;
		entry.key = effect_node.at("key").get<std::string>();
		entry.animation_key = effect_node.at("animation_key").get<std::string>();
		if (effect_node.contains("default_width"))
		{
			if (!effect_node.at("default_width").is_number()) return false;
			entry.default_width = effect_node.at("default_width").get<float>();
		}
		if (effect_node.contains("default_height"))
		{
			if (!effect_node.at("default_height").is_number()) return false;
			entry.default_height = effect_node.at("default_height").get<float>();
		}
		if (effect_node.contains("default_angle_degrees"))
		{
			if (!effect_node.at("default_angle_degrees").is_number()) return false;
			entry.default_angle_degrees = effect_node.at("default_angle_degrees").get<double>();
		}
		if (entry.default_width < 0.0f || entry.default_height < 0.0f
			|| ((entry.default_width == 0.0f) != (entry.default_height == 0.0f)))
		{
			ELYSIA_LOG_WARN("io", "Load effect manifest failed: default size must provide positive width and height: "
				<< manifest_path);
			return false;
		}
		for (auto field = effect_node.begin(); field != effect_node.end(); ++field)
			if (field.key() != "key" && field.key() != "animation_key"
				&& field.key() != "default_width" && field.key() != "default_height"
				&& field.key() != "default_angle_degrees") return false;
		if (entry.key.empty() || entry.animation_key.empty())
		{
			ELYSIA_LOG_WARN("io","Load effect manifest failed: empty effect values: "
				<< manifest_path);
			return false;
		}

		std::string key_error;
		if (!elysia::resources::ResourceKeyBuilder::validate_key(entry.key, key_error)
			|| !elysia::resources::ResourceKeyBuilder::validate_key(entry.animation_key, key_error))
		{
			ELYSIA_LOG_WARN("io", "Load effect manifest failed: " << key_error);
			return false;
		}
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, "/effects/" + std::to_string(current_index), {}, "effects", {}, entry.key);

		parsed_manifest.effects.push_back(std::move(entry));
	}

	manifest = std::move(parsed_manifest);
	return true;
}
}
