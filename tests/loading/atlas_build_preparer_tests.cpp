#define SDL_MAIN_HANDLED

#include "engine/resources/atlas/atlas_build_preparer.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
using elysia::tests::require;

void test_explicit_frame_directory_expansion()
{
	const std::filesystem::path atlas_test_root =
		std::filesystem::temp_directory_path() / "elysia_atlas_build_preparer_tests";
	std::filesystem::remove_all(atlas_test_root);
	std::filesystem::create_directories(atlas_test_root / "frames");
	std::ofstream(atlas_test_root / "frames" / "ExampleEntity_idle_000.png").put('\0');
	std::ofstream(atlas_test_root / "frames" / "ExampleEntity_idle_001.png").put('\0');
	std::ofstream(atlas_test_root / "frames" / "unconfigured_extra.png").put('\0');

	elysia::resources::AtlasBuildPreparer atlas_build_preparer;
	std::vector<elysia::resources::AtlasFramePrepareTask> atlas_tasks;
	elysia::resources::AtlasBuildRequest request;
	request.atlas_key = "ExampleEntity.idle";
	request.source_path = atlas_test_root / "frames";
	request.frame_count = 2;
	request.frame_filename_prefix = "ExampleEntity_idle";
	require(atlas_build_preparer.expand_build_request(request, atlas_tasks),
		"frame-directory atlas loading must expand its explicit prefix and frame count");
	require(atlas_tasks.size() == 2
		&& atlas_tasks[0].frame_path.filename() == "ExampleEntity_idle_000.png"
		&& atlas_tasks[1].frame_path.filename() == "ExampleEntity_idle_001.png"
		&& atlas_tasks[0].frame_index == 0
		&& atlas_tasks[1].frame_index == 1,
		"frame-directory tasks must use exact _000/_001 paths in configured order");
	require(atlas_tasks.size() == 2,
		"unconfigured extra PNG files must not be scanned into the atlas");

	elysia::resources::AtlasBuildRequest no_prefix = request;
	no_prefix.frame_filename_prefix.clear();
	require(!atlas_build_preparer.expand_build_request(no_prefix, atlas_tasks),
		"frame-directory requests without an explicit prefix must fail");

	request.frame_count = 3;
	require(!atlas_build_preparer.expand_build_request(request, atlas_tasks),
		"frame-directory loading must fail when an explicitly configured frame is missing");

	const std::filesystem::path strip_path = atlas_test_root / "strip.png";
	std::ofstream(strip_path).put('\0');
	elysia::resources::AtlasBuildRequest strip_request;
	strip_request.atlas_key = "strip";
	strip_request.source_path = strip_path;
	strip_request.frame_count = 14;
	strip_request.source_type = elysia::resources::AtlasSourceType::HorizontalStrip;
	require(atlas_build_preparer.expand_build_request(strip_request, atlas_tasks),
		"horizontal strip loading must accept a single image source");
	require(atlas_tasks.size() == 1
		&& atlas_tasks[0].frame_path == strip_path
		&& atlas_tasks[0].source_type == elysia::resources::AtlasSourceType::HorizontalStrip
		&& atlas_tasks[0].expected_frame_count == 14,
		"horizontal strip loading must expand to one preparation task");
	std::filesystem::remove_all(atlas_test_root);
}
}

int main()
{
    test_explicit_frame_directory_expansion();
    std::cout << "atlas build preparer tests passed\n";
    return EXIT_SUCCESS;
}
