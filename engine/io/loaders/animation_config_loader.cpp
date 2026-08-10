#include "../../resources/pipeline/filesystem_segment_formatter.h"
#include "../../resources/pipeline/resource_key_builder.h"
#include "../../tools/logger.h"
#include "animation_config_loader.h"
#include "../json/json_duplicate_key_checker.h"

#include <string>
#include <utility>

namespace elysia::io
{
namespace
{
bool has_only_fields(const json& node, std::initializer_list<const char*> fields, const std::string& label)
{
	for (auto item = node.begin(); item != node.end(); ++item)
	{
		bool known = false;
		for (const char* field : fields) known = known || item.key() == field;
		if (!known)
		{
			ELYSIA_LOG_WARN("io", "Load animation config failed: unknown " << label << " field: " << item.key());
			return false;
		}
	}
	return true;
}

std::string escape_json_pointer(std::string value)
{
	size_t position = 0;
	while ((position = value.find('~', position)) != std::string::npos) { value.replace(position, 1, "~0"); position += 2; }
	position = 0;
	while ((position = value.find('/', position)) != std::string::npos) { value.replace(position, 1, "~1"); position += 2; }
	return value;
}
}

bool AnimationConfigLoader::load(
	const std::filesystem::path& animation_config_path,
	const AnimationLayout& layout,
	AnimationConfig& config) const
{
	config = {};
	if (has_duplicate_json_object_key(animation_config_path)) return false;
	JsonLoader loader;
	const JsonReadResult result = loader.open_file(animation_config_path);
	if (!result || !loader.root().is_object())
	{
		ELYSIA_LOG_WARN("io", "Load animation config failed: " << (result ? "root is not an object" : result.error));
		return false;
	}
	const json& root = loader.root();
	if (!has_only_fields(root, {"defaults", "animations"}, "root")
		|| root.size() != 2
		|| !root.contains("defaults") || !root.at("defaults").is_object()
		|| !root.contains("animations") || !root.at("animations").is_object())
	{
		ELYSIA_LOG_WARN("io", "Load animation config failed: defaults and animations are required: " << animation_config_path);
		return false;
	}
	const json& defaults = root.at("defaults");
	if (!has_only_fields(defaults, {"source_type"}, "defaults")
		|| defaults.size() != 1 || !defaults.contains("source_type") || !defaults.at("source_type").is_string())
	{
		ELYSIA_LOG_WARN("io", "Load animation config failed: defaults.source_type is required.");
		return false;
	}
	const std::string source_type = defaults.at("source_type").get<std::string>();
	if (source_type == "frame_directory") config.source_type = AnimationSourceType::FrameDirectory;
	else if (source_type == "horizontal_strip") config.source_type = AnimationSourceType::HorizontalStrip;
	else
	{
		ELYSIA_LOG_WARN("io", "Load animation config failed: unsupported source_type: " << source_type);
		return false;
	}

	std::string key_error;
	for (auto animation = root.at("animations").begin(); animation != root.at("animations").end(); ++animation)
	{
		if (!elysia::resources::ResourceKeyBuilder::validate_component(animation.key(), key_error)
			|| !animation.value().is_object())
		{
			ELYSIA_LOG_WARN("io", "Load animation config failed: invalid animation name or entry: " << animation.key());
			return false;
		}
		const std::string pointer = "/animations/" + escape_json_pointer(animation.key());
		const json& node = animation.value();
		if (node.contains("segments"))
		{
			if (!has_only_fields(node, {"segments"}, "segmented animation")
				|| !node.at("segments").is_array() || node.at("segments").empty() || node.at("segments").size() > 100)
			{
				ELYSIA_LOG_WARN("io", "Load animation config failed: segments must contain 1-100 entries: " << animation.key());
				return false;
			}
			for (size_t index = 0; index < node.at("segments").size(); ++index)
			{
				const json& segment = node.at("segments").at(index);
				if (!segment.is_object() || !append_clip(animation_config_path,
					pointer + "/segments/" + std::to_string(index), animation.key(), true, index,
					segment, layout, config)) return false;
			}
		}
		else if (!append_clip(animation_config_path, pointer, animation.key(), false, 0, node, layout, config))
			return false;
	}
	return true;
}

std::filesystem::path AnimationConfigLoader::resolve_clip_path(
	const std::string& animation_name,
	bool is_segment,
	size_t segment_index,
	const AnimationLayout& layout) const
{
	const auto iterator = layout.animations.find(animation_name);
	if (iterator == layout.animations.end())
	{
		ELYSIA_LOG_WARN("io", "Load animation clip failed: layout entry does not exist: " << animation_name);
		return {};
	}
	const AnimationLayoutEntry& entry = iterator->second;
	if (!is_segment)
	{
		if (!entry.has_path) return {};
		return entry.path;
	}
	if (!entry.has_segment_path) return {};
	std::string segment;
	if (!elysia::resources::format_filesystem_segment(segment_index, segment)) return {};
	std::string path = entry.segment_path.generic_string();
	const size_t marker = path.find("{segment}");
	if (marker == std::string::npos) return (entry.segment_path / segment).lexically_normal();
	path.replace(marker, std::string("{segment}").size(), segment);
	return std::filesystem::path(path).lexically_normal();
}

bool AnimationConfigLoader::append_clip(
	const std::filesystem::path& config_path,
	const std::string& json_pointer,
	const std::string& animation_name,
	bool is_segment,
	size_t segment_index,
	const json& clip_node,
	const AnimationLayout& layout,
	AnimationConfig& config) const
{
	if (!has_only_fields(clip_node, {"frame_count", "fps", "loop"}, "clip")
		|| clip_node.size() != 3
		|| !clip_node.contains("frame_count") || !clip_node.at("frame_count").is_number_integer()
		|| !clip_node.contains("fps") || !clip_node.at("fps").is_number()
		|| !clip_node.contains("loop") || !clip_node.at("loop").is_boolean())
	{
		ELYSIA_LOG_WARN("io", "Load animation clip failed: frame_count, fps and loop are required: " << animation_name);
		return false;
	}
	const int frame_count = clip_node.at("frame_count").get<int>();
	const double fps = clip_node.at("fps").get<double>();
	if (frame_count <= 0 || fps <= 0.0) return false;
	const auto path = resolve_clip_path(animation_name, is_segment, segment_index, layout);
	if (path.empty()) return false;
	AnimationClipConfig clip;
	clip.animation_name = animation_name;
	clip.path = path;
	clip.frame_count = static_cast<size_t>(frame_count);
	clip.fps = fps;
	clip.loop = clip_node.at("loop").get<bool>();
	clip.is_segment = is_segment;
	clip.segment_index = segment_index;
	clip.origin = elysia::resources::make_resource_origin(
		config_path, json_pointer, {}, "animations", {}, animation_name,
		is_segment ? std::optional<size_t>(segment_index) : std::nullopt);
	config.clips.push_back(std::move(clip));
	return true;
}
}
