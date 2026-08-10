#include "../../tools/logger.h"
#include "atlas_build_preparer.h"

#include "../texture/surface_loader.h"

#include <iomanip>
#include <sstream>
namespace elysia::resources
{
namespace
{
std::filesystem::path make_frame_path(
	const std::filesystem::path& directory_path,
	const std::string& filename_prefix,
	size_t frame_index
)
{
	std::ostringstream filename;
	filename << filename_prefix << '_' << std::setw(3) << std::setfill('0')
		<< frame_index << ".png";
	return directory_path / filename.str();
}
}

bool AtlasBuildPreparer::expand_build_request(
	const AtlasBuildRequest& request,
	std::vector<AtlasFramePrepareTask>& out_tasks
) const
{
	out_tasks.clear();

	if (!request.is_valid())
	{
		ELYSIA_LOG_WARN("resource","Expand atlas build request failed: request is invalid: "
			<< request.atlas_key);
		return false;
	}

	if (request.source_type == AtlasSourceType::HorizontalStrip)
	{
		if (!std::filesystem::is_regular_file(request.source_path))
		{
			ELYSIA_LOG_WARN("resource","Expand atlas build request failed: horizontal strip does not exist: "
				<< request.source_path);
			return false;
		}

		AtlasFramePrepareTask task;
		task.atlas_key = request.atlas_key;
		task.frame_path = request.source_path;
		task.frame_index = 0;
		task.expected_frame_count = request.frame_count;
		task.source_type = request.source_type;
		out_tasks.push_back(std::move(task));
		return true;
	}

	if (!std::filesystem::is_directory(request.source_path))
	{
		ELYSIA_LOG_WARN("resource","Expand atlas build request failed: directory does not exist: "
			<< request.source_path);
		return false;
	}

	std::vector<std::filesystem::path> frame_paths;
	frame_paths.reserve(request.frame_count);
	for (size_t index = 0; index < request.frame_count; ++index)
	{
		std::filesystem::path frame_path = make_frame_path(
			request.source_path,
			request.frame_filename_prefix,
			index
		);
		if (!std::filesystem::is_regular_file(frame_path))
		{
			ELYSIA_LOG_WARN("resource","Expand atlas build request failed: expected frame is missing: "
				<< frame_path);
			return false;
		}
		frame_paths.push_back(std::move(frame_path));
	}

	out_tasks.reserve(frame_paths.size());
	for (size_t index = 0; index < frame_paths.size(); ++index)
	{
		AtlasFramePrepareTask task;
		task.atlas_key = request.atlas_key;
		task.frame_path = frame_paths[index];
		task.frame_index = index;
		task.expected_frame_count = request.frame_count;
		task.source_type = request.source_type;
		out_tasks.push_back(std::move(task));
	}

	return true;
}

AtlasFramePreparedResult AtlasBuildPreparer::prepare_frame(
	const AtlasFramePrepareTask& task
) const
{
	AtlasFramePreparedResult result;
	result.task = task;

	if (task.atlas_key.empty())
	{
		ELYSIA_LOG_WARN("resource","Prepare atlas frame failed: atlas key is empty.");
		return result;
	}

	if (task.frame_path.empty())
	{
		ELYSIA_LOG_WARN("resource","Prepare atlas frame failed: frame path is empty: "
			<< task.atlas_key);
		return result;
	}

	if (task.expected_frame_count == 0)
	{
		ELYSIA_LOG_WARN("resource","Prepare atlas frame failed: expected frame count is zero: "
			<< task.atlas_key);
		return result;
	}

	SurfaceLoadRequest surface_request;
	surface_request._asset_key = task.atlas_key;
	surface_request._frame_path = task.frame_path;
	surface_request._frame_index = task.frame_index;

	SurfaceLoader surface_loader;
	result.surface_result = surface_loader.load_surface(surface_request);
	if (result.surface_result._success && result.surface_result._surface)
	{
		result.coverage_mask_surface =
			create_coverage_mask_surface(*result.surface_result._surface);
		if (!result.coverage_mask_surface)
		{
			result.surface_result._success = false;
			result.surface_result._surface.reset();
		}
	}
	return result;
}

}
