#include "../../tools/logger.h"
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
		ELYSIA_LOG_WARN("resource","Build atlas failed: atlas key is empty.");
		return false;
	}

	if (request.frame_count == 0)
	{
		ELYSIA_LOG_WARN("resource","Build atlas failed: frame count is zero: "
			<< request.atlas_key);
		return false;
	}

	if (committed_frames.size() != request.frame_count)
	{
		ELYSIA_LOG_WARN("resource","Build atlas failed: texture count mismatch: "
			<< request.atlas_key << ", expected " << request.frame_count
			<< ", actual " << committed_frames.size());
		return false;
	}

	atlas.clear();
	atlas.set_name(request.atlas_key);

	for (size_t index = 0; index < committed_frames.size(); ++index)
	{
		const AtlasCommittedFrame& committed_frame = committed_frames[index];
		if (!committed_frame.texture)
		{
			ELYSIA_LOG_WARN("resource","Build atlas failed: texture is invalid: "
				<< request.atlas_key << ", frame " << index);
			return false;
		}
		if (!committed_frame.coverage_mask)
		{
			ELYSIA_LOG_WARN("resource","Build atlas failed: coverage mask is invalid: "
				<< request.atlas_key << ", frame " << index);
			return false;
		}

		if (committed_frame.frame_index != index)
		{
			ELYSIA_LOG_WARN("resource","Build atlas failed: frame index mismatch: "
				<< request.atlas_key << ", expected " << index
				<< ", actual " << committed_frame.frame_index);
			return false;
		}

		if (!atlas.add_frame(
			committed_frame.frame_path,
			committed_frame.texture,
			committed_frame.coverage_mask,
			committed_frame.source_rect))
		{
			ELYSIA_LOG_WARN("resource","Build atlas failed: add frame failed: "
				<< committed_frame.frame_path);
			return false;
		}
	}

	return true;
}

}
