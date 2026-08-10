#include "../../tools/logger.h"
#include "atlas_manager.h"

#include "atlas_builder.h"
#include "../texture/texture_loader.h"
#include "../texture/texture_manager.h"
#include <utility>

namespace elysia::resources
{
namespace
{
TexturePtr create_coverage_mask_texture(
	const TextureLoader& texture_loader,
	SDL_Renderer* renderer,
	const AtlasFramePreparedResult& prepared_result)
{
	if (!prepared_result.coverage_mask_surface)
		return {};

	TexturePtr texture = texture_loader.create_texture(
		renderer,
		*prepared_result.coverage_mask_surface);
	if (!texture
		|| SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_BLEND) != 0)
	{
		return {};
	}
	return texture;
}
}

AtlasManager::AtlasManager(TextureManager& texture_manager)
	: _texture_manager(texture_manager)
{
}

bool AtlasManager::begin_build(const AtlasBuildRequest& request)
{
	if (!request.is_valid())
	{
		ELYSIA_LOG_WARN("resource","Begin atlas build failed: request is invalid: "
			<< request.atlas_key);
		return false;
	}

	if (_atlas_pool.contains(request.atlas_key))
	{
		ELYSIA_LOG_WARN("resource","Begin atlas build failed: atlas already exists: "
			<< request.atlas_key);
		return false;
	}

	if (_assembly_states.contains(request.atlas_key))
	{
		ELYSIA_LOG_WARN("resource","Begin atlas build failed: atlas build already exists: "
			<< request.atlas_key);
		return false;
	}

	AtlasAssemblyState state;
	state.request = request;
	state.committed_frames.resize(request.frame_count);
	state.committed_frame_count = 0;
	state.finalized = false;
	_assembly_states.emplace(request.atlas_key, std::move(state));
	return true;
}

bool AtlasManager::begin_builds(const std::vector<AtlasBuildRequest>& requests)
{
	for (const AtlasBuildRequest& request : requests)
	{
		if (!begin_build(request))
			return false;
	}

	return true;
}

