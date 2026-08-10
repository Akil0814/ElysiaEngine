#include "resource_request_builder.h"

#include "filesystem_segment_formatter.h"
#include "resource_key_builder.h"
#include "../../io/path/path_manager.h"
#include "../../tools/logger.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace elysia::resources
{
namespace
{
bool log_key_error(const char* operation, const std::string& error)
{
	ELYSIA_LOG_WARN("resource", operation << ": " << error);
	return false;
}

bool append_texture_request(
	std::string key,
	std::filesystem::path file_path,
	ResourceOrigin origin,
	std::vector<TextureLoadRequest>& requests)
{
	if (!std::filesystem::is_regular_file(file_path))
	{
		ELYSIA_LOG_WARN("resource", "Build texture request failed: file does not exist: " << file_path);
		return false;
	}
	requests.push_back({std::move(key), std::move(file_path), std::move(origin)});
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

bool make_frame_prefix(
	const std::string& pattern,
	const elysia::io::EntityAnimationContentEntry& entry,
	const elysia::io::AnimationClipConfig& clip,
	std::string& prefix)
{
	prefix = pattern;
	replace_all(prefix, "{id}", entry.entity.id);
	replace_all(prefix, "{animation}", clip.animation_name);
	std::string suffix;
	if (clip.is_segment)
	{
		std::string segment;
		if (!format_filesystem_segment(clip.segment_index, segment)) return false;
		suffix = "_" + segment;
	}
	replace_all(prefix, "{segment_suffix}", suffix);
	return !prefix.empty() && prefix.find('{') == std::string::npos && prefix.find('}') == std::string::npos;
}

std::optional<size_t> clip_segment(const elysia::io::AnimationClipConfig& clip)
{
	return clip.is_segment ? std::optional<size_t>(clip.segment_index) : std::nullopt;
}

std::optional<size_t> effect_segment(const elysia::io::EffectDefinitionConfigEntry& effect)
{
	return effect.is_segment ? std::optional<size_t>(effect.segment_index) : std::nullopt;
}
}

bool ResourceRequestBuilder::append_font_requests(
	const elysia::io::FontManifest& manifest,
	std::span<const int> point_sizes,
	std::vector<FontLoadRequest>& requests) const
{
	const auto root = elysia::io::PathManager::instance()->fonts();
	for (const auto& entry : manifest.fonts)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)) return log_key_error("Build font requests failed", error);
		for (const int size : point_sizes)
		{
			if (size <= 0) return false;
			std::string key;
			if (!ResourceKeyBuilder::append_component(entry.key, std::to_string(size), key, error))
				return log_key_error("Build font requests failed", error);
			auto origin = entry.origin;
			origin.logical_name = key;
			requests.push_back({std::move(key), (root / entry.file_path).lexically_normal(), size, std::move(origin)});
		}
	}
	return true;
}

bool ResourceRequestBuilder::append_audio_requests(
	const elysia::io::AudioManifest& manifest,
	std::vector<SoundLoadRequest>& sounds,
	std::vector<MusicLoadRequest>& music) const
{
	const auto root = elysia::io::PathManager::instance()->audio();
	for (const auto& entry : manifest.sounds)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)) return log_key_error("Build sound requests failed", error);
		sounds.push_back({entry.key, (root / entry.file_path).lexically_normal(), entry.origin});
	}
	for (const auto& entry : manifest.music)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)) return log_key_error("Build music requests failed", error);
		music.push_back({entry.key, (root / entry.file_path).lexically_normal(), entry.origin});
	}
	return true;
}

bool ResourceRequestBuilder::append_texture_manifest_requests(
	const elysia::io::TextureManifest& manifest,
	const std::filesystem::path& root,
	std::vector<TextureLoadRequest>& requests) const
{
	if (root.empty())
	{
		ELYSIA_LOG_WARN("resource", "Build texture requests failed: texture root is empty.");
		return false;
	}
	for (const auto& entry : manifest.textures)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)) return log_key_error("Build texture requests failed", error);
		if (!append_texture_request(entry.key, (root / entry.file_path).lexically_normal(), entry.origin, requests)) return false;
	}
	return true;
}

