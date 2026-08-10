#pragma once

#include "json_loader.h"

#include <expected>
#include <filesystem>
#include <string>

namespace elysia::io
{
[[nodiscard]] std::expected<json,std::string> load_strict_json(
    const std::filesystem::path& path);
}