bool AtlasManager::commit_prepared_frame(
	SDL_Renderer* renderer,
	const AtlasFramePreparedResult& prepared_result
)
{
	if (!renderer)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: renderer is null: "
			<< prepared_result.task.atlas_key);
		return false;
	}

	if (prepared_result.task.atlas_key.empty())
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: atlas key is empty.");
		return false;
	}

	std::unordered_map<std::string, AtlasAssemblyState>::iterator iterator =
		_assembly_states.find(prepared_result.task.atlas_key);
	if (iterator == _assembly_states.end())
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: build state does not exist: "
			<< prepared_result.task.atlas_key);
		return false;
	}

	AtlasAssemblyState& state = iterator->second;
	if (state.finalized)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: atlas already finalized: "
			<< prepared_result.task.atlas_key);
		return false;
	}

	if (prepared_result.task.expected_frame_count != state.request.frame_count)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: frame count mismatch: "
			<< prepared_result.task.atlas_key);
		return false;
	}
	if (prepared_result.task.source_type != state.request.source_type)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: source type mismatch: "
			<< prepared_result.task.atlas_key);
		return false;
	}

	if (prepared_result.task.frame_index >= state.committed_frames.size())
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: frame index out of range: "
			<< prepared_result.task.atlas_key << ", frame "
			<< prepared_result.task.frame_index);
		return false;
	}

	const SurfaceLoadResult& surface_result = prepared_result.surface_result;
	if (!surface_result._success || !surface_result._surface)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: prepared surface is invalid: "
			<< prepared_result.task.frame_path);
		return false;
	}

	if (surface_result._asset_key != prepared_result.task.atlas_key)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: asset key mismatch: "
			<< surface_result._asset_key);
		return false;
	}

	if (surface_result._frame_index != prepared_result.task.frame_index)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: frame index mismatch: "
			<< prepared_result.task.atlas_key);
		return false;
	}

	if (state.request.source_type == AtlasSourceType::HorizontalStrip)
	{
		if (prepared_result.task.frame_index != 0)
		{
			ELYSIA_LOG_WARN("resource","Commit horizontal strip failed: task index must be zero: "
				<< prepared_result.task.atlas_key);
			return false;
		}

		const int texture_width = surface_result._surface->w;
		const int texture_height = surface_result._surface->h;
		if (texture_width <= 0 || texture_height <= 0
			|| state.request.frame_count > static_cast<size_t>(texture_width)
			|| static_cast<size_t>(texture_width) % state.request.frame_count != 0)
		{
			ELYSIA_LOG_WARN("resource","Commit horizontal strip failed: image width must be divisible by frame count: "
				<< prepared_result.task.atlas_key << ", width " << texture_width
				<< ", frames " << state.request.frame_count);
			return false;
		}

		const int frame_width = static_cast<int>(
			static_cast<size_t>(texture_width) / state.request.frame_count
		);
		if (frame_width <= 0)
		{
			ELYSIA_LOG_WARN("resource","Commit horizontal strip failed: frame width is zero: "
				<< prepared_result.task.atlas_key);
			return false;
		}

		TextureLoader texture_loader;
		TextureLoadResult texture_result = texture_loader.load_texture(renderer, surface_result);
		if (!texture_result._success || !texture_result._texture)
			return false;
		TexturePtr coverage_mask =
			create_coverage_mask_texture(texture_loader,renderer,prepared_result);
		if (!coverage_mask)
		{
			ELYSIA_LOG_WARN("resource","Commit horizontal strip failed: coverage mask creation failed: "
				<< prepared_result.task.atlas_key);
			return false;
		}

		const std::string texture_key =
			make_strip_texture_key(prepared_result.task.atlas_key);
		SDL_Texture* shared_texture = texture_result._texture.get();
		SDL_Texture* shared_coverage_mask = coverage_mask.get();
		state.pending_textures.push_back(AnimationTextureResource{
			.key = texture_key,
			.texture = std::move(texture_result._texture),
			.coverage_mask = std::move(coverage_mask)
		});

		for (size_t index = 0; index < state.committed_frames.size(); ++index)
		{
			AtlasAssemblyFrame& strip_frame = state.committed_frames[index];
			strip_frame.frame_path = surface_result._frame_path;
			strip_frame.texture = shared_texture;
			strip_frame.coverage_mask = shared_coverage_mask;
			strip_frame.source_rect = elysia::core::Rect(
				static_cast<float>(index * static_cast<size_t>(frame_width)),
				0.0f,
				static_cast<float>(frame_width),
				static_cast<float>(texture_height)
			);
			strip_frame.committed = true;
		}
		state.committed_frame_count = state.request.frame_count;
		return finalize_build(state.request.atlas_key);
	}

	AtlasAssemblyFrame& frame_state =
		state.committed_frames[prepared_result.task.frame_index];
	if (frame_state.committed)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: frame already committed: "
			<< prepared_result.task.atlas_key << ", frame "
			<< prepared_result.task.frame_index);
		return false;
	}

	TextureLoader texture_loader;
	TextureLoadResult texture_result =
		texture_loader.load_texture(renderer, surface_result);
	if (!texture_result._success || !texture_result._texture)
		return false;
	TexturePtr coverage_mask =
		create_coverage_mask_texture(texture_loader,renderer,prepared_result);
	if (!coverage_mask)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: coverage mask creation failed: "
			<< prepared_result.task.atlas_key << ", frame "
			<< prepared_result.task.frame_index);
		return false;
	}

	const std::string texture_key = make_texture_key(
		prepared_result.task.atlas_key,
		prepared_result.task.frame_index
	);
	frame_state.frame_path = surface_result._frame_path;
	frame_state.texture = texture_result._texture.get();
	frame_state.coverage_mask = coverage_mask.get();
	frame_state.committed = true;
	state.pending_textures.push_back(AnimationTextureResource{
		.key = texture_key,
		.texture = std::move(texture_result._texture),
		.coverage_mask = std::move(coverage_mask)
	});
	++state.committed_frame_count;

	if (!frame_state.texture || !frame_state.coverage_mask)
	{
		ELYSIA_LOG_WARN("resource","Commit atlas frame failed: stored texture lookup failed: "
			<< texture_key);
		return false;
	}

	if (state.committed_frame_count == state.request.frame_count)
		return finalize_build(state.request.atlas_key);

	return true;
}

