#include "animated_entity_content_loader.h"

#include "../io/loaders/animation_config_loader.h"
#include "../io/loaders/animation_layout_loader.h"
#include "../io/loaders/effect_definition_config_loader.h"
#include "../io/loaders/entity_audio_layout_loader.h"
#include "../io/loaders/entity_manifest_loader.h"
#include "../io/loaders/entity_texture_layout_loader.h"
#include "../io/json/json_loader.h"
#include "../io/json/json_duplicate_key_checker.h"
#include "../io/path/path_manager.h"
#include "../resources/pipeline/resource_key_builder.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace elysia::loading
{
namespace
{
bool fail(std::string& error, std::string message)
{
	error = "Entity content module failed: " + std::move(message);
	return false;
}

bool has_only_fields(
	const elysia::io::json& node,
	std::initializer_list<std::string_view> fields,
	std::string_view label,
	std::string& error)
{
	for (auto item = node.begin(); item != node.end(); ++item)
	{
		bool known = false;
		for (const auto field : fields) known = known || item.key() == field;
		if (!known) return fail(error, "unknown " + std::string(label) + " field: " + item.key());
	}
	return true;
}

bool read_required_string(
	const elysia::io::json& node,
	std::string_view field,
	std::string& value,
	std::string& error)
{
	const std::string name(field);
	if (!node.contains(name) || !node.at(name).is_string())
		return fail(error, name + " is missing or invalid");
	value = node.at(name).get<std::string>();
	if (value.empty() && field != "key_namespace") return fail(error, name + " is empty");
	return true;
}

bool validate_template_tokens(
	std::string_view value,
	std::initializer_list<std::string_view> allowed,
	std::string& error)
{
	size_t position = 0;
	while (position < value.size())
	{
		const size_t begin = value.find_first_of("{}", position);
		if (begin == std::string_view::npos) break;
		if (value[begin] == '}') return fail(error, "template has an unmatched brace");
		const size_t end = value.find('}', begin + 1);
		if (end == std::string_view::npos) return fail(error, "template has an unterminated token");
		if (value.find('{', begin + 1) < end) return fail(error, "template has a nested token");
		const auto token = value.substr(begin, end - begin + 1);
		bool known = false;
		for (const auto candidate : allowed) known = known || token == candidate;
		if (!known) return fail(error, "template contains unsupported token: " + std::string(token));
		position = end + 1;
	}
	return true;
}

void replace_all(std::string& value, std::string_view marker, std::string_view replacement)
{
	size_t position = 0;
	while ((position = value.find(marker, position)) != std::string::npos)
	{
		value.replace(position, marker.size(), replacement);
		position += replacement.size();
	}
}

bool resolve_entity_root(
	const std::string& pattern,
	const std::string& id,
	std::filesystem::path& result,
	std::string& error)
{
	if (!validate_template_tokens(pattern, {"{id}"}, error)) return false;
	std::string value = pattern;
	if (value.find("{id}") == std::string::npos)
		value = (std::filesystem::path(value) / id).generic_string();
	else
		replace_all(value, "{id}", id);
	result = elysia::io::PathManager::instance()->to_asset_path(value);
	if (!std::filesystem::is_directory(result))
		return fail(error, "entity resource root does not exist: " + result.generic_string());
	return true;
}

bool resolve_config_template(
	const std::string& pattern,
	const std::string& id,
	std::filesystem::path& result,
	std::string& error)
{
	if (!validate_template_tokens(pattern, {"{id}"}, error)) return false;
	if (pattern.find("{id}") == std::string::npos)
		return fail(error, "config_template must contain {id}");
	std::string value = pattern;
	replace_all(value, "{id}", id);
	result = elysia::io::PathManager::instance()->to_config_path(value);
	if (!std::filesystem::is_regular_file(result))
		return fail(error, "configured file does not exist: " + result.generic_string());
	return true;
}

bool load_animation_layouts(
	const elysia::io::json& capability,
	std::unordered_map<std::string, elysia::io::AnimationLayout>& layouts,
	std::string& error)
{
	if (!capability.contains("layouts") || !capability.at("layouts").is_object()
		|| capability.at("layouts").empty()) return fail(error, "animations.layouts is missing or empty");
	std::string key_error;
	elysia::io::AnimationLayoutLoader loader;
	for (auto item = capability.at("layouts").begin(); item != capability.at("layouts").end(); ++item)
	{
		if (!elysia::resources::ResourceKeyBuilder::validate_component(item.key(), key_error)
			|| !item.value().is_string()) return fail(error, "invalid animation layout entry: " + item.key());
		const auto path = elysia::io::PathManager::instance()->to_asset_path(item.value().get<std::string>());
		elysia::io::AnimationLayout layout;
		if (!std::filesystem::is_regular_file(path) || !loader.load(path, layout))
			return fail(error, "animation layout load failed: " + path.generic_string());
		layouts.emplace(item.key(), std::move(layout));
	}
	return true;
}

bool validate_frame_prefix_template(
	const std::string& value,
	bool has_segments,
	std::string& error)
{
	if (value.find('/') != std::string::npos || value.find('\\') != std::string::npos
		|| value.find("..") != std::string::npos)
		return fail(error, "frame_prefix_template must be a filename prefix, not a path");
	if (!validate_template_tokens(value,
		{"{id}", "{animation}", "{segment_suffix}"}, error)) return false;
	if (value.find("{id}") == std::string::npos
		|| value.find("{animation}") == std::string::npos)
		return fail(error, "frame_prefix_template must contain {id} and {animation}");
	if (has_segments && value.find("{segment_suffix}") == std::string::npos)
		return fail(error, "segmented animations require {segment_suffix} in frame_prefix_template");
	return true;
}

elysia::io::EntityResourceIdentity make_identity(
	const elysia::io::EntityManifestEntry& entity,
	const std::string& module_name)
{
	auto identity = elysia::io::EntityResourceIdentity{
		entity.id, entity.animation_layout, entity.origin};
	identity.origin.module = module_name;
	identity.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
	return identity;
}

void enrich_animation_origins(
	elysia::io::AnimationConfig& config,
	const std::string& module_name,
	const std::string& entity_id)
{
	for (auto& clip : config.clips)
	{
		clip.origin.module = module_name;
		clip.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
		clip.origin.capability = "animations";
		clip.origin.entity_id = entity_id;
	}
}

void enrich_effect_origins(
	elysia::io::EffectDefinitionConfig& config,
	const std::string& module_name,
	const std::string& entity_id)
{
	for (auto& effect : config.effects)
	{
		effect.origin.module = module_name;
		effect.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
		effect.origin.capability = "effects";
		effect.origin.entity_id = entity_id;
	}
}
}

bool AnimatedEntityContentLoader::load(
	const std::string& module_name,
	const std::filesystem::path& manifest_path,
	elysia::io::EntityContentModule& content,
	std::string& error) const
{
	content = {};
	content.name = module_name;
	if (elysia::io::has_duplicate_json_object_key(manifest_path))
		return fail(error, "module manifest contains duplicate object keys");
	elysia::io::JsonLoader loader;
	if (!loader.open_file(manifest_path) || !loader.root().is_object())
		return fail(error, "module manifest is invalid: " + manifest_path.generic_string());
	const elysia::io::json& root = loader.root();
	if (!has_only_fields(root, {"entities", "key_namespace", "capabilities"}, "root", error)
		|| root.size() != 3) return fail(error, "entities, key_namespace and capabilities are required");

	std::string entities_value;
	if (!read_required_string(root, "entities", entities_value, error)
		|| !read_required_string(root, "key_namespace", content.key_namespace, error)) return false;
	std::string key_error;
	if (!content.key_namespace.empty()
		&& !elysia::resources::ResourceKeyBuilder::validate_component(content.key_namespace, key_error))
		return fail(error, key_error);
	if (!root.contains("capabilities") || !root.at("capabilities").is_object())
		return fail(error, "capabilities is missing or invalid");
	const elysia::io::json& capabilities = root.at("capabilities");
	for (auto item = capabilities.begin(); item != capabilities.end(); ++item)
	{
		if (item.key() != "animations" && item.key() != "effects"
			&& item.key() != "textures" && item.key() != "audio")
			return fail(error, "unknown capability: " + item.key());
		if (!item.value().is_object()) return fail(error, "capability is not an object: " + item.key());
	}
	if (capabilities.contains("effects") && !capabilities.contains("animations"))
		return fail(error, "effects requires animations in the same module");

	const auto entities_path = elysia::io::PathManager::instance()->to_asset_path(entities_value);
	elysia::io::EntityManifest entity_manifest;
	elysia::io::EntityManifestLoader entity_loader;
	if (!std::filesystem::is_regular_file(entities_path) || !entity_loader.load(entities_path, entity_manifest))
		return fail(error, "entity manifest load failed: " + entities_path.generic_string());
	for (const auto& entity : entity_manifest.entities)
		content.entities.push_back(make_identity(entity, module_name));

	if (capabilities.contains("animations"))
	{
		const auto& capability = capabilities.at("animations");
		if (!has_only_fields(capability,
			{"texture_root", "config_template", "frame_prefix_template", "layouts"},
			"animations capability", error)) return false;
		std::string texture_template, config_template, frame_prefix_template;
		if (!read_required_string(capability, "texture_root", texture_template, error)
			|| !read_required_string(capability, "config_template", config_template, error)) return false;
		if (capability.contains("frame_prefix_template"))
		{
			if (!capability.at("frame_prefix_template").is_string()) return fail(error, "frame_prefix_template is invalid");
			frame_prefix_template = capability.at("frame_prefix_template").get<std::string>();
			if (frame_prefix_template.empty()
				|| !validate_frame_prefix_template(frame_prefix_template, false, error)) return false;
		}
		std::unordered_map<std::string, elysia::io::AnimationLayout> layouts;
		if (!load_animation_layouts(capability, layouts, error)) return false;
		elysia::io::AnimationConfigLoader config_loader;
		for (const auto& entity : entity_manifest.entities)
		{
			const auto layout = layouts.find(entity.animation_layout);
			if (entity.animation_layout.empty() || layout == layouts.end())
				return fail(error, "unknown animation layout for entity: " + entity.id);
			std::filesystem::path texture_root, config_path;
			if (!resolve_entity_root(texture_template, entity.id, texture_root, error)
				|| !resolve_config_template(config_template, entity.id, config_path, error)) return false;
			elysia::io::AnimationConfig config;
			if (!config_loader.load(config_path, layout->second, config))
				return fail(error, "animation config load failed: " + config_path.generic_string());
			const bool has_segments = std::any_of(config.clips.begin(), config.clips.end(),
				[](const auto& clip) { return clip.is_segment; });
			if (config.source_type == elysia::io::AnimationSourceType::FrameDirectory)
			{
				if (frame_prefix_template.empty()
					|| !validate_frame_prefix_template(frame_prefix_template, has_segments, error)) return false;
			}
			enrich_animation_origins(config, module_name, entity.id);
			content.animation_entries.push_back({
				make_identity(entity, module_name), std::move(texture_root), frame_prefix_template, std::move(config)});
		}
	}

	if (capabilities.contains("effects"))
	{
		const auto& capability = capabilities.at("effects");
		if (!has_only_fields(capability, {"config_template"}, "effects capability", error)) return false;
		std::string config_template;
		if (!read_required_string(capability, "config_template", config_template, error)) return false;
		elysia::io::EffectDefinitionConfigLoader effect_loader;
		for (const auto& animation_entry : content.animation_entries)
		{
			std::filesystem::path config_path;
			if (!resolve_config_template(config_template, animation_entry.entity.id, config_path, error)) return false;
			elysia::io::EffectDefinitionConfig config;
			if (!effect_loader.load(config_path, animation_entry.animation_config, config))
				return fail(error, "effect config load failed: " + config_path.generic_string());
			enrich_effect_origins(config, module_name, animation_entry.entity.id);
			content.effect_entries.push_back({animation_entry.entity, std::move(config)});
		}
	}

	if (capabilities.contains("textures"))
	{
		const auto& capability = capabilities.at("textures");
		if (!has_only_fields(capability, {"texture_root", "layout"}, "textures capability", error)) return false;
		std::string texture_template, layout_value;
		if (!read_required_string(capability, "texture_root", texture_template, error)
			|| !read_required_string(capability, "layout", layout_value, error)) return false;
		const auto layout_path = elysia::io::PathManager::instance()->to_asset_path(layout_value);
		elysia::io::EntityTextureLayout layout;
		elysia::io::EntityTextureLayoutLoader layout_loader;
		if (!std::filesystem::is_regular_file(layout_path) || !layout_loader.load(layout_path, layout))
			return fail(error, "texture layout load failed");
		for (const auto& entity : entity_manifest.entities)
		{
			std::filesystem::path root_path;
			if (!resolve_entity_root(texture_template, entity.id, root_path, error)) return false;
			auto entity_layout = layout;
			for (auto& item : entity_layout.textures)
			{
				item.origin.module = module_name;
				item.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
				item.origin.entity_id = entity.id;
			}
			content.texture_entries.push_back({make_identity(entity, module_name), std::move(root_path), std::move(entity_layout)});
		}
	}

	if (capabilities.contains("audio"))
	{
		const auto& capability = capabilities.at("audio");
		if (!has_only_fields(capability, {"audio_root", "layout"}, "audio capability", error)) return false;
		std::string audio_template, layout_value;
		if (!read_required_string(capability, "audio_root", audio_template, error)
			|| !read_required_string(capability, "layout", layout_value, error)) return false;
		const auto layout_path = elysia::io::PathManager::instance()->to_asset_path(layout_value);
		elysia::io::EntityAudioLayout layout;
		elysia::io::EntityAudioLayoutLoader layout_loader;
		if (!std::filesystem::is_regular_file(layout_path) || !layout_loader.load(layout_path, layout))
			return fail(error, "audio layout load failed");
		for (const auto& entity : entity_manifest.entities)
		{
			std::filesystem::path root_path;
			if (!resolve_entity_root(audio_template, entity.id, root_path, error)) return false;
			auto entity_layout = layout;
			for (auto& item : entity_layout.sounds)
			{
				item.origin.module = module_name;
				item.origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
				item.origin.entity_id = entity.id;
			}
			content.audio_entries.push_back({make_identity(entity, module_name), std::move(root_path), std::move(entity_layout)});
		}
	}
	return true;
}
}
