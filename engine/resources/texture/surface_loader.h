#pragma once

#include <SDL.h>

#include <filesystem>
#include <memory>
#include <string>

namespace elysia::resources
{
struct SurfaceDeleter
{
	void operator()(SDL_Surface* surface) const;
};

using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

struct SurfaceLoadRequest
{
	std::string _asset_key;
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
};

struct SurfaceLoadResult
{
	bool _success = false;
	std::string _asset_key;
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
	SurfacePtr _surface;
};

class SurfaceLoader
{
public:
	SurfaceLoadResult load_surface(const SurfaceLoadRequest& request) const;
};

[[nodiscard]] SurfacePtr create_coverage_mask_surface(
	const SDL_Surface& source_surface);

}
