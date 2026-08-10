#include "../../tools/logger.h"
#include "entity_manifest_loader.h"

#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"
#include "../../resources/pipeline/resource_key_builder.h"

#include <unordered_map>

namespace elysia::io
{
bool EntityManifestLoader::load(const std::filesystem::path& manifest_path, EntityManifest& manifest) const
{
	manifest = EntityManifest{};
	if (has_duplicate_json_object_key(manifest_path)) return false;
	JsonLoader loader;
	if (!loader.open_file(manifest_path) || !loader.root().is_object() || loader.root().size() != 1
		|| !loader.root().contains("entities") || !loader.root().at("entities").is_array())
	{
		ELYSIA_LOG_WARN("io", "Load entity manifest failed: entities is missing or invalid: " << manifest_path);
		return false;
	}

	EntityManifest parsed;
	std::unordered_map<std::string, elysia::resources::ResourceOrigin> entity_origins;
	size_t entity_index = 0;
	for (const json& node : loader.root().at("entities"))
	{
		const std::string pointer = "/entities/" + std::to_string(entity_index++);
		if (!node.is_object() || !node.contains("id") || !node.at("id").is_string())
		{
			ELYSIA_LOG_WARN("io", "Load entity manifest failed: entity id is missing or invalid.");
			return false;
		}
		if (node.contains("enabled") && !node.at("enabled").is_boolean())
		{
			ELYSIA_LOG_WARN("io", "Load entity manifest failed: enabled is invalid: " << node.at("id"));
			return false;
		}
		for (auto field = node.begin(); field != node.end(); ++field)
			if (field.key() != "id" && field.key() != "enabled"
				&& field.key() != "animation_layout")
			{
				ELYSIA_LOG_WARN("io", "Load entity manifest failed: unknown field: " << field.key());
				return false;
			}
		EntityManifestEntry entry;
		entry.id = node.at("id").get<std::string>();
		if (node.contains("animation_layout"))
		{
			if (!node.at("animation_layout").is_string())
			{
				ELYSIA_LOG_WARN("io", "Load entity manifest failed: animation_layout is invalid: " << entry.id);
				return false;
			}
			entry.animation_layout = node.at("animation_layout").get<std::string>();
		}
		std::string key_error;
		if (!elysia::resources::ResourceKeyBuilder::validate_component(entry.id, key_error)
			|| (!entry.animation_layout.empty()
				&& !elysia::resources::ResourceKeyBuilder::validate_component(entry.animation_layout, key_error)))
		{
			ELYSIA_LOG_WARN("io", "Load entity manifest failed: " << key_error);
			return false;
		}
		entry.origin = elysia::resources::make_resource_origin(
			manifest_path, pointer, {}, "entities", entry.id, entry.id);
		const auto [first, inserted] = entity_origins.emplace(entry.id, entry.origin);
		if (!inserted)
		{
			ELYSIA_LOG_WARN("io", "Load entity manifest failed: duplicate entity id: " << entry.id
				<< "\n  first:  " << first->second.describe()
				<< "\n  second: " << entry.origin.describe());
			return false;
		}
		if (node.value("enabled", true) == false)
			continue;
		parsed.entities.push_back(std::move(entry));
	}

	manifest = std::move(parsed);
	return true;
}
}
