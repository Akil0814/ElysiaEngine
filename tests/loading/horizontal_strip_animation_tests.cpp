#define SDL_MAIN_HANDLED

#include "engine/animation/animation_service.h"
#include "engine/animation/runtime/animation_manager.h"
#include "engine/camera/camera.h"
#include "engine/core/render/render_command_projection.h"
#include "engine/core/render/sdl_render_command_executor.h"
#include "engine/io/loaders/animation_effect_manifest_loader.h"
#include "engine/io/loaders/animation_manifest_loader.h"
#include "engine/io/loaders/entity_manifest_loader.h"
#include "engine/io/loaders/fonts_manifest_loader.h"
#include "engine/io/loaders/i18n_manifest_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/resources/atlas/atlas_build_preparer.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/resources/texture/surface_loader.h"
#include "engine/ui/widgets/image/ui_animation.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace
{
using elysia::tests::require;

std::uint32_t read_surface_pixel(
	const SDL_Surface& surface,
	int x,
	int y)
{
	std::uint32_t pixel = 0;
	const auto* row = static_cast<const std::uint8_t*>(surface.pixels)
		+ y * surface.pitch;
	std::memcpy(
		&pixel,
		row + x * surface.format->BytesPerPixel,
		sizeof(pixel));
	return pixel;
}

void write_surface_pixel(
	SDL_Surface& surface,
	int x,
	int y,
	std::uint32_t pixel)
{
	auto* row = static_cast<std::uint8_t*>(surface.pixels)
		+ y * surface.pitch;
	std::memcpy(
		row + x * surface.format->BytesPerPixel,
		&pixel,
		sizeof(pixel));
}

void test_coverage_mask_surface_conversion()
{
	using namespace elysia;

	require(SDL_Init(SDL_INIT_VIDEO) == 0,
		"coverage mask tests must initialize SDL video");
	resources::SurfacePtr source(SDL_CreateRGBSurfaceWithFormat(
		0,3,1,32,SDL_PIXELFORMAT_RGBA32));
	require(source != nullptr,
		"coverage mask tests must create a source surface");

	const std::array<Uint8,3> alpha_values{ 0,127,255 };
	for (int index = 0; index < 3; ++index)
	{
		write_surface_pixel(
			*source,
			index,
			0,
			SDL_MapRGBA(
				source->format,
				static_cast<Uint8>(10 + index),
				static_cast<Uint8>(20 + index),
				static_cast<Uint8>(30 + index),
				alpha_values[index]));
	}
	const std::array<std::uint32_t,3> source_pixels{
		read_surface_pixel(*source,0,0),
		read_surface_pixel(*source,1,0),
		read_surface_pixel(*source,2,0)
	};

	resources::SurfacePtr mask =
		resources::create_coverage_mask_surface(*source);
	require(mask != nullptr && mask->w == source->w && mask->h == source->h,
		"coverage mask must preserve source dimensions");
	for (int index = 0; index < 3; ++index)
	{
		Uint8 red = 0;
		Uint8 green = 0;
		Uint8 blue = 0;
		Uint8 alpha = 0;
		SDL_GetRGBA(
			read_surface_pixel(*mask,index,0),
			mask->format,
			&red,
			&green,
			&blue,
			&alpha);
		require(red == 255 && green == 255 && blue == 255
				&& alpha == alpha_values[index],
			"coverage mask pixels must be white while preserving alpha");
		require(read_surface_pixel(*source,index,0) == source_pixels[index],
			"coverage mask conversion must not modify its source surface");
	}

	SDL_Quit();
}

void test_horizontal_strip_manifest_schema()
{
	const std::filesystem::path test_root =
		std::filesystem::temp_directory_path() / "elysia_horizontal_strip_json_tests";
	std::filesystem::remove_all(test_root);
	std::filesystem::create_directories(test_root);

	const std::filesystem::path animation_manifest_path = test_root / "animations.json";
	std::ofstream(animation_manifest_path)
		<< R"({"animations":[{"key":"bad","path":"bad.png","frame_count":1,"fps":10,"loop":true,"horizontal_strip":"yes"}]})";
	elysia::io::AnimationManifest animation_manifest;
	require(!elysia::io::AnimationManifestLoader{}.load(animation_manifest_path, animation_manifest),
		"animation manifest horizontal_strip must reject non-boolean values");

	const std::filesystem::path directory_without_prefix_path = test_root / "directory_without_prefix.json";
	std::ofstream(directory_without_prefix_path)
		<< R"({"animations":[{"key":"core.idle","path":"idle","frame_count":2,"fps":10,"loop":true}]})";
	require(!elysia::io::AnimationManifestLoader{}.load(directory_without_prefix_path, animation_manifest),
		"core frame-directory animations must require frame_prefix");

	const std::filesystem::path strip_with_prefix_path = test_root / "strip_with_prefix.json";
	std::ofstream(strip_with_prefix_path)
		<< R"({"animations":[{"key":"core.strip","path":"strip.png","frame_count":2,"fps":10,"loop":true,"horizontal_strip":true,"frame_prefix":"strip"}]})";
	require(!elysia::io::AnimationManifestLoader{}.load(strip_with_prefix_path, animation_manifest),
		"core horizontal-strip animations must reject frame_prefix");
	const std::filesystem::path strip_with_empty_prefix_path = test_root / "strip_with_empty_prefix.json";
	std::ofstream(strip_with_empty_prefix_path)
		<< R"({"animations":[{"key":"core.strip","path":"strip.png","frame_count":2,"fps":10,"loop":true,"horizontal_strip":true,"frame_prefix":""}]})";
	require(!elysia::io::AnimationManifestLoader{}.load(strip_with_empty_prefix_path, animation_manifest),
		"core horizontal-strip animations must reject even an explicitly empty frame_prefix field");

	const std::filesystem::path valid_directory_path = test_root / "valid_directory.json";
	std::ofstream(valid_directory_path)
		<< R"({"animations":[{"key":"core.idle","path":"idle","frame_count":2,"fps":10,"loop":true,"frame_prefix":"core_idle"}]})";
	require(elysia::io::AnimationManifestLoader{}.load(valid_directory_path, animation_manifest)
		&& animation_manifest.animations.size() == 1
		&& animation_manifest.animations.front().frame_prefix == "core_idle"
		&& !animation_manifest.animations.front().horizontal_strip,
		"core frame-directory animations must accept an explicit frame_prefix");

	const std::filesystem::path valid_strip_path = test_root / "valid_strip.json";
	std::ofstream(valid_strip_path)
		<< R"({"animations":[{"key":"core.strip","path":"strip.png","frame_count":2,"fps":10,"loop":true,"horizontal_strip":true}]})";
	require(elysia::io::AnimationManifestLoader{}.load(valid_strip_path, animation_manifest)
		&& animation_manifest.animations.size() == 1
		&& animation_manifest.animations.front().frame_prefix.empty()
		&& animation_manifest.animations.front().horizontal_strip,
		"core horizontal-strip animations must load without a frame_prefix");

	const std::filesystem::path entity_manifest_path = test_root / "legacy_asset_key_entity.json";
	std::ofstream(entity_manifest_path)
		<< R"({"entities":[{"id":"Bad","asset_key":"Bad"}]})";
	elysia::io::EntityManifest entity_manifest;
	require(!elysia::io::EntityManifestLoader{}.load(entity_manifest_path, entity_manifest),
		"entity manifests must reject the removed asset_key field");
	const std::filesystem::path removed_horizontal_strip_entity_path = test_root / "horizontal_strip_entity.json";
	std::ofstream(removed_horizontal_strip_entity_path)
		<< R"({"entities":[{"id":"Bad","horizontal_strip":true}]})";
	require(!elysia::io::EntityManifestLoader{}.load(removed_horizontal_strip_entity_path, entity_manifest),
		"entity manifests must reject the removed horizontal_strip field");
	const std::filesystem::path missing_id_entity_path = test_root / "missing_id_entity.json";
	std::ofstream(missing_id_entity_path)
		<< R"({"entities":[{"enabled":true}]})";
	require(!elysia::io::EntityManifestLoader{}.load(missing_id_entity_path, entity_manifest),
		"entity manifests must reject entries without id");
	const std::filesystem::path unknown_entity_field_path = test_root / "unknown_entity_field.json";
	std::ofstream(unknown_entity_field_path)
		<< R"({"entities":[{"id":"Bad","unknown":true}]})";
	require(!elysia::io::EntityManifestLoader{}.load(unknown_entity_field_path, entity_manifest),
		"entity manifests must reject unknown fields");
	const std::filesystem::path disabled_invalid_entity_path = test_root / "disabled_invalid_entity.json";
	std::ofstream(disabled_invalid_entity_path)
		<< R"({"entities":[{"id":"bad-id","animation_layout":1,"enabled":false}]})";
	require(!elysia::io::EntityManifestLoader{}.load(disabled_invalid_entity_path, entity_manifest),
		"disabled entities must still validate all configured fields and key components");
	const std::filesystem::path duplicate_entity_path = test_root / "duplicate_entity.json";
	std::ofstream(duplicate_entity_path)
		<< R"({"entities":[{"id":"duplicate"},{"id":"duplicate"}]})";
	std::ostringstream duplicate_entity_log;
	std::streambuf* previous_log_buffer = std::clog.rdbuf(duplicate_entity_log.rdbuf());
	const bool duplicate_entity_loaded =
		elysia::io::EntityManifestLoader{}.load(duplicate_entity_path, entity_manifest);
	std::clog.rdbuf(previous_log_buffer);
	require(!duplicate_entity_loaded
		&& duplicate_entity_log.str().find("duplicate entity id: duplicate") != std::string::npos
		&& duplicate_entity_log.str().find("first:") != std::string::npos
		&& duplicate_entity_log.str().find("#/entities/0") != std::string::npos
		&& duplicate_entity_log.str().find("second:") != std::string::npos
		&& duplicate_entity_log.str().find("#/entities/1") != std::string::npos,
		"duplicate entity ids must be rejected with both manifest indices in the diagnostic");

	const std::filesystem::path duplicate_animation_property_path = test_root / "duplicate_animation_property.json";
	std::ofstream(duplicate_animation_property_path)
		<< R"({"animations":[{"key":"core.strip","key":"core.other","path":"strip.png","frame_count":2,"fps":10,"loop":true,"horizontal_strip":true}]})";
	require(!elysia::io::AnimationManifestLoader{}.load(duplicate_animation_property_path, animation_manifest),
		"core animation manifests must reject duplicate JSON object properties before parsing");

	const std::filesystem::path duplicate_effect_property_path = test_root / "duplicate_effect_property.json";
	std::ofstream(duplicate_effect_property_path)
		<< R"({"effects":[{"key":"effect.test","animation_key":"core.strip","animation_key":"core.other"}]})";
	elysia::io::AnimationEffectManifest effect_manifest;
	require(!elysia::io::AnimationEffectManifestLoader{}.load(duplicate_effect_property_path, effect_manifest),
		"core effect manifests must reject duplicate JSON object properties before parsing");

	const std::filesystem::path duplicate_font_property_path = test_root / "duplicate_font_property.json";
	std::ofstream(duplicate_font_property_path)
		<< R"({"fonts":[{"key":"ui.test","file":"first.ttf","file":"second.ttf"}]})";
	elysia::io::FontManifest font_manifest;
	require(!elysia::io::FontsManifestLoader{}.load(duplicate_font_property_path, font_manifest),
		"font manifests must reject duplicate JSON object properties before parsing");

	const std::filesystem::path valid_font_manifest_path = test_root / "valid_fonts.json";
	std::ofstream(valid_font_manifest_path)
		<< R"({"fonts":[{"key":"ui.test","file":"test.ttf"}]})";
	require(elysia::io::FontsManifestLoader{}.load(valid_font_manifest_path, font_manifest),
		"font manifests must accept font families without a sizes field");

	const std::filesystem::path legacy_font_sizes_path = test_root / "legacy_font_sizes.json";
	std::ofstream(legacy_font_sizes_path)
		<< R"({"sizes":[10,20,30,40,50,60,70],"fonts":[{"key":"ui.test","file":"test.ttf"}]})";
	require(!elysia::io::FontsManifestLoader{}.load(legacy_font_sizes_path, font_manifest),
		"font manifests must reject the removed sizes field");

	const std::filesystem::path duplicate_i18n_property_path = test_root / "duplicate_i18n_property.json";
	std::ofstream(duplicate_i18n_property_path)
		<< R"({"default_language":"en","default_language":"zh-Hans","languages":["en"],"file":["base.json"]})";
	elysia::io::I18nManifest i18n_manifest;
	require(!elysia::io::I18nManifestLoader{}.load(duplicate_i18n_property_path, i18n_manifest),
		"i18n manifests must reject duplicate JSON object properties before parsing");

	std::filesystem::remove_all(test_root);
}

void test_horizontal_strip_build_and_render_commands()
{
	using namespace elysia;

	require(SDL_Init(SDL_INIT_VIDEO) == 0,
		"horizontal strip tests must initialize SDL video");
	SDL_Surface* target_surface = SDL_CreateRGBSurfaceWithFormat(
		0,640,480,32,SDL_PIXELFORMAT_RGBA32);
	require(target_surface != nullptr,
		"horizontal strip tests must create a software target surface");
	SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target_surface);
	require(renderer != nullptr,
		"horizontal strip tests must create a software renderer");

	io::PathManager* path_manager = io::PathManager::instance();
	require(path_manager->initialize(),
		"horizontal strip tests must initialize the project path manager");
	resources::ResourceManager* resource_manager = resources::ResourceManager::instance();
	resource_manager->clear();

	resources::AtlasBuildRequest request;
	request.atlas_key = "horizontal_strip.test";
	request.source_path = path_manager->textures() / "test" / "frame_group.png";
	request.frame_count = 14;
	request.source_type = resources::AtlasSourceType::HorizontalStrip;

	resources::AtlasBuildPreparer preparer;
	std::vector<resources::AtlasFramePrepareTask> tasks;
	require(preparer.expand_build_request(request, tasks) && tasks.size() == 1,
		"horizontal strip must expand into exactly one preparation task");
	resources::AtlasFramePreparedResult prepared = preparer.prepare_frame(tasks.front());
	require(prepared.surface_result._success && prepared.surface_result._surface
			&& prepared.coverage_mask_surface,
		"horizontal strip source image and coverage mask must prepare successfully");
	require(resource_manager->begin_atlas_build(request),
		"horizontal strip atlas build must initialize");
	require(resource_manager->commit_prepared_atlas_frame(renderer, prepared),
		"horizontal strip atlas must commit from one prepared image");

	const resources::Atlas* atlas = ELYSIA_RESOURCES->find_atlas(request.atlas_key);
	require(atlas && atlas->size() == 14,
		"horizontal strip atlas must expose fourteen logical frames");
	const resources::FrameInfo* first = atlas->frame_at(0);
	const resources::FrameInfo* second = atlas->frame_at(1);
	const resources::FrameInfo* last = atlas->frame_at(13);
	require(first && second && last
		&& first->_texture == second->_texture && second->_texture == last->_texture
		&& first->_coverage_mask != nullptr
		&& first->_coverage_mask == second->_coverage_mask
		&& second->_coverage_mask == last->_coverage_mask,
		"horizontal strip frames must share one base and coverage mask texture pair");
	require(resource_manager->texture_resource_count() == 1,
		"horizontal strip atlas must store exactly one owned texture");
	require(first->_width == 146 && first->_height == 146
		&& first->_source_rect.has_value()
		&& first->_source_rect->nearly_equals(core::Rect{ 0,0,146,146 })
		&& second->_source_rect.has_value()
		&& second->_source_rect->nearly_equals(core::Rect{ 146,0,146,146 })
		&& last->_source_rect.has_value()
		&& last->_source_rect->nearly_equals(core::Rect{ 1898,0,146,146 }),
		"horizontal strip frames must expose contiguous 146 by 146 source rectangles");

	resources::AnimationBuildRequest animation_request;
	animation_request.animation_key = "horizontal_strip.test.animation";
	animation_request.atlas_key = request.atlas_key;
	animation_request.fps = 10.0;
	animation_request.loop = true;
	require(animation::AnimationManager::instance()->register_animation(animation_request, atlas),
		"horizontal strip animation must register");
	std::unique_ptr<animation::Animation> animation =
		ELYSIA_ANIMATIONS->create_animation(animation_request.animation_key);
	require(animation != nullptr,
		"horizontal strip animation must create a playback instance");
	core::RenderCommand render_command;
	require(animation->build_render_command(
		core::Rect{ 0,0,292,292 },0.0,core::SpriteFlip::None,render_command)
		&& render_command.use_src_rect
		&& render_command.src_rect.nearly_equals(core::Rect{ 0,0,146,146 }),
		"world animation render commands must preserve the frame source rectangle");
	animation->update(0.11);
	require(animation->build_render_command(
		core::Rect{ 0,0,292,292 },0.0,core::SpriteFlip::None,render_command)
		&& render_command.src_rect.nearly_equals(core::Rect{ 146,0,146,146 }),
		"horizontal strip playback must advance to the next logical source rectangle");

	std::vector<core::RenderCommand> render_commands;
	require(animation->append_render_commands(
			core::Rect{ 10,20,292,292 },
			27.0,
			core::SpriteFlip::Horizontal,
			std::nullopt,
			render_commands)
			&& render_commands.size() == 1,
		"animation without a color overlay must append only its base command");
	render_commands.clear();
	const core::Color overlay_color{ 33,150,243,128 };
	require(animation->append_render_commands(
			core::Rect{ 10,20,292,292 },
			27.0,
			core::SpriteFlip::Horizontal,
			overlay_color,
			render_commands)
			&& render_commands.size() == 2,
		"animation with a color overlay must append base then coverage mask");
	const core::RenderCommand& base_command = render_commands[0];
	const core::RenderCommand& overlay_command = render_commands[1];
	require(base_command.texture == second->_texture
			&& overlay_command.texture == second->_coverage_mask
			&& overlay_command.command_rect.nearly_equals(base_command.command_rect)
			&& overlay_command.src_rect.nearly_equals(base_command.src_rect)
			&& overlay_command.rotation_degrees == base_command.rotation_degrees
			&& overlay_command.rotation_origin == base_command.rotation_origin
			&& overlay_command.flip == base_command.flip
			&& overlay_command.alpha == overlay_color.a
			&& overlay_command.texture_color_modulation
				== core::TextureColorModulation{
					.r = overlay_color.r,
					.g = overlay_color.g,
					.b = overlay_color.b },
		"coverage mask command must preserve base geometry and carry overlay color");

	const camera::Camera camera(
		core::Vector2{ 320.0f,240.0f },
		core::Vector2{ 640.0f,480.0f });
	const core::ScreenRenderCommand screen_overlay =
		core::project_render_command_to_screen(overlay_command,camera);
	require(screen_overlay.texture_color_modulation
			== overlay_command.texture_color_modulation
			&& screen_overlay.alpha == overlay_command.alpha,
		"world-to-screen projection must preserve texture modulation");

	require(SDL_SetTextureColorMod(
			screen_overlay.texture,12,34,56) == 0
			&& SDL_SetTextureAlphaMod(screen_overlay.texture,77) == 0,
		"executor test must configure an initial shared texture modulation");
	core::execute_render_command(renderer,screen_overlay);
	Uint8 restored_red = 0;
	Uint8 restored_green = 0;
	Uint8 restored_blue = 0;
	Uint8 restored_alpha = 0;
	SDL_GetTextureColorMod(
		screen_overlay.texture,
		&restored_red,
		&restored_green,
		&restored_blue);
	SDL_GetTextureAlphaMod(screen_overlay.texture,&restored_alpha);
	require(restored_red == 12 && restored_green == 34
			&& restored_blue == 56 && restored_alpha == 77,
		"SDL executor must restore shared texture color and alpha modulation");

	ui::UiAnimation ui_animation(
		animation_request.animation_key,
		core::Rect{ 0,0,292,292 }
	);
	std::vector<core::UiRenderCommand> ui_commands;
	ui_animation.submit_ui_render_commands(ui_commands);
	require(ui_commands.size() == 1
		&& ui_commands.front().use_src_rect
		&& ui_commands.front().src_rect.nearly_equals(core::Rect{ 0,0,146,146 }),
		"UI animation render commands must preserve the frame source rectangle");
	ui_animation.update(0.11);
	ui_commands.clear();
	ui_animation.submit_ui_render_commands(ui_commands);
	require(ui_commands.size() == 1
		&& ui_commands.front().src_rect.nearly_equals(core::Rect{ 146,0,146,146 }),
		"UI horizontal strip playback must advance its source rectangle");
	ui_animation.set_opacity(128);
	ui_animation.set_color_overlay(overlay_color);
	ui_commands.clear();
	ui_animation.submit_ui_render_commands(ui_commands);
	require(ui_commands.size() == 2
			&& ui_commands[0].texture == second->_texture
			&& ui_commands[1].texture == second->_coverage_mask
			&& ui_commands[1].screen_rect.nearly_equals(ui_commands[0].screen_rect)
			&& ui_commands[1].src_rect.nearly_equals(ui_commands[0].src_rect)
			&& ui_commands[1].alpha == 64
			&& ui_commands[1].texture_color_modulation
				== core::TextureColorModulation{
					.r = overlay_color.r,
					.g = overlay_color.g,
					.b = overlay_color.b },
		"UiAnimation must append a matching mask command and apply control opacity");

	resource_manager->clear();
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(target_surface);
	SDL_Quit();
}