Atlas* AtlasManager::find_atlas(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find atlas failed: key is empty.");
		return nullptr;
	}

	AtlasPool::const_iterator iterator = _atlas_pool.find(std::string(key));
	if (iterator == _atlas_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find atlas failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second.get();
}

bool AtlasManager::has_in_progress_build(const std::string_view& key) const
{
	return _assembly_states.contains(std::string(key));
}

size_t AtlasManager::in_progress_build_count() const
{
	return _assembly_states.size();
}

void AtlasManager::clear()
{
	_assembly_states.clear();
	_atlas_pool.clear();
}

size_t AtlasManager::resource_count() const
{
	return _atlas_pool.size();
}

bool AtlasManager::finalize_build(const std::string& atlas_key)
{
	std::unordered_map<std::string, AtlasAssemblyState>::iterator iterator =
		_assembly_states.find(atlas_key);
	if (iterator == _assembly_states.end())
	{
		ELYSIA_LOG_WARN("resource","Finalize atlas build failed: build state does not exist: "
			<< atlas_key);
		return false;
	}

	AtlasAssemblyState& state = iterator->second;
	if (state.finalized)
	{
		ELYSIA_LOG_WARN("resource","Finalize atlas build failed: atlas already finalized: "
			<< atlas_key);
		return false;
	}

	if (state.committed_frame_count != state.request.frame_count)
	{
		ELYSIA_LOG_WARN("resource","Finalize atlas build failed: committed frame count mismatch: "
			<< atlas_key << ", expected " << state.request.frame_count
			<< ", actual " << state.committed_frame_count);
		return false;
	}

	std::vector<AtlasCommittedFrame> committed_frames;
	committed_frames.reserve(state.committed_frames.size());
	for (size_t index = 0; index < state.committed_frames.size(); ++index)
	{
		const AtlasAssemblyFrame& frame_state = state.committed_frames[index];
		if (!frame_state.committed || !frame_state.texture)
		{
			ELYSIA_LOG_WARN("resource","Finalize atlas build failed: frame is missing: "
				<< atlas_key << ", frame " << index);
			return false;
		}
		if (!frame_state.coverage_mask)
		{
			ELYSIA_LOG_WARN("resource","Finalize atlas build failed: coverage mask is missing: "
				<< atlas_key << ", frame " << index);
			return false;
		}

		AtlasCommittedFrame committed_frame;
		committed_frame.frame_path = frame_state.frame_path;
		committed_frame.texture = frame_state.texture;
		committed_frame.coverage_mask = frame_state.coverage_mask;
		committed_frame.frame_index = index;
		committed_frame.source_rect = frame_state.source_rect;
		committed_frames.push_back(std::move(committed_frame));
	}

	std::unique_ptr<Atlas> atlas = std::make_unique<Atlas>(atlas_key);
	AtlasBuilder atlas_builder;
	if (!atlas_builder.build_atlas(state.request, committed_frames, *atlas))
		return false;
	if (!_texture_manager.store_animation_textures(
		std::move(state.pending_textures)))
	{
		return false;
	}

	state.finalized = true;
	_atlas_pool.emplace(atlas_key, std::move(atlas));
	_assembly_states.erase(iterator);
	return true;
}

std::string AtlasManager::make_texture_key(
	const std::string& atlas_key,
	size_t frame_index
) const
{
	return atlas_key + "#" + std::to_string(frame_index);
}

std::string AtlasManager::make_strip_texture_key(const std::string& atlas_key) const
{
	return atlas_key + "#strip";
}

}
