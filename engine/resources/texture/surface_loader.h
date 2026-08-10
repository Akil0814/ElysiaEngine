#pragma once

#include <SDL.h>

#include <filesystem>
#include <expected>
#include <memory>
#include <string>

#include "../resource_failure.h"

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
	std::string _asset_key;
	std::filesystem::path _frame_path;
	size_t _frame_index = 0;
	SurfacePtr _surface;
};

class SurfaceLoader
{
public:
	[[nodiscard]] std::expected<SurfaceLoadResult,ResourceFailure>
	load_surface(const SurfaceLoadRequest& request) const;
};

[[nodiscard]] std::expected<SurfacePtr,ResourceFailure> create_coverage_mask_surface(
	const SDL_Surface& source_surface);

}