void test_horizontal_strip_rejects_non_divisible_width()
{
	using namespace elysia;

	require(SDL_Init(SDL_INIT_VIDEO) == 0,
		"invalid horizontal strip test must initialize SDL video");
	SDL_Surface* target_surface = SDL_CreateRGBSurfaceWithFormat(
		0,64,64,32,SDL_PIXELFORMAT_RGBA32);
	SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target_surface);
	require(target_surface && renderer,
		"invalid horizontal strip test must create a software renderer");

	resources::ResourceManager* resource_manager = resources::ResourceManager::instance();
	resource_manager->clear();
	resources::AtlasBuildRequest request;
	request.atlas_key = "horizontal_strip.invalid";
	request.source_path = "invalid.png";
	request.frame_count = 3;
	request.source_type = resources::AtlasSourceType::HorizontalStrip;
	require(resource_manager->begin_atlas_build(request),
		"invalid strip build must initialize before dimension validation");

	resources::AtlasFramePreparedResult prepared;
	prepared.task.atlas_key = request.atlas_key;
	prepared.task.frame_path = request.source_path;
	prepared.task.expected_frame_count = request.frame_count;
	prepared.task.source_type = request.source_type;
	prepared.surface_result._success = true;
	prepared.surface_result._asset_key = request.atlas_key;
	prepared.surface_result._frame_path = request.source_path;
	prepared.surface_result._surface.reset(SDL_CreateRGBSurfaceWithFormat(
		0,10,4,32,SDL_PIXELFORMAT_RGBA32));
	require(prepared.surface_result._surface != nullptr,
		"invalid strip test must create its source surface");
	require(!resource_manager->commit_prepared_atlas_frame(renderer, prepared),
		"horizontal strip width not divisible by frame count must fail");

	resource_manager->clear();
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(target_surface);
	SDL_Quit();
}

