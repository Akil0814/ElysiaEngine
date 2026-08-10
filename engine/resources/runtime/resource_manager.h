#pragma once
#include "../atlas/atlas_manager.h"
#include "../audio/audio_manager.h"
#include "../font/font_manager.h"
#include "../texture/texture_manager.h"

#include "../../tools/singleton.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <filesystem>
#include <string_view>
#include <vector>

namespace elysia::resources
{
class ResourceService;

class ResourceManager : public elysia::tools::Singleton<ResourceManager>
{
	friend elysia::tools::Singleton<ResourceManager>;
	friend class ResourceService;

public:
	bool init();

	bool begin_atlas_build(const AtlasBuildRequest& request);
	bool begin_atlas_builds(const std::vector<AtlasBuildRequest>& requests);
	bool commit_prepared_atlas_frame(SDL_Renderer* renderer,
		const AtlasFramePreparedResult& result);
	bool store_texture(const std::string& key,TexturePtr texture);
	bool load_font(const std::string& key,
		const std::filesystem::path& file_path,int point_size);
	bool load_sound(const SoundLoadRequest& request);
	bool load_sounds(const std::vector<SoundLoadRequest>& requests);
	bool load_music(const MusicLoadRequest& request);
	bool load_music(const std::vector<MusicLoadRequest>& requests);

	void clear();
	[[nodiscard]] bool has_in_progress_atlas_builds() const;
	[[nodiscard]] size_t atlas_resource_count() const;
	[[nodiscard]] size_t texture_resource_count() const;
	[[nodiscard]] size_t resource_count() const;

private:
	ResourceManager();

	[[nodiscard]] Atlas* find_atlas(std::string_view key) const;
	[[nodiscard]] bool has_font(std::string_view key) const noexcept;
	[[nodiscard]] TTF_Font* find_font(std::string_view key) const;
	[[nodiscard]] Mix_Chunk* find_sound(std::string_view key) const;
	[[nodiscard]] Mix_Music* find_music(std::string_view key) const;
	[[nodiscard]] SDL_Texture* find_texture(std::string_view key) const;

	TextureManager _texture_manager;
	AtlasManager _atlas_manager;
	FontManager _font_manager;
	AudioManager _audio_manager;
};

}
