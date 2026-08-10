#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace elysia::resources
{
class ResourceKeyBuilder
{
public:
	[[nodiscard]] static bool validate_component(std::string_view component, std::string& error);
	[[nodiscard]] static bool validate_key(std::string_view key, std::string& error);
	[[nodiscard]] static bool build(
		std::string_view entity_id,
		std::string_view key_namespace,
		const std::vector<std::string>& logical_components,
		std::optional<size_t> segment_index,
		std::string& key,
		std::string& error
	);
	[[nodiscard]] static bool append_component(
		std::string_view base_key,
		std::string_view component,
		std::string& key,
		std::string& error
	);
};
}
