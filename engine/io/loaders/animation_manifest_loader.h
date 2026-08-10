#pragma once

#include "asset_config_types.h"

#include <filesystem>

namespace elysia::io
{
class AnimationManifestLoader
{
public:
	bool load(
		const std::filesystem::path& manifest_path,
		AnimationManifest& manifest
	) const;
};
}
