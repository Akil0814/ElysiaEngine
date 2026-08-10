#pragma once

#include "asset_config_types.h"
#include <filesystem>

namespace elysia::io
{
class AnimationLayoutLoader
{
public:
	bool load(const std::filesystem::path& layout_path, AnimationLayout& layout) const;
};
}
