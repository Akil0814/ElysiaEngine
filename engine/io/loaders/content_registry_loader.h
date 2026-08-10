#pragma once

#include "asset_config_types.h"

#include <filesystem>

namespace elysia::io
{
class ContentRegistryLoader
{
public:
	bool load(
		const std::filesystem::path& content_registry_path,
		ContentRegistry& content_registry
	) const;
};
}