bool ResourceRequestBuilder::append_animation_manifest_requests(
	const elysia::io::AnimationManifest& manifest,
	const std::filesystem::path& root,
	std::vector<AtlasBuildRequest>& atlases,
	std::vector<AnimationBuildRequest>& animations) const
{
	for (const auto& entry : manifest.animations)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)) return log_key_error("Build animation requests failed", error);
		const auto source = (root / entry.source_path).lexically_normal();
		const bool exists = entry.horizontal_strip ? std::filesystem::is_regular_file(source) : std::filesystem::is_directory(source);
		if (!exists)
		{
			ELYSIA_LOG_WARN("resource", "Build animation requests failed: source is missing or has the wrong type: key="
				<< entry.key << ", path=" << source);
			return false;
		}
		if (entry.frame_count == 0 || entry.fps <= 0.0)
		{
			ELYSIA_LOG_WARN("resource", "Build animation requests failed: invalid frame count or FPS: " << entry.key);
			return false;
		}
		AtlasBuildRequest atlas;
		atlas.atlas_key = entry.key;
		atlas.source_path = source;
		atlas.frame_count = entry.frame_count;
		atlas.frame_filename_prefix = entry.frame_prefix;
		atlas.source_type = entry.horizontal_strip ? AtlasSourceType::HorizontalStrip : AtlasSourceType::FrameDirectory;
		atlas.origin = entry.origin;
		AnimationBuildRequest animation;
		animation.animation_key = entry.key;
		animation.atlas_key = entry.key;
		animation.fps = entry.fps;
		animation.loop = entry.loop;
		animation.origin = entry.origin;
		atlases.push_back(std::move(atlas));
		animations.push_back(std::move(animation));
	}
	return true;
}

bool ResourceRequestBuilder::append_animation_effect_manifest_requests(
	const elysia::io::AnimationEffectManifest& manifest,
	std::vector<AnimationEffectBuildRequest>& requests) const
{
	for (const auto& entry : manifest.effects)
	{
		std::string error;
		if (!ResourceKeyBuilder::validate_key(entry.key, error)
			|| !ResourceKeyBuilder::validate_key(entry.animation_key, error))
			return log_key_error("Build effect requests failed", error);
		requests.push_back({entry.key, entry.animation_key,
			elysia::core::Vector2(entry.default_width, entry.default_height),
			entry.default_angle_degrees, entry.origin});
	}
	return true;
}

bool ResourceRequestBuilder::append_entity_animation_requests(
	const std::string& key_namespace,
	const elysia::io::EntityAnimationContentEntry& entry,
	std::vector<AtlasBuildRequest>& atlases,
	std::vector<AnimationBuildRequest>& animations) const
{
	for (const auto& clip : entry.animation_config.clips)
	{
		std::string key, error;
		if (!ResourceKeyBuilder::build(entry.entity.id, key_namespace, {clip.animation_name},
			clip_segment(clip), key, error)) return log_key_error("Build entity animation requests failed", error);
		AtlasBuildRequest atlas;
		atlas.atlas_key = key;
		atlas.frame_count = clip.frame_count;
		atlas.origin = clip.origin;
		const auto resolved = (entry.texture_root / clip.path).lexically_normal();
		if (entry.animation_config.source_type == elysia::io::AnimationSourceType::HorizontalStrip)
		{
			atlas.source_type = AtlasSourceType::HorizontalStrip;
			atlas.source_path = (resolved / (clip.animation_name + ".png")).lexically_normal();
			if (!std::filesystem::is_regular_file(atlas.source_path))
			{
				ELYSIA_LOG_WARN("resource", "Build entity animation requests failed: horizontal strip does not exist: key="
					<< key << ", path=" << atlas.source_path);
				return false;
			}
		}
		else
		{
			atlas.source_type = AtlasSourceType::FrameDirectory;
			atlas.source_path = resolved;
			if (!std::filesystem::is_directory(atlas.source_path))
			{
				ELYSIA_LOG_WARN("resource", "Build entity animation requests failed: frame directory does not exist: key="
					<< key << ", path=" << atlas.source_path);
				return false;
			}
			if (!make_frame_prefix(entry.frame_prefix_template, entry, clip, atlas.frame_filename_prefix))
			{
				ELYSIA_LOG_WARN("resource", "Build entity animation requests failed: frame prefix template expansion failed: key="
					<< key << ", template=" << entry.frame_prefix_template);
				return false;
			}
		}
		AnimationBuildRequest animation;
		animation.animation_key = key;
		animation.atlas_key = key;
		animation.fps = clip.fps;
		animation.loop = clip.loop;
		animation.segment_index = clip.segment_index;
		animation.origin = clip.origin;
		atlases.push_back(std::move(atlas));
		animations.push_back(std::move(animation));
	}
	return true;
}

