#include "resource_key_builder.h"
#include "../../core/validation/dotted_key_validator.h"

#include <utility>

namespace elysia::resources
{
bool ResourceKeyBuilder::validate_component(std::string_view component, std::string& error)
{
	return elysia::core::DottedKeyValidator::validate_component(component,error);
}

bool ResourceKeyBuilder::validate_key(std::string_view key, std::string& error)
{
	return elysia::core::DottedKeyValidator::validate_key(key,error);
}

bool ResourceKeyBuilder::build(
	std::string_view entity_id,
	std::string_view key_namespace,
	const std::vector<std::string>& logical_components,
	std::optional<size_t> segment_index,
	std::string& key,
	std::string& error)
{
	key.clear();
	if (!validate_component(entity_id, error)) return false;
	if (!key_namespace.empty() && !validate_component(key_namespace, error)) return false;
	if (logical_components.empty())
	{
		error = "resource key requires at least one logical component";
		return false;
	}
	key.assign(entity_id);
	if (!key_namespace.empty()) key += "." + std::string(key_namespace);
	for (const std::string& component : logical_components)
	{
		if (!validate_component(component, error)) return false;
		key += "." + component;
	}
	if (segment_index)
	{
		if (*segment_index > 99)
		{
			error = "resource segment index exceeds 99";
			return false;
		}
		key += "." + std::to_string(*segment_index);
	}
	return true;
}

bool ResourceKeyBuilder::append_component(
	std::string_view base_key,
	std::string_view component,
	std::string& key,
	std::string& error)
{
	if (!validate_key(base_key, error) || !validate_component(component, error)) return false;
	key = std::string(base_key) + "." + std::string(component);
	return true;
}
}
