#pragma once

#include <filesystem>

namespace elysia::io
{
[[nodiscard]] bool has_duplicate_json_object_key(const std::filesystem::path& path);
}
