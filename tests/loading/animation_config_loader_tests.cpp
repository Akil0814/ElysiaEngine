#define SDL_MAIN_HANDLED

#include "engine/io/loaders/animation_config_loader.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
using elysia::tests::require;

std::filesystem::path write_json(
	const std::filesystem::path& root,
	const std::string& name,
	const std::string& contents)
{
	const auto path = root / name;
	std::ofstream(path) << contents;
	return path;
}

elysia::io::AnimationLayout make_layout()
{
	elysia::io::AnimationLayout layout;
	elysia::io::AnimationLayoutEntry idle;
	idle.has_path = true;
	idle.path = "idle";
	layout.animations.emplace("idle", std::move(idle));
	elysia::io::AnimationLayoutEntry attack;
	attack.has_segment_path = true;
	attack.segment_path = "attack/{segment}";
	layout.animations.emplace("attack", std::move(attack));
	return layout;
}

std::string make_segment_config(size_t segment_count)
{
	std::ostringstream json;
	json << R"({"defaults":{"source_type":"frame_directory"},"animations":{"attack":{"segments":[)";
	for (size_t index = 0; index < segment_count; ++index)
	{
		if (index != 0) json << ',';
		json << R"({"frame_count":1,"fps":10,"loop":false})";
	}
	json << "]}}}";
	return json.str();
}

void test_new_animation_config_schema()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_animation_config_loader_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto layout = make_layout();
	elysia::io::AnimationConfigLoader loader;

	const auto valid = write_json(root, "valid.json",
		R"({"defaults":{"source_type":"frame_directory"},"animations":{"idle":{"frame_count":8,"fps":12,"loop":true},"attack":{"segments":[{"frame_count":2,"fps":10,"loop":false},{"frame_count":3,"fps":11,"loop":false},{"frame_count":4,"fps":12,"loop":true}]}}})");
	auto config_result = loader.load(valid,layout);
	require(config_result,
		"animation config must accept required defaults.source_type and explicit clip playback fields");
	const auto& config = *config_result;
	require(config.source_type == elysia::io::AnimationSourceType::FrameDirectory
		&& config.clips.size() == 4
		&& config.clips[0].animation_name == "attack"
		&& config.clips[0].segment_index == 0
		&& config.clips[0].path.generic_string() == "attack/00"
		&& config.clips[1].path.generic_string() == "attack/01"
		&& config.clips[2].path.generic_string() == "attack/02"
		&& config.clips[3].animation_name == "idle"
		&& config.clips[3].frame_count == 8,
		"segment filesystem paths must start at 00 while preserving explicit clip values");

	const auto strip = write_json(root, "strip.json",
		R"({"defaults":{"source_type":"horizontal_strip"},"animations":{"idle":{"frame_count":14,"fps":10,"loop":true}}})");
	auto strip_result = loader.load(strip,layout);
	require(strip_result
		&& strip_result->source_type == elysia::io::AnimationSourceType::HorizontalStrip,
		"animation source_type must be selected once at config scope");

	require(!loader.load(write_json(root, "old_schema.json",
		R"({"animations":{"idle":{"frame_count":8,"fps":10,"loop":true}}})"),layout),
		"animation config must reject the old schema without defaults.source_type");
	require(!loader.load(write_json(root, "clip_override.json",
		R"({"defaults":{"source_type":"frame_directory"},"animations":{"idle":{"source_type":"horizontal_strip","frame_count":8,"fps":10,"loop":true}}})"),layout),
		"individual animation entries must reject source_type overrides");
	require(!loader.load(write_json(root, "unknown_source.json",
		R"({"defaults":{"source_type":"vertical_strip"},"animations":{"idle":{"frame_count":8,"fps":10,"loop":true}}})"),layout),
		"animation config must reject unsupported source types");

	std::filesystem::remove_all(root);
}

void test_segment_limit_and_two_digit_paths()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_animation_config_segment_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto layout = make_layout();
	elysia::io::AnimationConfigLoader loader;
	auto config = loader.load(write_json(root,"segments_100.json",make_segment_config(100)),layout);
	require(config
		&& config->clips.size() == 100
		&& config->clips.front().segment_index == 0
		&& config->clips.front().path.generic_string() == "attack/00"
		&& config->clips.back().segment_index == 99
		&& config->clips.back().path.generic_string() == "attack/99",
		"segments 0 through 99 must be valid and use two-digit filesystem formatting");
	require(!loader.load(write_json(root,"segments_101.json",make_segment_config(101)),layout),
		"a segmented animation must reject more than 100 segments");

	std::filesystem::remove_all(root);
}

void test_missing_source_is_typed()
{
	const auto missing = std::filesystem::temp_directory_path()
		/ "elysia_missing_animation_config.json";
	std::filesystem::remove(missing);
	auto result = elysia::io::AnimationConfigLoader{}.load(missing,make_layout());
	require(!result
		&& result.error().code == elysia::io::ManifestLoadError::FileMissing
		&& result.error().diagnostic.entries.size() == 1
		&& result.error().diagnostic.entries.front().subject_type == "animation-config"
		&& result.error().diagnostic.entries.front().expected_path == missing,
		"missing manifest sources must return typed file diagnostics before JSON parsing");
}
}

int main()
{
	test_new_animation_config_schema();
	test_segment_limit_and_two_digit_paths();
	test_missing_source_is_typed();
	std::cout << "animation config loader tests passed\n";
	return EXIT_SUCCESS;
}
