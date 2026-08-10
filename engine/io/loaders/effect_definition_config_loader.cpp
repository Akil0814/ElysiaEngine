#include "../../resources/pipeline/resource_key_builder.h"
#include "../../tools/logger.h"
#include "effect_definition_config_loader.h"

#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"

#include <algorithm>
#include <utility>

namespace elysia::io
{
bool EffectDefinitionConfigLoader::load(
	const std::filesystem::path& config_path,
	const AnimationConfig& animation_config,
	EffectDefinitionConfig& config) const
{
	config = {};
	if (has_duplicate_json_object_key(config_path))
	{
		ELYSIA_LOG_WARN("io", "Load effect definition config failed: duplicate object key: " << config_path);
		return false;
	}
	JsonLoader loader;
	if (!loader.open_file(config_path) || !loader.root().is_object()
		|| loader.root().size() != 1
		|| !loader.root().contains("effects") || !loader.root().at("effects").is_object())
	{
		ELYSIA_LOG_WARN("io", "Load effect definition config failed: effects is missing or invalid: " << config_path);
		return false;
	}
	std::string key_error;
	EffectDefinitionConfig parsed;
	for (auto item = loader.root().at("effects").begin(); item != loader.root().at("effects").end(); ++item)
	{
		if (!elysia::resources::ResourceKeyBuilder::validate_component(item.key(), key_error)
			|| !item.value().is_object()) return false;
		const json& node = item.value();
		for (auto field = node.begin(); field != node.end(); ++field)
			if (field.key() != "animation" && field.key() != "default_width"
				&& field.key() != "default_height" && field.key() != "default_angle_degrees")
				return false;
		if (!node.contains("animation") || !node.at("animation").is_string()) return false;
		const std::string animation_name = node.at("animation").get<std::string>();
		if (!elysia::resources::ResourceKeyBuilder::validate_component(animation_name, key_error)) return false;
		float width = 0.0f;
		float height = 0.0f;
		double angle = 0.0;
		if (node.contains("default_width"))
		{
			if (!node.at("default_width").is_number()) return false;
			width = node.at("default_width").get<float>();
		}
		if (node.contains("default_height"))
		{
			if (!node.at("default_height").is_number()) return false;
			height = node.at("default_height").get<float>();
		}
		if (node.contains("default_angle_degrees"))
		{
			if (!node.at("default_angle_degrees").is_number()) return false;
			angle = node.at("default_angle_degrees").get<double>();
		}
		if ((width == 0.0f) != (height == 0.0f) || width < 0.0f || height < 0.0f)
		{
			ELYSIA_LOG_WARN("io", "Load effect definition config failed: width and height must both be zero or positive: " << item.key());
			return false;
		}
		bool matched = false;
		for (const AnimationClipConfig& clip : animation_config.clips)
		{
			if (clip.animation_name != animation_name) continue;
			matched = true;
			EffectDefinitionConfigEntry entry;
			entry.effect_name = item.key();
			entry.animation_name = animation_name;
			entry.is_segment = clip.is_segment;
			entry.segment_index = clip.segment_index;
			entry.default_width = width;
			entry.default_height = height;
			entry.default_angle_degrees = angle;
			entry.origin = elysia::resources::make_resource_origin(
				config_path, "/effects/" + item.key(), {}, "effects", {}, item.key(),
				clip.is_segment ? std::optional<size_t>(clip.segment_index) : std::nullopt);
			parsed.effects.push_back(std::move(entry));
		}
		if (!matched)
		{
			ELYSIA_LOG_WARN("io", "Load effect definition config failed: animation does not exist: " << animation_name);
			return false;
		}
	}
	config = std::move(parsed);
	return true;
}
}
