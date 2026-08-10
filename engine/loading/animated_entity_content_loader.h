#pragma once

#include "../io/loaders/asset_config_types.h"

#include <filesystem>
#include <string>

namespace elysia::loading
{
class AnimatedEntityContentLoader
{
public:
	bool load(
		const std::string& module_name,
		const std::filesystem::path& module_manifest_path,
		elysia::io::EntityContentModule& content,
		std::string& error_message
	) const;
};
}
