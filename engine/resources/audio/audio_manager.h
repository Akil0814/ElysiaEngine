#pragma once
#include "../resource_sub_manager.h"
#include "../resource_types.h"

#include <SDL_mixer.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::resources
{
using SoundPool = std::unordered_map<std::string, Mix_Chunk*>;
using MusicPool = std::unordered_map<std::string, Mix_Music*>;

class AudioManager : public ResourceSubManager
{
public:
	~AudioManager() override;

	bool load_sound(
		const std::string& key,
		const std::filesystem::path& file_path
	);
	bool load_sounds(const std::vector<SoundLoadRequest>& requests);
	bool store_sound(const std::string& key, Mix_Chunk* sound);
	Mix_Chunk* find_sound(const std::string_view& key) const;

	bool load_music(
		const std::string& key,
		const std::filesystem::path& file_path
	);
	bool load_music(const MusicLoadRequest& request);
	bool load_music(const std::vector<MusicLoadRequest>& requests);
	bool store_music(const std::string& key, Mix_Music* music);
	Mix_Music* find_music(const std::string_view& key) const;

	void clear() override;
	size_t resource_count() const override;

private:
	SoundPool _sound_pool;
	MusicPool _music_pool;
};

}
