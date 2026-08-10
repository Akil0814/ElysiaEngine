#include "engine/loading/resource_load_plan.h"
#include "engine/loading/resource_load_plan_validator.h"
#include "engine/resources/resource_origin.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace
{
using elysia::loading::ResourceLoadPlan;
using elysia::loading::ResourceLoadPlanValidationError;
using elysia::loading::ResourceLoadPlanValidator;
using elysia::resources::ResourceOrigin;
using elysia::tests::require;

enum class Registry
{
	Atlas,
	Animation,
	Effect,
	Texture,
	Font,
	Sound,
	Music
};

struct RegistryCase
{
	Registry registry;
	const char* name;
};

constexpr RegistryCase registry_cases[]{
	{Registry::Atlas, "Atlas"},
	{Registry::Animation, "Animation"},
	{Registry::Effect, "Effect"},
	{Registry::Texture, "Texture"},
	{Registry::Font, "Font"},
	{Registry::Sound, "Sound"},
	{Registry::Music, "Music"}
};

ResourceOrigin first_origin()
{
	return {
		std::filesystem::path{"assets/configs/core/first.json"},
		"/resources/0", "core", "animations", "core_entity", "first_idle", 0
	};
}

ResourceOrigin second_origin()
{
	ResourceOrigin origin{
		std::filesystem::path{"assets/configs/modules/second.json"},
		"/animations/idle/segments/99", "character_effects", "effects",
		"ExampleEntity", "slash_trail", 99
	};
	origin.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
	return origin;
}

void append_request(
	ResourceLoadPlan& plan,
	Registry registry,
	const std::string& key,
	const ResourceOrigin& origin)
{
	switch (registry)
	{
	case Registry::Atlas:
	{
		elysia::resources::AtlasBuildRequest request;
		request.atlas_key = key;
		request.origin = origin;
		plan.atlas_build_requests().push_back(std::move(request));
		break;
	}
	case Registry::Animation:
	{
		elysia::resources::AnimationBuildRequest request;
		request.animation_key = key;
		request.atlas_key = "backing_atlas";
		request.origin = origin;
		plan.animation_build_requests().push_back(std::move(request));
		break;
	}
	case Registry::Effect:
	{
		elysia::resources::AnimationEffectBuildRequest request;
		request.effect_key = key;
		request.animation_key = "backing_animation";
		request.origin = origin;
		plan.animation_effect_build_requests().push_back(std::move(request));
		break;
	}
	case Registry::Texture:
	{
		elysia::resources::TextureLoadRequest request;
		request.key = key;
		request.origin = origin;
		plan.texture_requests().push_back(std::move(request));
		break;
	}
	case Registry::Font:
	{
		elysia::resources::FontLoadRequest request;
		request.key = key;
		request.origin = origin;
		plan.font_requests().push_back(std::move(request));
		break;
	}
	case Registry::Sound:
	{
		elysia::resources::SoundLoadRequest request;
		request.key = key;
		request.origin = origin;
		plan.sound_requests().push_back(std::move(request));
		break;
	}
	case Registry::Music:
	{
		elysia::resources::MusicLoadRequest request;
		request.key = key;
		request.origin = origin;
		plan.music_requests().push_back(std::move(request));
		break;
	}
	}
}

void require_contains(const std::string& value, std::string_view needle, const char* message)
{
	if (value.find(needle) != std::string::npos) return;
	std::cerr << "FAILED: " << message << "\n  missing: " << needle
		<< "\n  value:   " << value << '\n';
	std::exit(EXIT_FAILURE);
}

void require_origin_equal(
	const ResourceOrigin& actual,
	const ResourceOrigin& expected,
	const char* message)
{
	require(actual.config_path == expected.config_path, message);
	require(actual.json_pointer == expected.json_pointer, message);
	require(actual.module == expected.module, message);
	require(actual.capability == expected.capability, message);
	require(actual.entity_id == expected.entity_id, message);
	require(actual.logical_name == expected.logical_name, message);
	require(actual.segment_index == expected.segment_index, message);
	require(actual.scope == expected.scope, message);
}

void test_duplicate_key_diagnostics_for_every_registry()
{
	const ResourceOrigin core_first = first_origin();
	ResourceOrigin core_second = second_origin();
	core_second.config_path = "assets/configs/core/second.json";
	core_second.module = "core";
	core_second.scope = elysia::resources::ResourceOriginScope::Core;
	ResourceOrigin module_first = first_origin();
	module_first.config_path = "assets/configs/modules/first.json";
	module_first.module = "characters";
	module_first.scope = elysia::resources::ResourceOriginScope::AdditionalModule;
	const ResourceOrigin module_second = second_origin();
	struct OriginPair { const ResourceOrigin* first; const ResourceOrigin* second; };
	const OriginPair origin_pairs[]{
		{&core_first, &core_second},
		{&module_first, &module_second},
		{&core_first, &module_second}
	};
	for (const RegistryCase& test_case : registry_cases)
	{
		for (size_t pair_index = 0; pair_index < std::size(origin_pairs); ++pair_index)
		{
			const auto& pair = origin_pairs[pair_index];
			ResourceLoadPlan plan;
			append_request(plan, test_case.registry, "shared.duplicate_key", *pair.first);
			append_request(plan, test_case.registry, "shared.duplicate_key", *pair.second);

			auto validation = ResourceLoadPlanValidator{}.validate(plan);
			require(!validation,
				"core-core, module-module, and core-module duplicates must fail in every registry");
			const auto& error = validation.error();
			require(error.duplicate, "duplicate failure must be marked as a duplicate");
			require(error.registry == test_case.name, "duplicate failure must identify its registry");
			require(error.key == "shared.duplicate_key", "duplicate failure must preserve the exact key");
			require_origin_equal(error.first, *pair.first, "duplicate failure must preserve the complete first origin");
			require_origin_equal(error.second, *pair.second, "duplicate failure must preserve the complete second origin");

			if (pair_index != 2) continue;
			const std::string description = error.describe();
			require_contains(description, std::string{"Duplicate "} + test_case.name
				+ " key: shared.duplicate_key", "description must identify registry and duplicate key");
			require_contains(description, "first:", "description must label the first origin");
			require_contains(description, "assets/configs/core/first.json#/resources/0",
				"description must include the first project-relative path and JSON pointer");
			require_contains(description, "scope=core module=core", "description must identify a core origin");
			require_contains(description, "capability=animations", "description must include first capability");
			require_contains(description, "entity=core_entity", "description must include first entity");
			require_contains(description, "logical=first_idle", "description must include first logical name");
			require_contains(description, "segment=0", "description must include a zero segment index");
			require_contains(description, "second:", "description must label the second origin");
			require_contains(description, "assets/configs/modules/second.json#/animations/idle/segments/99",
				"description must include the second project-relative path and JSON pointer");
			require_contains(description, "scope=additional module=character_effects", "description must include module scope and name");
			require_contains(description, "capability=effects", "description must include second capability");
			require_contains(description, "entity=ExampleEntity", "description must include second entity");
			require_contains(description, "logical=slash_trail", "description must include second logical name");
			require_contains(description, "segment=99", "description must include second segment index");
		}
	}
}

void test_same_key_is_legal_across_different_registries()
{
	ResourceLoadPlan plan;
	const ResourceOrigin origin = first_origin();
	for (const RegistryCase& test_case : registry_cases)
		append_request(plan, test_case.registry, "same.key", origin);

	plan.animation_build_requests().front().atlas_key = "same.key";
	plan.animation_effect_build_requests().front().animation_key = "same.key";

	auto validation = ResourceLoadPlanValidator{}.validate(plan);
	require(validation,
		"the same string must be legal when used once in each distinct registry");
}

void test_reference_integrity()
{
	const ResourceOrigin origin = second_origin();

	{
		ResourceLoadPlan plan;
		elysia::resources::AnimationBuildRequest animation;
		animation.animation_key = "hero.idle";
		animation.atlas_key = "hero.missing_atlas";
		animation.origin = origin;
		plan.animation_build_requests().push_back(std::move(animation));

		auto validation = ResourceLoadPlanValidator{}.validate(plan);
		require(!validation,
			"an Animation request must reference an existing Atlas request");
		const auto& error = validation.error();
		require(!error.duplicate, "a missing reference must not be reported as a duplicate");
		require_contains(error.describe(), "Animation references missing Atlas key: hero.missing_atlas",
			"missing Atlas diagnostic must name the absent key");
		require_contains(error.describe(), "assets/configs/modules/second.json#/animations/idle/segments/99",
			"missing Atlas diagnostic must preserve the complete request origin");
	}

	{
		ResourceLoadPlan plan;
		elysia::resources::AnimationEffectBuildRequest effect;
		effect.effect_key = "hero.slash";
		effect.animation_key = "hero.missing_animation";
		effect.origin = origin;
		plan.animation_effect_build_requests().push_back(std::move(effect));

		auto validation = ResourceLoadPlanValidator{}.validate(plan);
		require(!validation,
			"an Effect request must reference an existing Animation request");
		const auto& error = validation.error();
		require(!error.duplicate, "a missing Animation reference must not be a duplicate");
		require_contains(error.describe(), "Effect references missing Animation key: hero.missing_animation",
			"missing Animation diagnostic must name the absent key");
		require_contains(error.describe(), "scope=additional module=character_effects capability=effects",
			"missing Animation diagnostic must preserve module and capability context");
	}

	{
		ResourceLoadPlan plan;
		elysia::resources::AtlasBuildRequest atlas;
		atlas.atlas_key = "hero.idle";
		atlas.origin = origin;
		plan.atlas_build_requests().push_back(std::move(atlas));

		elysia::resources::AnimationBuildRequest animation;
		animation.animation_key = "hero.idle";
		animation.atlas_key = "hero.idle";
		animation.origin = origin;
		plan.animation_build_requests().push_back(std::move(animation));

		elysia::resources::AnimationEffectBuildRequest effect;
		effect.effect_key = "hero.slash";
		effect.animation_key = "hero.idle";
		effect.origin = origin;
		plan.animation_effect_build_requests().push_back(std::move(effect));

		require(ResourceLoadPlanValidator{}.validate(plan),
			"complete Atlas -> Animation -> Effect references must pass validation");
	}
}
}

int main()
{
	test_duplicate_key_diagnostics_for_every_registry();
	test_same_key_is_legal_across_different_registries();
	test_reference_integrity();
	std::cout << "resource load plan validator tests passed\n";
	return EXIT_SUCCESS;
}
