#include "../tools/logger.h"
#include "content_manifest_pipeline.h"

#include "../io/loaders/animation_manifest_loader.h"
#include "../io/loaders/audio_manifest_loader.h"
#include "../io/loaders/animation_effect_manifest_loader.h"
#include "../io/loaders/fonts_manifest_loader.h"
#include "../io/loaders/texture_manifest_loader.h"
#include "../config/content/config_load_pipeline.h"
#include "animated_entity_content_loader.h"

namespace elysia::loading
{
bool ContentManifestPipeline::load(
	const elysia::io::ContentRegistry& content_registry,
	ContentManifestResult& result
)
{
	result = ContentManifestResult{};
	_error_message.clear();

	const elysia::io::CoreManifestPaths& manifest_paths = content_registry.required;
	const auto config_snapshot = elysia::config::ConfigLoadPipeline{}.load(manifest_paths.configs);
	if (!config_snapshot)
	{
		fail("Content manifest pipeline failed: game config load failed: " + config_snapshot.error().message);
		return false;
	}
	result.config_snapshot = *config_snapshot;

	elysia::io::FontsManifestLoader fonts_manifest_loader;
	if (!fonts_manifest_loader.load(manifest_paths.fonts, result.font_manifest))
	{
		fail("Content manifest pipeline failed: fonts manifest load failed.");
		return false;
	}

	elysia::io::AudioManifestLoader audio_manifest_loader;
	if (!audio_manifest_loader.load(manifest_paths.audio, result.audio_manifest))
	{
		fail("Content manifest pipeline failed: audio manifest load failed.");
		return false;
	}

	elysia::io::TextureManifestLoader texture_manifest_loader;
	if (!texture_manifest_loader.load(manifest_paths.textures, result.texture_manifest))
	{
		fail("Content manifest pipeline failed: textures manifest load failed.");
		return false;
	}

	elysia::io::AnimationManifestLoader animation_manifest_loader;
	if (!animation_manifest_loader.load(manifest_paths.animations, result.animation_manifest))
	{
		fail("Content manifest pipeline failed: animations manifest load failed.");
		return false;
	}

	elysia::io::AnimationEffectManifestLoader animation_effect_manifest_loader;
	if (!animation_effect_manifest_loader.load(manifest_paths.effects, result.animation_effect_manifest))
	{
		fail("Content manifest pipeline failed: effects manifest load failed.");
		return false;
	}


	AnimatedEntityContentLoader module_loader;
	for (const auto& [module_name, module_manifest_path] : content_registry.additional_module_manifests)
	{
		elysia::io::EntityContentModule module;
		std::string module_error;
		if (!module_loader.load(module_name, module_manifest_path, module, module_error))
		{
			fail("Content manifest pipeline failed: " + module_error);
			return false;
		}
		result.additional_modules.emplace(module_name, std::move(module));
	}

	return true;
}

const std::string& ContentManifestPipeline::error_message() const
{
	return _error_message;
}

void ContentManifestPipeline::fail(std::string message)
{
	_error_message = std::move(message);
	ELYSIA_LOG_ERROR("resource",_error_message);
}

}
