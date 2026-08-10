#pragma once

#include "../tools/singleton.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <string_view>

#define ELYSIA_RESOURCES (::elysia::resources::ResourceService::instance())

namespace elysia::resources
{
class Atlas;

class ResourceService final : public elysia::tools::Singleton<ResourceService>
{
	friend elysia::tools::Singleton<ResourceService>;

public:
	[[nodiscard]] const Atlas* find_atlas(std::string_view key) const;
	[[nodiscard]] bool has_font(std::string_view key) const noexcept;
	[[nodiscard]] TTF_Font* find_font(std::string_view key) const;
	[[nodiscard]] Mix_Chunk* find_sound(std::string_view key) const;
	[[nodiscard]] Mix_Music* find_music(std::string_view key) const;
	[[nodiscard]] SDL_Texture* find_texture(std::string_view key) const;

private:
	ResourceService() = default;
};
}
