#include "resource_manager.h"

#include <utility>

namespace elysia::resources
{
elysia::resources::ResourceManager::ResourceManager()
	: _atlas_manager(_texture_manager)
{
}

bool elysia::resources::ResourceManager::init()
{
	return true;
}

bool elysia::resources::ResourceManager::begin_atlas_build(const AtlasBuildRequest& request)
{
	return _atlas_manager.begin_build(request);
}

bool elysia::resources::ResourceManager::begin_atlas_builds(const std::vector<AtlasBuildRequest>& requests)
{
	return _atlas_manager.begin_builds(requests);
}

bool elysia::resources::ResourceManager::commit_prepared_atlas_frame(
	SDL_Renderer* renderer,
	const AtlasFramePreparedResult& result
)
{
	return _atlas_manager.commit_prepared_frame(renderer, result);
}

bool elysia::resources::ResourceManager::store_texture(
	const std::string& key,
	TexturePtr texture)
{
	return _texture_manager.store_texture(key,std::move(texture));
}

bool elysia::resources::ResourceManager::load_font(
	const std::string& key,
	const std::filesystem::path& file_path,
	int point_size
)
{
	return _font_manager.load_font(key, file_path, point_size);
}

bool elysia::resources::ResourceManager::load_sounds(const std::vector<SoundLoadRequest>& requests)
{
	return _audio_manager.load_sounds(requests);
}

bool elysia::resources::ResourceManager::load_sound(const SoundLoadRequest& request)
{
	return _audio_manager.load_sound(request.key,request.file_path);
}

bool elysia::resources::ResourceManager::load_music(const MusicLoadRequest& request)
{
	return _audio_manager.load_music(request);
}

bool elysia::resources::ResourceManager::load_music(const std::vector<MusicLoadRequest>& requests)
{
	return _audio_manager.load_music(requests);
}

Atlas* elysia::resources::ResourceManager::find_atlas(std::string_view key) const
{
	return _atlas_manager.find_atlas(key);
}

bool elysia::resources::ResourceManager::has_font(std::string_view key) const noexcept
{
	return _font_manager.has_font(key);
}

TTF_Font* elysia::resources::ResourceManager::find_font(std::string_view key) const
{
	return _font_manager.find_font(key);
}

Mix_Chunk* elysia::resources::ResourceManager::find_sound(std::string_view key) const
{
	return _audio_manager.find_sound(key);
}

Mix_Music* elysia::resources::ResourceManager::find_music(std::string_view key) const
{
	return _audio_manager.find_music(key);
}

SDL_Texture* elysia::resources::ResourceManager::find_texture(std::string_view key) const
{
	return _texture_manager.find_texture(key);
}

void elysia::resources::ResourceManager::clear()
{
	_atlas_manager.clear();
	_texture_manager.clear();
	_font_manager.clear();
	_audio_manager.clear();
}

bool elysia::resources::ResourceManager::has_in_progress_atlas_builds() const
{
	return _atlas_manager.in_progress_build_count() != 0;
}

size_t elysia::resources::ResourceManager::atlas_resource_count() const
{
	return _atlas_manager.resource_count();
}

size_t elysia::resources::ResourceManager::texture_resource_count() const
{
	return _texture_manager.resource_count();
}

size_t elysia::resources::ResourceManager::resource_count() const
{
	return _atlas_manager.resource_count()
		+ _texture_manager.resource_count()
		+ _font_manager.resource_count()
		+ _audio_manager.resource_count();
}

}
