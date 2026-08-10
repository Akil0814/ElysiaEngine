#pragma once

#include "atlas.h"
#include "../resource_types.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace elysia::resources
{
struct AtlasCommittedFrame
{
	std::filesystem::path frame_path;
	SDL_Texture* texture = nullptr;
	SDL_Texture* coverage_mask = nullptr;
	size_t frame_index = 0;
	std::optional<elysia::core::Rect> source_rect;
};

class AtlasBuilder
{
public:
	bool build_atlas(
		const AtlasBuildRequest& request,
		const std::vector<AtlasCommittedFrame>& committed_frames,
		Atlas& atlas
	) const;
};

}
