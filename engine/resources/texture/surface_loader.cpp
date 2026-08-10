#include "../../tools/logger.h"
#include "surface_loader.h"

#include <SDL_image.h>

#include <cstdint>
#include <cstring>
namespace elysia::resources
{
void SurfaceDeleter::operator()(SDL_Surface* surface) const
{
	if (surface)
		SDL_FreeSurface(surface);
}

SurfaceLoadResult SurfaceLoader::load_surface(const SurfaceLoadRequest& request) const
{
	SurfaceLoadResult result;
	result._asset_key = request._asset_key;
	result._frame_path = request._frame_path;
	result._frame_index = request._frame_index;

	if (request._asset_key.empty())
	{
		ELYSIA_LOG_WARN("resource","Load surface failed: asset key is empty.");
		return result;
	}

	if (request._frame_path.empty())
	{
		ELYSIA_LOG_WARN("resource","Load surface failed: frame path is empty: "
			<< request._asset_key);
		return result;
	}

	SDL_Surface* surface = IMG_Load(request._frame_path.string().c_str());
	if (!surface)
	{
		ELYSIA_LOG_WARN("resource","Load surface failed: " << request._frame_path
			<< ", reason: " << IMG_GetError());
		return result;
	}

	result._surface.reset(surface);
	result._success = true;
	return result;
}

SurfacePtr create_coverage_mask_surface(const SDL_Surface& source_surface)
{
	if (source_surface.w <= 0 || source_surface.h <= 0
		|| !source_surface.format || !source_surface.pixels)
	{
		ELYSIA_LOG_WARN("resource","Create coverage mask failed: source surface is invalid.");
		return {};
	}

	SurfacePtr converted(SDL_ConvertSurfaceFormat(
		const_cast<SDL_Surface*>(&source_surface),
		SDL_PIXELFORMAT_RGBA32,
		0));
	if (!converted)
	{
		ELYSIA_LOG_WARN("resource","Create coverage mask failed: convert source surface failed: "
			<< SDL_GetError());
		return {};
	}

	SurfacePtr mask(SDL_CreateRGBSurfaceWithFormat(
		0,
		converted->w,
		converted->h,
		32,
		SDL_PIXELFORMAT_RGBA32));
	if (!mask)
	{
		ELYSIA_LOG_WARN("resource","Create coverage mask failed: create surface failed: "
			<< SDL_GetError());
		return {};
	}

	const bool lock_converted = SDL_MUSTLOCK(converted.get()) != 0;
	const bool lock_mask = SDL_MUSTLOCK(mask.get()) != 0;
	if ((lock_converted && SDL_LockSurface(converted.get()) != 0)
		|| (lock_mask && SDL_LockSurface(mask.get()) != 0))
	{
		if (lock_converted && converted->locked)
			SDL_UnlockSurface(converted.get());
		if (lock_mask && mask->locked)
			SDL_UnlockSurface(mask.get());
		ELYSIA_LOG_WARN("resource","Create coverage mask failed: lock surface failed: "
			<< SDL_GetError());
		return {};
	}

	for (int y = 0; y < converted->h; ++y)
	{
		const auto* source_row = static_cast<const std::uint8_t*>(converted->pixels)
			+ static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch);
		auto* mask_row = static_cast<std::uint8_t*>(mask->pixels)
			+ static_cast<std::size_t>(y) * static_cast<std::size_t>(mask->pitch);
		for (int x = 0; x < converted->w; ++x)
		{
			std::uint32_t source_pixel = 0;
			std::memcpy(
				&source_pixel,
				source_row + static_cast<std::size_t>(x) * sizeof(std::uint32_t),
				sizeof(source_pixel));
			Uint8 red = 0;
			Uint8 green = 0;
			Uint8 blue = 0;
			Uint8 alpha = 0;
			SDL_GetRGBA(source_pixel,converted->format,&red,&green,&blue,&alpha);
			const std::uint32_t mask_pixel =
				SDL_MapRGBA(mask->format,255,255,255,alpha);
			std::memcpy(
				mask_row + static_cast<std::size_t>(x) * sizeof(std::uint32_t),
				&mask_pixel,
				sizeof(mask_pixel));
		}
	}

	if (lock_mask)
		SDL_UnlockSurface(mask.get());
	if (lock_converted)
		SDL_UnlockSurface(converted.get());
	return mask;
}


}
