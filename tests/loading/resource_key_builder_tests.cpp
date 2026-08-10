#include "engine/resources/pipeline/filesystem_segment_formatter.h"
#include "engine/resources/pipeline/resource_key_builder.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
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
		require(ResourceKeyBuilder::validate_component(component),
			"ASCII letters, digits, and underscores must be valid key components");
	}

	const std::vector<std::string> invalid_components{
		"", "two.parts", "bad-name", "bad name", ".", "\xE8\xA7\x92\xE8\x89\xB2"
	};
	for (const std::string& component : invalid_components)
	{
		auto result = ResourceKeyBuilder::validate_component(component);
		require(!result,
			"invalid key component must be rejected");
		require(!result.error().message.empty(), "invalid component validation must explain the failure");
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
		require(ResourceKeyBuilder::validate_key(key),
			"a dot-separated sequence of valid components must be a valid key");
	}

	const std::vector<std::string> invalid_keys{
		"", ".idle", "idle.", "idle..move", "attack-normal", "attack normal",
		"enemy.\xE6\x81\xB6\xE9\xAD\x94"
	};
	for (const std::string& key : invalid_keys)
	{
		auto result = ResourceKeyBuilder::validate_key(key);
		require(!result,
			"empty, malformed, or non-ASCII full keys must be rejected");
		require(!result.error().message.empty(), "invalid full-key validation must explain the failure");
	}
}

void test_namespace_and_component_building()
{
	auto key = ResourceKeyBuilder::build("ExampleEntity","",{"idle"},std::nullopt);
	require(key,
		"an empty namespace must be accepted");
	require_equal(*key,"ExampleEntity.idle","empty namespace must not create an empty component");

	key = ResourceKeyBuilder::build(
		"ExampleEntity","effect",{"attack","normal"},std::nullopt);
	require(key,
		"a non-empty valid namespace must be accepted");
	require_equal(*key,"ExampleEntity.effect.attack.normal",
		"namespace and logical components must be joined in the documented order");

	auto appended = ResourceKeyBuilder::append_component("ExampleEntity.effect","slash_01");
	require(appended,
		"a valid component must append to a valid base key");
	require_equal(*appended,"ExampleEntity.effect.slash_01",
		"append_component must insert exactly one separator");

	for (const auto& invalid_build : std::vector<std::pair<std::string, std::string>>{
		{"bad-id", ""}, {"hero", "bad namespace"}, {"hero", "\xE6\x95\x88\xE6\x9E\x9C"}})
	{
		auto result = ResourceKeyBuilder::build(
			invalid_build.first,invalid_build.second,{"idle"},std::nullopt);
		require(!result,
			"invalid entity and namespace components must be rejected by build");
		require(!result.error().message.empty(), "invalid build input must produce an error");
	}

	require(!ResourceKeyBuilder::build("hero","",{"bad-name"},std::nullopt),
		"invalid logical components must be rejected by build");

	auto missing_component = ResourceKeyBuilder::build("hero","",{},std::nullopt);
	require(!missing_component
		&& missing_component.error().code == elysia::core::KeyValidationError::MissingLogicalComponent,
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
		auto key = ResourceKeyBuilder::build(
			"entity","",{"attack"},test_case.index);
		require(key,
			"segments in the inclusive 0-99 range must build successfully");
		require_equal(*key,test_case.resource_key,
			"runtime resource-key segments must use unpadded decimal notation");

		std::string formatted;
		require(elysia::resources::format_filesystem_segment(test_case.index, formatted),
			"segments in the inclusive 0-99 range must have filesystem notation");
		require_equal(formatted, test_case.filesystem_component,
			"filesystem segments must use exactly two decimal digits");
	}

	auto invalid_segment = ResourceKeyBuilder::build("entity","",{"attack"},100);
	require(!invalid_segment
		&& invalid_segment.error().code == elysia::core::KeyValidationError::SegmentOutOfRange
		&& std::filesystem::path(invalid_segment.error().origin.file_name()).filename()
			== "resource_key_builder.cpp",
		"runtime segment failures must retain the concrete error-production location");

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
