#pragma once

#include "../io/loaders/asset_config_types.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace elysia::config
{
class ConfigSnapshot;
}

namespace elysia::loading
{
struct ContentManifestResult
{
	std::shared_ptr<const elysia::config::ConfigSnapshot> config_snapshot;
	elysia::io::FontManifest font_manifest;
	elysia::io::AudioManifest audio_manifest;
	elysia::io::TextureManifest texture_manifest;
	elysia::io::AnimationManifest animation_manifest;
	elysia::io::AnimationEffectManifest animation_effect_manifest;

	std::map<std::string, elysia::io::EntityContentModule> additional_modules;
};

class ContentManifestPipeline
{
public:
	bool load(
		const elysia::io::ContentRegistry& content_registry,
		ContentManifestResult& result
	);

	const std::string& error_message() const;

private:
	void fail(std::string message);

private:
	std::string _error_message;
};

}
