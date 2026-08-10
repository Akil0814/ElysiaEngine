#include "../tools/logger.h"
#include "game_content_loader.h"

#include "content_manifest_pipeline.h"
#include "content_runtime_cleanup.h"
#include "resource_request_assembler.h"
#include "../animation/runtime/animation_manager.h"
#include "../config/config_service.h"
#include "../effects/runtime/effect_manager.h"
#include "../io/path/path_manager.h"
#include "../resources/resource_service.h"
#include "../resources/runtime/resource_manager.h"
#include "../resources/texture/surface_loader.h"
#include "../resources/texture/texture_loader.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace elysia::loading
{
namespace
{
constexpr std::size_t kMaxInFlightPrepareJobs = 16;
constexpr std::size_t kTextureCommitBudgetPerUpdate = 4;
constexpr std::size_t kAtlasFrameCommitBudgetPerUpdate = 8;
constexpr std::size_t kAudioLoadBudgetPerUpdate = 8;

std::size_t resolve_worker_count(std::size_t total_prepare_jobs)
{
	if (total_prepare_jobs == 0)
		return 0;

	std::size_t worker_count = std::thread::hardware_concurrency();
	if (worker_count == 0)
		worker_count = 2;

	worker_count = std::min<std::size_t>(worker_count, 4);
	return std::min(worker_count, total_prepare_jobs);
}
}

GameContentLoader::~GameContentLoader()
{
	reset();
}

void GameContentLoader::reset()
{
	const bool was_loading = is_running();
	shutdown_worker_threads();
	reset_streaming_state();
	if (was_loading)
		clear_loaded_content();
	_renderer = nullptr;
	_load_plan.clear();
	_config_snapshot.reset();
	_error_message.clear();
	_state = GameContentLoaderState::Idle;
	_progress = 0.0f;
	_total_work_units = 0;
	_completed_work_units = 0;
}

bool GameContentLoader::start(
	SDL_Renderer* renderer,
	const elysia::io::ContentRegistry& content_registry,
	std::span<const int> project_font_point_sizes)
{
	reset();
	clear_loaded_content();

	if (!renderer)
	{
		fail("GameContentLoader start failed: renderer is null.");
		return false;
	}

	_renderer = renderer;
	_state = GameContentLoaderState::PreparingRequests;

	elysia::io::PathManager* path_manager = elysia::io::PathManager::instance();
	if (!path_manager->is_initialized())
	{
		fail("GameContentLoader start failed: path manager is not init.");
		return false;
	}

	ContentManifestPipeline content_manifest_pipeline;
	ContentManifestResult config_result;
	if (!content_manifest_pipeline.load(content_registry, config_result))
	{
		fail(content_manifest_pipeline.error_message());
		return false;
	}
	_config_snapshot = config_result.config_snapshot;

	ResourceRequestAssembler assembler;
	if (!assembler.assemble(
		config_result,
		project_font_point_sizes,
		_load_plan))
	{
		fail("GameContentLoader start failed: " + assembler.error_message());
		return false;
	}

	if (!initialize_streaming_work())
		return false;

	_state = GameContentLoaderState::StreamingTextureAndAtlasWork;
	_progress = 0.0f;

	ELYSIA_LOG("resource","Total requests built: " << _load_plan.total_request_count()
		<< ", total work units: " << _total_work_units);
	return true;
}

void GameContentLoader::update()
{
	if (_state == GameContentLoaderState::Idle
		|| _state == GameContentLoaderState::Finished
		|| _state == GameContentLoaderState::Failed)
	{
		return;
	}

	if (_state == GameContentLoaderState::StreamingTextureAndAtlasWork)
	{
		if (!_renderer)
		{
			fail("GameContentLoader update failed: renderer is null.");
			return;
		}

		dispatch_prepare_jobs();
		drain_completed_prepare_results();
		if (!commit_ready_streaming_results())
			return;

		if (is_streaming_phase_complete())
		{
			shutdown_worker_threads();
			_state = GameContentLoaderState::LoadingFonts;
		}

		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::LoadingFonts)
	{
		if (!load_fonts())
			return;

		_state = GameContentLoaderState::LoadingAudio;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::LoadingAudio)
	{
		if (!load_audio())
			return;

		_state = GameContentLoaderState::RegisteringAnimations;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::RegisteringAnimations)
	{
		if (!register_animations())
			return;

		_state = GameContentLoaderState::RegisteringEffects;
		update_progress_value();
		return;
	}

	if (_state == GameContentLoaderState::RegisteringEffects)
	{
		if (!register_animation_effects())
			return;
		if (!_config_snapshot)
		{
			fail("GameContentLoader config publish failed: snapshot is missing.");
			return;
		}
		elysia::config::ConfigService::instance()->publish(_config_snapshot);

		_state = GameContentLoaderState::Finished;
		_progress = 1.0f;
	}
}

bool GameContentLoader::is_running() const
{
	return _state == GameContentLoaderState::PreparingRequests
		|| _state == GameContentLoaderState::StreamingTextureAndAtlasWork
		|| _state == GameContentLoaderState::LoadingFonts
		|| _state == GameContentLoaderState::LoadingAudio
		|| _state == GameContentLoaderState::RegisteringAnimations
		|| _state == GameContentLoaderState::RegisteringEffects;
}

bool GameContentLoader::is_finished() const
{
	return _state == GameContentLoaderState::Finished;
}

bool GameContentLoader::has_failed() const
{
	return _state == GameContentLoaderState::Failed;
}

float GameContentLoader::progress() const
{
	return _progress;
}

const std::string& GameContentLoader::error_message() const
{
	return _error_message;
}

GameContentLoaderState GameContentLoader::state() const
{
	return _state;
}

bool GameContentLoader::initialize_streaming_work()
{
	elysia::resources::AtlasBuildPreparer atlas_build_preparer;
	std::vector<elysia::resources::AtlasFramePrepareTask> atlas_frame_tasks;
	for (const elysia::resources::AtlasBuildRequest& request : _load_plan.atlas_build_requests())
	{
		std::vector<elysia::resources::AtlasFramePrepareTask> expanded_tasks;
		if (!atlas_build_preparer.expand_build_request(request, expanded_tasks))
		{
			fail("GameContentLoader start failed: atlas build request expansion failed.");
			return false;
		}

		atlas_frame_tasks.insert(
			atlas_frame_tasks.end(),
			std::make_move_iterator(expanded_tasks.begin()),
			std::make_move_iterator(expanded_tasks.end())
		);
	}

	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	if (!resource_manager->begin_atlas_builds(_load_plan.atlas_build_requests()))
	{
		fail("GameContentLoader start failed: atlas build initialization failed.");
		return false;
	}

	_atlas_frame_tasks = std::move(atlas_frame_tasks);
	_total_work_units = _load_plan.texture_requests().size()
		+ _atlas_frame_tasks.size()
		+ _load_plan.font_requests().size()
		+ _load_plan.sound_requests().size()
		+ _load_plan.music_requests().size()
		+ _load_plan.animation_build_requests().size()
		+ _load_plan.animation_effect_build_requests().size();

	start_worker_threads();
	return true;
}

void GameContentLoader::reset_streaming_state()
{
	_atlas_frame_tasks.clear();
	_next_texture_request_index = 0;
	_next_atlas_frame_task_index = 0;
	_dispatch_texture_turn = true;
	_prepare_jobs.clear();
	_completed_texture_results.clear();
	_completed_atlas_frame_results.clear();
	_ready_texture_results.clear();
	_ready_atlas_frame_results.clear();
	_in_flight_prepare_job_count.store(0);
	_stop_workers.store(false);
	_prepared_texture_count = 0;
	_prepared_atlas_frame_count = 0;
	_committed_texture_count = 0;
	_committed_atlas_frame_count = 0;
	_next_sound_request_index = 0;
	_next_music_request_index = 0;
}

void GameContentLoader::start_worker_threads()
{
	const std::size_t total_prepare_jobs =
		_load_plan.texture_requests().size() + _atlas_frame_tasks.size();
	const std::size_t worker_count = resolve_worker_count(total_prepare_jobs);
	if (worker_count == 0)
		return;

	_worker_threads.reserve(worker_count);
	for (std::size_t index = 0; index < worker_count; ++index)
		_worker_threads.emplace_back(&GameContentLoader::worker_loop, this);
}

void GameContentLoader::shutdown_worker_threads()
{
	_stop_workers.store(true);
	_prepare_cv.notify_all();

	for (std::thread& worker : _worker_threads)
	{
		if (worker.joinable())
			worker.join();
	}
	_worker_threads.clear();
}

void GameContentLoader::worker_loop()
{
	elysia::resources::SurfaceLoader surface_loader;
	elysia::resources::AtlasBuildPreparer atlas_build_preparer;

	for (;;)
	{
		PrepareJob job;
		{
			std::unique_lock<std::mutex> lock(_prepare_mutex);
			_prepare_cv.wait(lock, [this]()
			{
				return _stop_workers.load() || !_prepare_jobs.empty();
			});

			if (_stop_workers.load() && _prepare_jobs.empty())
				return;

			job = std::move(_prepare_jobs.front());
			_prepare_jobs.pop_front();
		}

		if (std::holds_alternative<elysia::resources::TextureLoadRequest>(job.payload))
		{
			const elysia::resources::TextureLoadRequest& texture_request =
				std::get<elysia::resources::TextureLoadRequest>(job.payload);

			elysia::resources::SurfaceLoadRequest surface_request;
			surface_request._asset_key = texture_request.key;
			surface_request._frame_path = texture_request.file_path;
			surface_request._frame_index = 0;

			elysia::resources::SurfaceLoadResult surface_result =
				surface_loader.load_surface(surface_request);
			{
				std::lock_guard<std::mutex> lock(_completed_results_mutex);
				_completed_texture_results.push_back(std::move(surface_result));
			}
		}
		else
		{
			elysia::resources::AtlasFramePreparedResult prepared_result =
				atlas_build_preparer.prepare_frame(
					std::get<elysia::resources::AtlasFramePrepareTask>(job.payload)
				);
			{
				std::lock_guard<std::mutex> lock(_completed_results_mutex);
				_completed_atlas_frame_results.push_back(std::move(prepared_result));
			}
		}

		_in_flight_prepare_job_count.fetch_sub(1);
	}
}

void GameContentLoader::dispatch_prepare_jobs()
{
	auto try_enqueue_texture = [this]() -> bool
	{
		if (_next_texture_request_index >= _load_plan.texture_requests().size())
			return false;

		PrepareJob job;
		job.payload = _load_plan.texture_requests()[_next_texture_request_index++];
		{
			std::lock_guard<std::mutex> lock(_prepare_mutex);
			_prepare_jobs.push_back(std::move(job));
		}

		_in_flight_prepare_job_count.fetch_add(1);
		_prepare_cv.notify_one();
		return true;
	};

	auto try_enqueue_atlas = [this]() -> bool
	{
		if (_next_atlas_frame_task_index >= _atlas_frame_tasks.size())
			return false;

		PrepareJob job;
		job.payload = _atlas_frame_tasks[_next_atlas_frame_task_index++];
		{
			std::lock_guard<std::mutex> lock(_prepare_mutex);
			_prepare_jobs.push_back(std::move(job));
		}

		_in_flight_prepare_job_count.fetch_add(1);
		_prepare_cv.notify_one();
		return true;
	};

	while (_in_flight_prepare_job_count.load() < kMaxInFlightPrepareJobs)
	{
		bool enqueued = false;
		if (_dispatch_texture_turn)
			enqueued = try_enqueue_texture() || try_enqueue_atlas();
		else
			enqueued = try_enqueue_atlas() || try_enqueue_texture();

		if (!enqueued)
			break;

		_dispatch_texture_turn = !_dispatch_texture_turn;
	}
}

void GameContentLoader::drain_completed_prepare_results()
{
	std::lock_guard<std::mutex> lock(_completed_results_mutex);

	while (!_completed_texture_results.empty())
	{
		_ready_texture_results.push_back(std::move(_completed_texture_results.front()));
		_completed_texture_results.pop_front();
		++_prepared_texture_count;
	}

	while (!_completed_atlas_frame_results.empty())
	{
		_ready_atlas_frame_results.push_back(
			std::move(_completed_atlas_frame_results.front())
		);
		_completed_atlas_frame_results.pop_front();
		++_prepared_atlas_frame_count;
	}
}

bool GameContentLoader::commit_ready_streaming_results()
{
	std::size_t committed_texture_count = 0;
	while (committed_texture_count < kTextureCommitBudgetPerUpdate
		&& !_ready_texture_results.empty())
	{
		elysia::resources::SurfaceLoadResult surface_result = std::move(_ready_texture_results.front());
		_ready_texture_results.pop_front();
		if (!commit_texture_result(surface_result))
			return false;

		++committed_texture_count;
	}

	std::size_t committed_atlas_count = 0;
	while (committed_atlas_count < kAtlasFrameCommitBudgetPerUpdate
		&& !_ready_atlas_frame_results.empty())
	{
		elysia::resources::AtlasFramePreparedResult prepared_result =
			std::move(_ready_atlas_frame_results.front());
		_ready_atlas_frame_results.pop_front();
		if (!commit_atlas_frame_result(prepared_result))
			return false;

		++committed_atlas_count;
	}

	return true;
}

bool GameContentLoader::commit_texture_result(const elysia::resources::SurfaceLoadResult& surface_result)
{
	if (!surface_result._success || !surface_result._surface)
	{
		fail("GameContentLoader texture commit failed: prepared surface is invalid.");
		return false;
	}

	elysia::resources::TextureLoader texture_loader;
	elysia::resources::TextureLoadResult texture_result =
		texture_loader.load_texture(_renderer, surface_result);
	if (!texture_result._success || !texture_result._texture)
	{
		fail("GameContentLoader texture commit failed: texture creation failed.");
		return false;
	}

	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	if (!resource_manager->store_texture(
		surface_result._asset_key,
		std::move(texture_result._texture)))
	{
		fail("GameContentLoader texture commit failed: texture store failed.");
		return false;
	}

	++_committed_texture_count;
	++_completed_work_units;
	return true;
}

bool GameContentLoader::commit_atlas_frame_result(
	const elysia::resources::AtlasFramePreparedResult& prepared_result
)
{
	if (!prepared_result.surface_result._success || !prepared_result.surface_result._surface)
	{
		fail("GameContentLoader atlas frame commit failed: prepared surface is invalid.");
		return false;
	}

	if (!elysia::resources::ResourceManager::instance()->commit_prepared_atlas_frame(
		_renderer,
		prepared_result))
	{
		fail("GameContentLoader atlas frame commit failed: atlas manager commit failed.");
		return false;
	}

	++_committed_atlas_frame_count;
	++_completed_work_units;
	return true;
}

bool GameContentLoader::is_streaming_phase_complete()
{
	if (_next_texture_request_index != _load_plan.texture_requests().size())
		return false;

	if (_next_atlas_frame_task_index != _atlas_frame_tasks.size())
		return false;

	if (_committed_texture_count != _load_plan.texture_requests().size())
		return false;

	if (_committed_atlas_frame_count != _atlas_frame_tasks.size())
		return false;

	if (_in_flight_prepare_job_count.load() != 0)
		return false;

	if (!_ready_texture_results.empty() || !_ready_atlas_frame_results.empty())
		return false;

	{
		std::lock_guard<std::mutex> lock(_completed_results_mutex);
		if (!_completed_texture_results.empty()
			|| !_completed_atlas_frame_results.empty())
		{
			return false;
		}
	}

	return !elysia::resources::ResourceManager::instance()->has_in_progress_atlas_builds();
}

bool GameContentLoader::load_fonts()
{
	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	for (const elysia::resources::FontLoadRequest& request : _load_plan.font_requests())
	{
		if (!resource_manager->load_font(request.key, request.file_path, request.point_size))
		{
			fail("GameContentLoader font load failed.");
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

bool GameContentLoader::load_audio()
{
	elysia::resources::ResourceManager* resource_manager = elysia::resources::ResourceManager::instance();
	std::size_t loaded_this_update = 0;

	while (_next_sound_request_index < _load_plan.sound_requests().size()
		&& loaded_this_update < kAudioLoadBudgetPerUpdate)
	{
		const elysia::resources::SoundLoadRequest& request =
			_load_plan.sound_requests()[_next_sound_request_index];
		if (!resource_manager->load_sound(request))
		{
			fail("GameContentLoader sound load failed.");
			return false;
		}

		++_next_sound_request_index;
		++loaded_this_update;
		++_completed_work_units;
	}

	while (_next_music_request_index < _load_plan.music_requests().size()
		&& loaded_this_update < kAudioLoadBudgetPerUpdate)
	{
		const elysia::resources::MusicLoadRequest& request =
			_load_plan.music_requests()[_next_music_request_index];
		if (!resource_manager->load_music(request))
		{
			fail("GameContentLoader music load failed.");
			return false;
		}

		++_next_music_request_index;
		++loaded_this_update;
		++_completed_work_units;
	}

	update_progress_value();
	return _next_sound_request_index == _load_plan.sound_requests().size()
		&& _next_music_request_index == _load_plan.music_requests().size();
}

bool GameContentLoader::register_animations()
{
	elysia::animation::AnimationManager* animation_manager = elysia::animation::AnimationManager::instance();
	for (const elysia::resources::AnimationBuildRequest& request : _load_plan.animation_build_requests())
	{
		const elysia::resources::Atlas* atlas =
			elysia::resources::ResourceService::instance()->find_atlas(request.atlas_key);
		if (!animation_manager->register_animation(request, atlas))
		{
			fail("GameContentLoader animation registration failed.");
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

bool GameContentLoader::register_animation_effects()
{
	elysia::effects::EffectManager* effect_manager = elysia::effects::EffectManager::instance();
	for (const elysia::resources::AnimationEffectBuildRequest& request : _load_plan.animation_effect_build_requests())
	{
		if (!effect_manager->register_animation_effect(request))
		{
			fail("GameContentLoader effect registration failed.");
			return false;
		}

		++_completed_work_units;
	}

	return true;
}

void GameContentLoader::update_progress_value()
{
	if (_state == GameContentLoaderState::Finished)
	{
		_progress = 1.0f;
		return;
	}

	if (_total_work_units == 0)
	{
		_progress = 0.0f;
		return;
	}

	const float ratio = static_cast<float>(_completed_work_units)
		/ static_cast<float>(_total_work_units);
	_progress = std::clamp(ratio, 0.0f, 1.0f);
}

void GameContentLoader::fail(std::string message)
{
	shutdown_worker_threads();
	clear_loaded_content();
	_config_snapshot.reset();
	_error_message = std::move(message);
	_state = GameContentLoaderState::Failed;
	update_progress_value();
	ELYSIA_LOG_ERROR("resource",_error_message);
}


}
