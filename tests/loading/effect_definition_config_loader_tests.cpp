#define SDL_MAIN_HANDLED

#include "engine/io/loaders/effect_definition_config_loader.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

elysia::io::AnimationConfig make_animation_config()
{
	elysia::io::AnimationConfig config;
	elysia::io::AnimationClipConfig normal;
	normal.animation_name = "idle";
	config.clips.push_back(std::move(normal));
	for (size_t segment = 0; segment < 2; ++segment)
	{
		elysia::io::AnimationClipConfig clip;
		clip.animation_name = "attack_normal";
		clip.is_segment = true;
		clip.segment_index = segment;
		config.clips.push_back(std::move(clip));
	}
	return config;
}

void test_mapping_and_segment_expansion()
{
	const auto root = std::filesystem::temp_directory_path()
		/ "elysia_effect_definition_config_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);

	elysia::io::EffectDefinitionConfigLoader loader;
	const auto path = write_json(root, "valid.json",
		R"({"effects":{"slash_trail":{"animation":"attack_normal"},"aura":{"animation":"idle","default_width":64,"default_height":32,"default_angle_degrees":15}}})");
	auto config_result = loader.load(path,make_animation_config());
	require(config_result && config_result->effects.size() == 3,
		"effect mappings must expand every configured segment and preserve ordinary animations");
	const auto& config = *config_result;

	const auto aura = std::find_if(config.effects.begin(), config.effects.end(),
		[](const auto& effect) { return effect.effect_name == "aura"; });
	require(aura != config.effects.end()
		&& aura->animation_name == "idle"
		&& !aura->is_segment
		&& aura->default_width == 64.0f
		&& aura->default_height == 32.0f
		&& aura->default_angle_degrees == 15.0,
		"ordinary effect mappings must preserve animation and presentation defaults");

	for (size_t segment = 0; segment < 2; ++segment)
	{
		const auto slash = std::find_if(config.effects.begin(), config.effects.end(),
			[segment](const auto& effect)
			{
				return effect.effect_name == "slash_trail"
					&& effect.is_segment && effect.segment_index == segment;
			});
		require(slash != config.effects.end()
			&& slash->animation_name == "attack_normal"
			&& slash->default_width == 0.0f
			&& slash->default_height == 0.0f,
			"segmented effects must retain zero defaults so playback uses the natural frame size");
	}

	std::filesystem::remove_all(root);
}

void test_invalid_effect_definitions_fail()
{
	const auto root = std::filesystem::temp_directory_path()
		/ "elysia_invalid_effect_definition_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);

	const auto animations = make_animation_config();
	elysia::io::EffectDefinitionConfigLoader loader;
	require(!loader.load(write_json(root, "missing_animation.json",
		R"({"effects":{"missing":{"animation":"does_not_exist"}}})"),animations),
		"effect definitions must reject mappings to animations absent from the same module config");
	require(!loader.load(write_json(root, "one_sided_size.json",
		R"({"effects":{"bad_size":{"animation":"idle","default_width":32}}})"),animations),
		"effect definitions must require default width and height together");
	require(!loader.load(write_json(root, "negative_size.json",
		R"({"effects":{"bad_size":{"animation":"idle","default_width":-1,"default_height":-1}}})"),animations),
		"effect definitions must reject negative default dimensions");
	require(!loader.load(write_json(root, "duplicate_effect.json",
		R"({"effects":{"same":{"animation":"idle"},"same":{"animation":"idle"}}})"),animations),
		"effect definitions must reject duplicate logical effect members before JSON values are overwritten");
	require(!loader.load(write_json(root, "invalid_component.json",
		R"({"effects":{"bad-name":{"animation":"idle"}}})"),animations),
		"effect logical names must use the shared key-component syntax");
	require(!loader.load(write_json(root, "unknown_field.json",
		R"({"effects":{"bad":{"animation":"idle","atlas":"legacy"}}})"),animations),
		"effect definitions must reject fields outside the mapping and presentation schema");

	std::filesystem::remove_all(root);
}
}

int main()
{
	test_mapping_and_segment_expansion();
	test_invalid_effect_definitions_fail();
	std::cout << "effect definition config loader tests passed\n";
	return EXIT_SUCCESS;
}
