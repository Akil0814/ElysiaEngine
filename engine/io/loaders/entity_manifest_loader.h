#pragma once

#include "asset_config_types.h"

#include <filesystem>

namespace elysia::io
{
class EntityManifestLoader
{
public:
	bool load(const std::filesystem::path& manifest_path, EntityManifest& manifest) const;
};
}