elysia::resources::AtlasFramePreparedResult make_directory_frame(
	std::string_view atlas_key,
	std::size_t frame_index,
	std::size_t frame_count,
	bool include_mask)
{
	using namespace elysia;

	resources::AtlasFramePreparedResult prepared;
	prepared.task.atlas_key = atlas_key;
	prepared.task.frame_path =
		std::filesystem::path("frame_")
		/ (std::to_string(frame_index) + ".png");
	prepared.task.frame_index = frame_index;
	prepared.task.expected_frame_count = frame_count;
	prepared.task.source_type = resources::AtlasSourceType::FrameDirectory;
	prepared.surface_result._success = true;
	prepared.surface_result._asset_key = atlas_key;
	prepared.surface_result._frame_path = prepared.task.frame_path;
	prepared.surface_result._frame_index = frame_index;
	prepared.surface_result._surface.reset(SDL_CreateRGBSurfaceWithFormat(
		0,4,4,32,SDL_PIXELFORMAT_RGBA32));
	if (include_mask && prepared.surface_result._surface)
	{
		prepared.coverage_mask_surface =
			resources::create_coverage_mask_surface(
				*prepared.surface_result._surface);
	}
	return prepared;
}

void test_directory_frames_publish_transactionally()
{
	using namespace elysia;

	require(SDL_Init(SDL_INIT_VIDEO) == 0,
		"directory atlas tests must initialize SDL video");
	SDL_Surface* target_surface = SDL_CreateRGBSurfaceWithFormat(
		0,64,64,32,SDL_PIXELFORMAT_RGBA32);
	SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(target_surface);
	require(target_surface && renderer,
		"directory atlas tests must create a software renderer");

	resources::ResourceManager* resource_manager =
		resources::ResourceManager::instance();
	resource_manager->clear();
	resources::AtlasBuildRequest request;
	request.atlas_key = "directory.test";
	request.source_path = "directory";
	request.frame_count = 2;
	request.frame_filename_prefix = "frame";
	request.source_type = resources::AtlasSourceType::FrameDirectory;
	require(resource_manager->begin_atlas_build(request),
		"directory atlas build must initialize");

	auto first_prepared = make_directory_frame(
		request.atlas_key,0,request.frame_count,true);
	require(resource_manager->commit_prepared_atlas_frame(
			renderer,first_prepared)
			&& resource_manager->texture_resource_count() == 0
			&& resource_manager->atlas_resource_count() == 0,
		"incomplete directory atlas resources must remain private to assembly");
	auto second_prepared = make_directory_frame(
		request.atlas_key,1,request.frame_count,true);
	require(resource_manager->commit_prepared_atlas_frame(
			renderer,second_prepared),
		"complete directory atlas must commit");
	const resources::Atlas* atlas =
		ELYSIA_RESOURCES->find_atlas(request.atlas_key);
	const resources::FrameInfo* first = atlas ? atlas->frame_at(0) : nullptr;
	const resources::FrameInfo* second = atlas ? atlas->frame_at(1) : nullptr;
	require(first && second
			&& first->_texture != second->_texture
			&& first->_coverage_mask != nullptr
			&& second->_coverage_mask != nullptr
			&& first->_coverage_mask != second->_coverage_mask
			&& resource_manager->texture_resource_count() == 2,
		"directory frames must publish one distinct base/mask pair per logical frame");

	resource_manager->clear();
	require(resource_manager->texture_resource_count() == 0
			&& resource_manager->atlas_resource_count() == 0,
		"resource cleanup must release both base and mask ownership together");

	request.atlas_key = "directory.failed";
	require(resource_manager->begin_atlas_build(request),
		"failed directory atlas test must initialize");
	first_prepared = make_directory_frame(
		request.atlas_key,0,request.frame_count,true);
	require(resource_manager->commit_prepared_atlas_frame(
			renderer,first_prepared),
		"failed directory atlas test must stage its first frame");
	second_prepared = make_directory_frame(
		request.atlas_key,1,request.frame_count,false);
	require(!resource_manager->commit_prepared_atlas_frame(
			renderer,second_prepared)
			&& resource_manager->texture_resource_count() == 0
			&& resource_manager->atlas_resource_count() == 0,
		"a missing mask must fail without publishing partial textures or Atlas");

	resource_manager->clear();
	SDL_DestroyRenderer(renderer);
	SDL_FreeSurface(target_surface);
	SDL_Quit();
}
}

int main()
{
	test_coverage_mask_surface_conversion();
	test_horizontal_strip_manifest_schema();
	test_horizontal_strip_build_and_render_commands();
	test_horizontal_strip_rejects_non_divisible_width();
	test_directory_frames_publish_transactionally();
	std::cout << "horizontal strip animation tests passed\n";
	return EXIT_SUCCESS;
}