bool ResourceRequestBuilder::append_entity_effect_requests(
	const std::string& key_namespace,
	const elysia::io::EntityEffectContentEntry& entry,
	const std::vector<AnimationBuildRequest>& animations,
	std::vector<AnimationEffectBuildRequest>& effects) const
{
	for (const auto& definition : entry.effect_config.effects)
	{
		std::string effect_key, animation_key, error;
		if (!ResourceKeyBuilder::build(entry.entity.id, key_namespace, {definition.effect_name},
			effect_segment(definition), effect_key, error)
			|| !ResourceKeyBuilder::build(entry.entity.id, key_namespace, {definition.animation_name},
				effect_segment(definition), animation_key, error))
			return log_key_error("Build entity effect requests failed", error);
		const bool exists = std::any_of(animations.begin(), animations.end(),
			[&animation_key](const auto& request) { return request.animation_key == animation_key; });
		if (!exists)
		{
			ELYSIA_LOG_WARN("resource", "Build entity effect requests failed: animation request does not exist: " << animation_key);
			return false;
		}
		effects.push_back({std::move(effect_key), std::move(animation_key),
			elysia::core::Vector2(definition.default_width, definition.default_height),
			definition.default_angle_degrees, definition.origin});
	}
	return true;
}

bool ResourceRequestBuilder::append_entity_texture_requests(
	const std::string& key_namespace,
	const elysia::io::EntityTextureContentEntry& content,
	std::vector<TextureLoadRequest>& requests) const
{
	for (const auto& entry : content.layout.textures)
	{
		std::string base_key, error;
		if (!ResourceKeyBuilder::build(content.entity.id, key_namespace, {entry.key}, std::nullopt, base_key, error))
			return log_key_error("Build entity texture requests failed", error);
		const auto resolved = (content.texture_root / entry.path).lexically_normal();
		if (std::filesystem::is_regular_file(resolved))
		{
			if (!append_texture_request(base_key, resolved, entry.origin, requests)) return false;
			continue;
		}
		if (!std::filesystem::is_directory(resolved))
		{
			ELYSIA_LOG_WARN("resource", "Build entity texture requests failed: target is neither a file nor directory: key="
				<< base_key << ", path=" << resolved);
			return false;
		}
		std::vector<std::filesystem::path> files;
		for (const auto& directory_entry : std::filesystem::directory_iterator(resolved))
			if (directory_entry.is_regular_file()) files.push_back(directory_entry.path());
		std::sort(files.begin(), files.end());
		if (files.empty())
		{
			ELYSIA_LOG_WARN("resource", "Build entity texture requests failed: directory contains no regular files: key="
				<< base_key << ", path=" << resolved);
			return false;
		}
		for (const auto& file : files)
		{
			const std::string stem = file.stem().string();
			std::string key;
			if (!ResourceKeyBuilder::append_component(base_key, stem, key, error))
				return log_key_error("Build entity texture requests failed", error);
			auto origin = entry.origin;
			origin.json_pointer += "/files/" + file.filename().generic_string();
			origin.logical_name = entry.key + "." + stem;
			if (!append_texture_request(std::move(key), file, std::move(origin), requests)) return false;
		}
	}
	return true;
}

bool ResourceRequestBuilder::append_entity_audio_requests(
	const std::string& key_namespace,
	const elysia::io::EntityAudioContentEntry& content,
	std::vector<SoundLoadRequest>& requests) const
{
	for (const auto& entry : content.layout.sounds)
	{
		std::string key, error;
		if (!ResourceKeyBuilder::build(content.entity.id, key_namespace, {entry.key}, std::nullopt, key, error))
			return log_key_error("Build entity audio requests failed", error);
		const auto path = (content.audio_root / entry.path).lexically_normal();
		if (!std::filesystem::is_regular_file(path))
		{
			ELYSIA_LOG_WARN("resource", "Build entity audio requests failed: sound file does not exist: key="
				<< key << ", path=" << path);
			return false;
		}
		requests.push_back({std::move(key), path, entry.origin});
	}
	return true;
}
}
