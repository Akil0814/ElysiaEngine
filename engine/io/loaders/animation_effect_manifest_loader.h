#pragma once

#include "asset_config_types.h"

#include <filesystem>

namespace elysia::io
{
class AnimationEffectManifestLoader
{
public:
	bool load(
		const std::filesystem::path& manifest_path,
		AnimationEffectManifest& manifest
	) const;
};
}
