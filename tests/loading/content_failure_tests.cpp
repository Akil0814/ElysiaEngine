#define SDL_MAIN_HANDLED

#include "engine/builtin/scenes/application_failure_presentation.h"
#include "engine/loading/content_load_failure.h"
#include "engine/loading/resource_load_plan.h"
#include "engine/loading/resource_load_plan_preflight.h"
#include "engine/io/path/path_manager.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/loading/game_content_loader.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <source_location>
#include <string>
#include <thread>
#include <utility>

#include <SDL.h>

int main()
{
    using elysia::tests::require;
    using elysia::loading::ContentLoadError;
    constexpr std::array cases{
        std::pair{ ContentLoadError::Config,"CONTENT-CONFIG" },
        std::pair{ ContentLoadError::Manifest,"CONTENT-MANIFEST" },
        std::pair{ ContentLoadError::Plan,"CONTENT-PLAN" },
        std::pair{ ContentLoadError::MissingResource,"CONTENT-MISSING" },
        std::pair{ ContentLoadError::Texture,"CONTENT-TEXTURE" },
        std::pair{ ContentLoadError::Atlas,"CONTENT-ATLAS" },
        std::pair{ ContentLoadError::Font,"CONTENT-FONT" },
        std::pair{ ContentLoadError::Audio,"CONTENT-AUDIO" },
        std::pair{ ContentLoadError::Animation,"CONTENT-ANIMATION" },
        std::pair{ ContentLoadError::Effect,"CONTENT-EFFECT" }
    };
    for (const auto& [stage,code] : cases)
    {
        const auto failure = elysia::loading::make_content_load_failure(stage,"failure");
        require(failure.error_code() == code,
            "each content stage must retain a stable external error code");
    }

    const std::source_location origin = std::source_location::current();
    const auto failure = elysia::loading::make_content_load_failure(
        ContentLoadError::Texture,"decoder rejected the image","ui.bad",
        "assets/textures/bad.png",origin);
    const auto route = elysia::builtin::make_application_failure_route(failure,"resource");
    const auto* payload = elysia::scene::try_scene_payload<
        elysia::builtin::ApplicationFailureScenePayload>(route.payload);
    require(payload && payload->diagnostic.origin.line() == origin.line()
            && payload->diagnostic.origin.file_name() == origin.file_name(),
        "failure wrapping must preserve the original source location");

    const auto release = elysia::builtin::build_application_failure_presentation(
        *payload,"Elysia.log",false);
    const auto debug = elysia::builtin::build_application_failure_presentation(
        *payload,"Elysia.log",true);
    require(release.diagnostic_details.empty()
            && release.copy_report.find("decoder rejected") == std::string::npos,
        "Release presentation must omit raw diagnostics");
    require(debug.diagnostic_details.find("ui.bad") != std::string::npos
            && debug.diagnostic_details.find("assets/textures/bad.png") != std::string::npos
            && debug.copy_report.find("decoder rejected") != std::string::npos
			&& debug.diagnostic_details.find(
				std::filesystem::path(origin.file_name()).filename().string())
				!= std::string::npos
			&& (std::filesystem::path(origin.file_name()).filename() == origin.file_name()
				|| debug.diagnostic_details.find(origin.file_name()) == std::string::npos),
        "Debug presentation must include diagnostics while hiding absolute source paths");

    const auto unavailable = elysia::builtin::build_application_failure_presentation(
        *payload,{},false);
    require(!unavailable.log_available
            && unavailable.copy_report.find("unavailable") != std::string::npos,
        "missing active logs must use a stable unavailable fallback");

    auto* paths = elysia::io::PathManager::instance();
    require(paths->initialize(),"content preflight tests must initialize project paths");
    elysia::resources::ResourceOrigin resource_origin;
    resource_origin.config_path = paths->configs() / "tests/missing_resources.json";
    resource_origin.json_pointer = "/resources/0";
    resource_origin.module = "test-module";
    const auto missing_root = paths->textures() / "missing-preflight";
    elysia::loading::ResourceLoadPlan plan;
    plan.texture_requests().push_back({"missing.texture",missing_root / "texture.png",resource_origin});
    plan.font_requests().push_back({"missing.font",missing_root / "font.ttf",24,resource_origin});
    plan.sound_requests().push_back({"missing.sound",missing_root / "sound.wav",resource_origin});
    plan.music_requests().push_back({"missing.music",missing_root / "music.ogg",resource_origin});
    plan.atlas_build_requests().push_back({
        .atlas_key = "missing.strip",.source_path = missing_root / "strip.png",
        .frame_count = 4,.source_type = elysia::resources::AtlasSourceType::HorizontalStrip,
        .origin = resource_origin});
    plan.atlas_build_requests().push_back({
        .atlas_key = "missing.frames",.source_path = paths->textures(),
        .frame_count = 2,.frame_filename_prefix = "missing_preflight_frame",
        .source_type = elysia::resources::AtlasSourceType::FrameDirectory,
        .origin = resource_origin});
    auto preflight = elysia::loading::ResourceLoadPlanPreflight{}.validate(plan);
    require(!preflight
            && preflight.error().error_code() == "CONTENT-MISSING"
            && preflight.error().diagnostic.entries.size() == 7,
        "full-load preflight must aggregate every missing file category");
    bool saw_texture = false,saw_font = false,saw_sound = false,saw_music = false;
    bool saw_strip = false,saw_frame = false;
    std::string previous;
    for (const auto& entry : preflight.error().diagnostic.entries)
    {
        const std::string ordering = entry.subject_type + "\n" + entry.subject_key
            + "\n" + entry.expected_path.generic_string();
        require(previous.empty() || previous <= ordering,
            "missing resource diagnostics must use deterministic ordering");
        previous = ordering;
        require(!entry.expected_path.is_absolute()
                && entry.expected_path.generic_string().starts_with("assets/")
                && entry.declaration_path == "assets/configs/tests/missing_resources.json"
                && entry.declaration_pointer == "/resources/0",
            "missing resource diagnostics must preserve relative paths and manifest origin");
        saw_texture = saw_texture || entry.subject_type == "texture";
        saw_font = saw_font || entry.subject_type == "font";
        saw_sound = saw_sound || entry.subject_type == "sound";
        saw_music = saw_music || entry.subject_type == "music";
        saw_strip = saw_strip || entry.subject_type == "atlas-strip";
        saw_frame = saw_frame || entry.subject_type == "atlas-frame";
    }
    require(saw_texture && saw_font && saw_sound && saw_music && saw_strip && saw_frame,
        "aggregated preflight diagnostics must identify each concrete resource type");
    const std::string formatted = elysia::core::format_failure_diagnostic(
        preflight.error().diagnostic,preflight.error().error_code(),"startup",paths->root());
    require(formatted.find("CONTENT-MISSING") != std::string::npos
            && formatted.find("type=texture") != std::string::npos
            && formatted.find("key=missing.texture") != std::string::npos
            && formatted.find("expected=assets/textures/missing-preflight/texture.png")
                != std::string::npos
            && formatted.find("declared_at=assets/configs/tests/missing_resources.json#/resources/0")
                != std::string::npos,
        "the handling boundary formatter must emit concrete type, key, path and declaration data");
    const auto missing_route = elysia::builtin::make_application_failure_route(
        preflight.error(),"startup");
    const auto* missing_payload = elysia::scene::try_scene_payload<
        elysia::builtin::ApplicationFailureScenePayload>(missing_route.payload);
    require(missing_payload
            && missing_payload->reason == elysia::builtin::ApplicationFailureReason::MissingResource,
        "CONTENT-MISSING must select the dedicated localized failure summary");
    const auto missing_release = elysia::builtin::build_application_failure_presentation(
        *missing_payload,"Elysia.log",false);
    const auto missing_debug = elysia::builtin::build_application_failure_presentation(
        *missing_payload,"Elysia.log",true);
    require(missing_release.diagnostic_details.empty()
            && missing_release.copy_report.find("missing.texture") == std::string::npos
            && missing_debug.diagnostic_details.find("missing.texture") != std::string::npos
            && missing_debug.copy_report.find("missing_preflight_frame_001.png")
                != std::string::npos,
        "Release must hide aggregated paths while Debug and copy include the complete list");

    const auto source_root = paths->root();
    const auto temporary_root = std::filesystem::temp_directory_path()
        / "elysia_content_missing_start_tests";
    std::filesystem::remove_all(temporary_root);
    std::filesystem::create_directories(temporary_root);
    std::filesystem::copy(
        source_root / "assets",temporary_root / "assets",
        std::filesystem::copy_options::recursive);
    std::filesystem::remove(temporary_root / "assets/textures/ui/moon.png");
    std::filesystem::remove(temporary_root / "assets/fonts/fusion-pixel-10px-proportional-latin.ttf");
    std::filesystem::remove(temporary_root / "assets/audio/system/button_click_down.wav");
    std::filesystem::remove(temporary_root
        / "assets/textures/examples/ryougi/RyougiShiki/animation/base/idle/RyougiShiki_idle_000.png");
    require(paths->initialize(temporary_root),
        "missing-resource start test must initialize its isolated project root");
    auto registry = elysia::io::ContentRegistryLoader{}.load(paths->content_registry());
    require(registry,"missing-resource start test must parse the copied registry");
    require(SDL_Init(SDL_INIT_VIDEO) == 0,
        "missing-resource start test must initialize SDL video");
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0,32,32,32,SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    require(renderer,"missing-resource start test must create a software renderer");
    elysia::loading::GameContentLoader content_loader;
    constexpr std::array point_sizes{10};
    const auto start = content_loader.start(renderer,*registry,point_sizes);
    require(!start
            && content_loader.state() == elysia::loading::GameContentLoaderState::Failed
            && content_loader.failure()
            && content_loader.failure()->error_code() == "CONTENT-MISSING"
            && content_loader.failure()->diagnostic.entries.size() == 4,
        "GameContentLoader start must publish one synchronous aggregate before starting workers");
    const bool saw_additional_module_frame = std::any_of(
        content_loader.failure()->diagnostic.entries.begin(),
        content_loader.failure()->diagnostic.entries.end(),[](const auto& entry)
        {
            return entry.subject_type == "atlas-frame"
                && entry.subject_key == "RyougiShiki.idle"
                && entry.expected_path.generic_string().ends_with(
                    "RyougiShiki_idle_000.png");
        });
    require(saw_additional_module_frame,
        "full-load preflight must include missing files from additional modules");
    content_loader.reset();

	std::filesystem::remove_all(temporary_root / "assets");
	std::filesystem::copy(
		source_root / "assets",temporary_root / "assets",
		std::filesystem::copy_options::recursive);
	auto runtime_registry = elysia::io::ContentRegistryLoader{}.load(
		paths->content_registry());
	require(runtime_registry,
		"post-preflight deletion test must parse the restored content registry");
	elysia::loading::GameContentLoader runtime_loader;
	const auto runtime_start = runtime_loader.start(renderer,*runtime_registry,point_sizes);
	require(runtime_start,
		"post-preflight deletion test must begin with every declared file present");
	std::filesystem::remove(temporary_root / "assets/textures/ui/moon.png");
	for (int attempt = 0; attempt < 1000 && runtime_loader.is_running(); ++attempt)
	{
		runtime_loader.update();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	require(runtime_loader.has_failed() && runtime_loader.failure()
		&& runtime_loader.failure()->error_code() == "CONTENT-TEXTURE"
		&& !runtime_loader.failure()->diagnostic.entries.empty(),
		"a texture deleted after preflight must remain a single asynchronous texture failure");
	const auto& runtime_entry = runtime_loader.failure()->diagnostic.entries.front();
	require(runtime_entry.subject_type == "texture"
		&& runtime_entry.subject_key == "ui.moon"
		&& runtime_entry.expected_path == "assets/textures/ui/moon.png"
		&& runtime_entry.declaration_path
			== "assets/configs/manifests/textures_manifest.json"
		&& runtime_entry.declaration_pointer == "/textures/ui.moon",
		"asynchronous failures must preserve concrete type, key, safe path and manifest origin");
	runtime_loader.reset();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    require(paths->initialize(source_root),
        "missing-resource start test must restore the source project root");
    std::filesystem::remove_all(temporary_root);
    return EXIT_SUCCESS;
}
