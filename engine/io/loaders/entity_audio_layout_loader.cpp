#include "../../resources/pipeline/resource_key_builder.h"
#include "../../tools/logger.h"
#include "entity_audio_layout_loader.h"
#include "../json/json_loader.h"
#include "../json/json_duplicate_key_checker.h"

namespace elysia::io
{
bool EntityAudioLayoutLoader::load(const std::filesystem::path& path, EntityAudioLayout& layout) const
{
	layout = {};
	if (has_duplicate_json_object_key(path)) return false;
	JsonLoader loader;
	if (!loader.open_file(path) || !loader.root().is_object())
	{
		ELYSIA_LOG_WARN("io", "Load entity audio layout failed: " << path);
		return false;
	}
	std::string error;
	for (auto item = loader.root().begin(); item != loader.root().end(); ++item)
	{
		if (!elysia::resources::ResourceKeyBuilder::validate_component(item.key(), error)
			|| !item.value().is_string() || item.value().get<std::string>().empty()) return false;
		layout.sounds.push_back({item.key(), item.value().get<std::string>(),
			elysia::resources::make_resource_origin(path, "/" + item.key(), {}, "audio", {}, item.key())});
	}
	return true;
}
}
