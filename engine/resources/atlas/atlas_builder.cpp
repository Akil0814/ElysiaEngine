#include "atlas_builder.h"
namespace elysia::resources
{
bool AtlasBuilder::build_atlas(
	const AtlasBuildRequest& request,
	const std::vector<AtlasCommittedFrame>& committed_frames,
	Atlas& atlas
) const
{
	if (request.atlas_key.empty())
	{
		return false;
	}

	if (request.frame_count == 0)
	{
		return false;
	}

	if (committed_frames.size() != request.frame_count)
	{
		return false;
	}

	atlas.clear();
	atlas.set_name(request.atlas_key);

	for (size_t index = 0; index < committed_frames.size(); ++index)
	{
		const AtlasCommittedFrame& committed_frame = committed_frames[index];
		if (!committed_frame.texture)
		{
			return false;
		}
		if (!committed_frame.coverage_mask)
		{
			return false;
		}

		if (committed_frame.frame_index != index)
		{
			return false;
		}

		if (!atlas.add_frame(
			committed_frame.frame_path,
			committed_frame.texture,
			committed_frame.coverage_mask,
			committed_frame.source_rect))
		{
			return false;
		}
	}

	return true;
}

}
