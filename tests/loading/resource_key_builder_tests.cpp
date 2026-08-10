#include "engine/resources/pipeline/filesystem_segment_formatter.h"
#include "engine/resources/pipeline/resource_key_builder.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using elysia::resources::ResourceKeyBuilder;
using elysia::tests::require;

void require_equal(const std::string& actual, std::string_view expected, const char* message)
{
	if (actual == expected) return;
	std::cerr << "FAILED: " << message << "\n  expected: " << expected
		<< "\n  actual:   " << actual << '\n';
	std::exit(EXIT_FAILURE);
}

void test_component_validation()
{
	const std::vector<std::string> valid_components{
		"A", "z", "0", "_", "Alpha_09", "stage_000", "CG_12"
	};
	for (const std::string& component : valid_components)
	{
		std::string error;
		require(ResourceKeyBuilder::validate_component(component, error),
			"ASCII letters, digits, and underscores must be valid key components");
		require(error.empty(), "valid component validation must not produce an error");
	}

	const std::vector<std::string> invalid_components{
		"", "two.parts", "bad-name", "bad name", ".", "\xE8\xA7\x92\xE8\x89\xB2"
	};
	for (const std::string& component : invalid_components)
	{
		std::string error;
		require(!ResourceKeyBuilder::validate_component(component, error),
			"invalid key component must be rejected");
		require(!error.empty(), "invalid component validation must explain the failure");
	}
}

void test_full_key_validation()
{
	const std::vector<std::string> valid_keys{
		"idle", "test.animation", "ExampleEntity.effect.attack_normal.0",
		"font.default.10", "stage_000.background"
	};
	for (const std::string& key : valid_keys)
	{
		std::string error;
		require(ResourceKeyBuilder::validate_key(key, error),
			"a dot-separated sequence of valid components must be a valid key");
		require(error.empty(), "valid full-key validation must not produce an error");
	}

	const std::vector<std::string> invalid_keys{
		"", ".idle", "idle.", "idle..move", "attack-normal", "attack normal",
		"enemy.\xE6\x81\xB6\xE9\xAD\x94"
	};
	for (const std::string& key : invalid_keys)
	{
		std::string error;
		require(!ResourceKeyBuilder::validate_key(key, error),
			"empty, malformed, or non-ASCII full keys must be rejected");
		require(!error.empty(), "invalid full-key validation must explain the failure");
	}
}

void test_namespace_and_component_building()
{
	std::string key;
	std::string error;
	require(ResourceKeyBuilder::build("ExampleEntity", "", {"idle"}, std::nullopt, key, error),
		"an empty namespace must be accepted");
	require_equal(key, "ExampleEntity.idle", "empty namespace must not create an empty component");

	key.clear();
	error.clear();
	require(ResourceKeyBuilder::build(
		"ExampleEntity", "effect", {"attack", "normal"}, std::nullopt, key, error),
		"a non-empty valid namespace must be accepted");
	require_equal(key, "ExampleEntity.effect.attack.normal",
		"namespace and logical components must be joined in the documented order");

	std::string appended;
	error.clear();
	require(ResourceKeyBuilder::append_component("ExampleEntity.effect", "slash_01", appended, error),
		"a valid component must append to a valid base key");
	require_equal(appended, "ExampleEntity.effect.slash_01",
		"append_component must insert exactly one separator");

	for (const auto& invalid_build : std::vector<std::pair<std::string, std::string>>{
		{"bad-id", ""}, {"hero", "bad namespace"}, {"hero", "\xE6\x95\x88\xE6\x9E\x9C"}})
	{
		key.clear();
		error.clear();
		require(!ResourceKeyBuilder::build(
			invalid_build.first, invalid_build.second, {"idle"}, std::nullopt, key, error),
			"invalid entity and namespace components must be rejected by build");
		require(!error.empty(), "invalid build input must produce an error");
	}

	key.clear();
	error.clear();
	require(!ResourceKeyBuilder::build("hero", "", {"bad-name"}, std::nullopt, key, error),
		"invalid logical components must be rejected by build");

	key.clear();
	error.clear();
	require(!ResourceKeyBuilder::build("hero", "", {}, std::nullopt, key, error),
		"build must require at least one logical component");
}

void test_segment_key_and_filesystem_formatting()
{
	struct SegmentCase
	{
		size_t index;
		const char* resource_key;
		const char* filesystem_component;
	};
	const SegmentCase cases[]{
		{0, "entity.attack.0", "00"},
		{1, "entity.attack.1", "01"},
		{2, "entity.attack.2", "02"},
		{99, "entity.attack.99", "99"}
	};

	for (const SegmentCase& test_case : cases)
	{
		std::string key;
		std::string error;
		require(ResourceKeyBuilder::build(
			"entity", "", {"attack"}, test_case.index, key, error),
			"segments in the inclusive 0-99 range must build successfully");
		require_equal(key, test_case.resource_key,
			"runtime resource-key segments must use unpadded decimal notation");

		std::string formatted;
		require(elysia::resources::format_filesystem_segment(test_case.index, formatted),
			"segments in the inclusive 0-99 range must have filesystem notation");
		require_equal(formatted, test_case.filesystem_component,
			"filesystem segments must use exactly two decimal digits");
	}

	std::string key;
	std::string error;
	require(!ResourceKeyBuilder::build("entity", "", {"attack"}, 100, key, error),
		"runtime segment index 100 must be rejected");
	require(!error.empty(), "out-of-range runtime segment must produce an error");

	std::string formatted;
	require(!elysia::resources::format_filesystem_segment(100, formatted),
		"filesystem segment index 100 must be rejected");
}
}

int main()
{
	test_component_validation();
	test_full_key_validation();
	test_namespace_and_component_building();
	test_segment_key_and_filesystem_formatting();
	std::cout << "resource key builder tests passed\n";
	return EXIT_SUCCESS;
}
