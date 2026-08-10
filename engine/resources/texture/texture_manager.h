#pragma once
#include "../resource_sub_manager.h"
#include "texture_loader.h"

#include <SDL.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::resources
{
using TexturePool = std::unordered_map<std::string, TextureResource>;

struct AnimationTextureResource
{
	std::string key;
	TexturePtr texture;
	TexturePtr coverage_mask;
};

class TextureManager : public ResourceSubManager
{
public:
	bool store_texture(const std::string& key, TexturePtr texture);
	bool store_animation_texture(
		const std::string& key,
		TexturePtr texture,
		TexturePtr coverage_mask);
	bool store_animation_textures(
		std::vector<AnimationTextureResource>&& resources);
	SDL_Texture* find_texture(const std::string_view& key) const;
	SDL_Texture* find_coverage_mask(const std::string_view& key) const;

	void clear() override;
	size_t resource_count() const override;

private:
	bool store_resource(const std::string& key, TextureResource resource);
	TexturePool _texture_pool;
};

}
