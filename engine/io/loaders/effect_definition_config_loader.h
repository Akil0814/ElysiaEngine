#pragma once

#include "asset_config_types.h"

#include <filesystem>

namespace elysia::io
{
class EffectDefinitionConfigLoader
{
public:
	bool load(
		const std::filesystem::path& config_path,
		const AnimationConfig& animation_config,
		EffectDefinitionConfig& config
	) const;
};
}
