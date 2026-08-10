#include "../../tools/logger.h"
#include "audio_manager.h"
namespace elysia::resources
{
AudioManager::~AudioManager()
{
	clear();
}

bool AudioManager::load_sound(
	const std::string& key,
	const std::filesystem::path& file_path
)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Load sound failed: key is empty.");
		return false;
	}

	if (file_path.empty())
	{
		ELYSIA_LOG_WARN("resource","Load sound failed: file path is empty: " << key);
		return false;
	}

	Mix_Chunk* sound = Mix_LoadWAV(file_path.string().c_str());
	if (!sound)
	{
		ELYSIA_LOG_WARN("resource","Load sound failed: " << file_path
			<< " error: " << Mix_GetError());
		return false;
	}

	return store_sound(key, sound);
}

bool AudioManager::load_sounds(const std::vector<SoundLoadRequest>& requests)
{
	for (const SoundLoadRequest& request : requests)
	{
		if (!load_sound(request.key, request.file_path))
			return false;
	}

	return true;
}

bool AudioManager::store_sound(const std::string& key, Mix_Chunk* sound)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Store sound failed: key is empty.");
		if (sound)
			Mix_FreeChunk(sound);
		return false;
	}

	if (!sound)
	{
		ELYSIA_LOG_WARN("resource","Store sound failed: sound is null: " << key);
		return false;
	}

	SoundPool::iterator iterator = _sound_pool.find(key);
	if (iterator != _sound_pool.end())
	{
		if (iterator->second)
			Mix_FreeChunk(iterator->second);

		iterator->second = sound;
		return true;
	}

	_sound_pool.emplace(key, sound);
	return true;
}

Mix_Chunk* AudioManager::find_sound(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find sound failed: key is empty.");
		return nullptr;
	}

	SoundPool::const_iterator iterator = _sound_pool.find(std::string(key));
	if (iterator == _sound_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find sound failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

bool AudioManager::load_music(
	const std::string& key,
	const std::filesystem::path& file_path
)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Load music failed: key is empty.");
		return false;
	}

	if (file_path.empty())
	{
		ELYSIA_LOG_WARN("resource","Load music failed: file path is empty: " << key);
		return false;
	}

	Mix_Music* music = Mix_LoadMUS(file_path.string().c_str());
	if (!music)
	{
		ELYSIA_LOG_WARN("resource","Load music failed: " << file_path
			<< " error: " << Mix_GetError());
		return false;
	}

	return store_music(key, music);
}

bool AudioManager::load_music(const MusicLoadRequest& request)
{
	return load_music(request.key, request.file_path);
}

bool AudioManager::load_music(const std::vector<MusicLoadRequest>& requests)
{
	for (const MusicLoadRequest& request : requests)
	{
		if (!load_music(request))
			return false;
	}

	return true;
}

bool AudioManager::store_music(const std::string& key, Mix_Music* music)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Store music failed: key is empty.");
		if (music)
			Mix_FreeMusic(music);
		return false;
	}

	if (!music)
	{
		ELYSIA_LOG_WARN("resource","Store music failed: music is null: " << key);
		return false;
	}

	MusicPool::iterator iterator = _music_pool.find(key);
	if (iterator != _music_pool.end())
	{
		if (iterator->second)
			Mix_FreeMusic(iterator->second);

		iterator->second = music;
		return true;
	}

	_music_pool.emplace(key, music);
	return true;
}

Mix_Music* AudioManager::find_music(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find music failed: key is empty.");
		return nullptr;
	}

	MusicPool::const_iterator iterator = _music_pool.find(std::string(key));
	if (iterator == _music_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find music failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

void AudioManager::clear()
{
	for (SoundPool::value_type& sound : _sound_pool)
	{
		if (sound.second)
			Mix_FreeChunk(sound.second);
	}

	for (MusicPool::value_type& music : _music_pool)
	{
		if (music.second)
			Mix_FreeMusic(music.second);
	}

	_sound_pool.clear();
	_music_pool.clear();
}

size_t AudioManager::resource_count() const
{
	return _sound_pool.size() + _music_pool.size();
}

}
