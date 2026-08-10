#pragma once

#include "asset_config_types.h"
#include "../json/json_loader.h"

#include <cstddef>
#include <filesystem>
#include <string>

namespace elysia::io
{
class AnimationConfigLoader
{
public:
	bool load(
		const std::filesystem::path& animation_config_path,
		const AnimationLayout& layout,
		AnimationConfig& config
	) const;

private:
	std::filesystem::path resolve_clip_path(
		const std::string& animation_name,
		bool is_segment,
		size_t segment_index,
		const AnimationLayout& layout
	) const;

	bool append_clip(
		const std::filesystem::path& config_path,
		const std::string& json_pointer,
		const std::string& animation_name,
		bool is_segment,
		size_t segment_index,
		const json& clip_node,
		const AnimationLayout& layout,
		AnimationConfig& config
	) const;
};
}
