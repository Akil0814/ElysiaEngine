#include "../../tools/logger.h"
#include "texture_loader.h"
namespace elysia::resources
{
void TextureDeleter::operator()(SDL_Texture* texture) const
{
	if (texture)
		SDL_DestroyTexture(texture);
}

TexturePtr TextureLoader::create_texture(
	SDL_Renderer* renderer,
	const SDL_Surface& surface) const
{
	if (!renderer)
		return {};

	return TexturePtr(SDL_CreateTextureFromSurface(
		renderer,
		const_cast<SDL_Surface*>(&surface)));
}

TextureLoadResult TextureLoader::load_texture(
	SDL_Renderer* renderer,
	const SurfaceLoadResult& surface_result
) const
{
	TextureLoadResult result;
	result._asset_key = surface_result._asset_key;
	result._frame_path = surface_result._frame_path;
	result._frame_index = surface_result._frame_index;

	if (!renderer)
	{
		ELYSIA_LOG_WARN("resource","Load texture failed: renderer is null: "
			<< surface_result._asset_key);
		return result;
	}

	if (!surface_result._success || !surface_result._surface)
	{
		ELYSIA_LOG_WARN("resource","Load texture failed: surface is invalid: "
			<< surface_result._frame_path);
		return result;
	}

	TexturePtr texture = create_texture(renderer,*surface_result._surface);
	if (!texture)
	{
		ELYSIA_LOG_WARN("resource","Load texture failed: " << surface_result._frame_path
			<< ", reason: " << SDL_GetError());
		return result;
	}

	result._texture = std::move(texture);
	result._success = true;
	return result;
}


}
