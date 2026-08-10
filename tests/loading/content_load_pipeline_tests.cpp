#define SDL_MAIN_HANDLED

#include "engine/io/loaders/content_registry_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/animated_entity_content_loader.h"
#include "engine/loading/content_manifest_pipeline.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace
{
using elysia::tests::require;

std::string json_path(const std::filesystem::path& path)
{
	return path.generic_string();
}

std::filesystem::path write_file(
	const std::filesystem::path& root,
	const std::string& name,
	const std::string& contents)
{
	const auto path = root / name;
	std::ofstream(path) << contents;
	return path;
}

std::string required_manifests()
{
	return R"("required":{"configs":"configs/manifests/config_manifest.json","fonts":"configs/manifests/fonts_manifest.json","audio":"configs/manifests/audio_manifest.json","i18n":"configs/manifests/i18n_manifest.json","textures":"configs/manifests/textures_manifest.json","animations":"configs/manifests/animations_manifest.json","effects":"configs/manifests/effects_manifest.json"})";
}

std::filesystem::path write_registry(
	const std::filesystem::path& root,
	const std::string& name,
	const std::string& additional = {})
{
	std::string manifests = "{" + required_manifests();
	if (!additional.empty()) manifests += ",\"additional\":" + additional;
	manifests += "}";
	return write_file(root, name,
		R"({"bootstrap":{"app_config":"configs/global/app_config.json","preload_manifest":"configs/manifests/preload_manifest.json"},"manifests":)"
		+ manifests + "}");
}

void test_current_repository_loads_minimal_sample()
{
	auto* path_manager = elysia::io::PathManager::instance();
	require(path_manager->initialize(), "path manager must initialize from the project root");

	elysia::loading::ContentManifestPipeline pipeline;
	auto registry_result = elysia::io::ContentRegistryLoader{}.load(path_manager->content_registry());
	require(registry_result,
		"current content registry must parse before it is supplied to the content pipeline");
	auto load_result = pipeline.load(*registry_result);
    require(load_result,
		"current content registry must load through the generic content pipeline");
	auto& result = *load_result;
	require(result.config_snapshot != nullptr,
		"content manifest loading must build the deferred generic config snapshot");
	const auto module = result.additional_modules.find("ryougi_sample");
	std::set<std::string> animation_names;
	if (module != result.additional_modules.end()
		&& module->second.animation_entries.size() == 1)
	{
		for (const auto& clip :
			module->second.animation_entries.front().animation_config.clips)
		{
			animation_names.insert(clip.animation_name);
		}
	}
	require(module != result.additional_modules.end()
		&& result.additional_modules.size() == 1
		&& module->second.entities.size() == 1
		&& module->second.entities.front().id == "RyougiShiki"
		&& module->second.animation_entries.size() == 1
		&& module->second.animation_entries.front().animation_config.clips.size() == 8
		&& animation_names == std::set<std::string>{
			"attack_normal", "idle", "run_loop" }
		&& module->second.effect_entries.empty()
		&& module->second.texture_entries.empty()
		&& module->second.audio_entries.empty()
		&& result.font_manifest.fonts.size() == 5
		&& result.texture_manifest.textures.size() == 1
		&& result.audio_manifest.sounds.size() == 2
		&& result.audio_manifest.music.empty()
		&& result.animation_manifest.animations.size() == 1
		&& result.animation_effect_manifest.effects.size() == 1,
		"the standalone repository must expose the reviewed core resources and Ryougi animation sample");
}

void test_arbitrary_and_empty_additional_module()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_generic_module_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto entities = write_file(root, "npcs.json",
		R"({"entities":[{"id":"Npc_1"}]})");
	const auto module = write_file(root, "npc_module.json",
		"{\"entities\":\"" + json_path(entities)
		+ "\",\"key_namespace\":\"\",\"capabilities\":{}}");

	elysia::loading::AnimatedEntityContentLoader loader;
	auto content = loader.load("npcs",module);
	require(content
		&& content->name == "npcs"
		&& content->entities.size() == 1
		&& content->entities.front().id == "Npc_1"
		&& content->animation_entries.empty()
		&& content->effect_entries.empty()
		&& content->texture_entries.empty()
		&& content->audio_entries.empty(),
		"an arbitrary module name with an empty capability object must be valid");
	content = loader.load("",module);
	require(content
		&& content->entities.front().origin.scope == elysia::resources::ResourceOriginScope::AdditionalModule
		&& content->entities.front().origin.module.empty(),
		"an empty additional module name must remain distinguishable from core origin scope");
	content = loader.load("core",module);
	require(content
		&& content->entities.front().origin.scope == elysia::resources::ResourceOriginScope::AdditionalModule
		&& content->entities.front().origin.module == "core",
		"an additional module literally named core must remain distinguishable from core origin scope");

	elysia::loading::ContentManifestPipeline pipeline;
	const auto registry = write_registry(root, "registry.json",
		"{\"npcs\":\"" + json_path(module) + "\"}");
	auto content_registry = elysia::io::ContentRegistryLoader{}.load(registry);
	require(content_registry,
		"arbitrary-module registry must parse before it is supplied to the content pipeline");
	auto load_result = pipeline.load(*content_registry);
	require(load_result
		&& load_result->additional_modules.size() == 1
		&& load_result->additional_modules.contains("npcs")
		&& load_result->additional_modules.at("npcs").entities.size() == 1,
		"ContentManifestPipeline must dispatch arbitrary additional names through the same loader");

	std::filesystem::remove_all(root);
}

void test_module_schema_rejections()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_module_schema_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	const auto entities = write_file(root, "entities.json",
		R"({"entities":[{"id":"Npc"}]})");
	const std::string prefix = "{\"entities\":\"" + json_path(entities) + "\",\"key_namespace\":\"\",";

	elysia::loading::AnimatedEntityContentLoader loader;
	auto result = loader.load("effects_only",write_file(root,"effects_only.json",
		prefix + R"("capabilities":{"effects":{"config_template":"configs/{id}/effects.json"}}})"));
	require(!result && result.error().diagnostic.message.find("effects requires animations") != std::string::npos,
		"effects capability must require animations in the same module");
	result = loader.load("unknown",write_file(root,"unknown_capability.json",
		prefix + R"("capabilities":{"particles":{}}})"));
	require(!result && result.error().diagnostic.message.find("unknown capability") != std::string::npos,
		"module manifests must reject capabilities outside animations/effects/textures/audio");
	require(!loader.load("legacy",write_file(root,"legacy_resources.json",
		prefix + R"("resources":{},"capabilities":{}})")),
		"module manifests must reject the removed shared resources object");
	require(!loader.load("missing_namespace",write_file(root,"missing_namespace.json",
		"{\"entities\":\"" + json_path(entities) + "\",\"capabilities\":{}}")),
		"module manifests must require key_namespace even when it is empty");
	result = loader.load("unknown_field",write_file(root,"unknown_animation_field.json",
		prefix + R"("capabilities":{"animations":{"texture_root":"textures/{id}","config_template":"configs/{id}.json","layouts":{},"legacy":true}}})"));
	require(!result && result.error().diagnostic.message.find("unknown animations capability field") != std::string::npos,
		"capability objects must reject unknown fields");

	std::filesystem::remove_all(root);
}

void test_texture_only_and_audio_only_modules()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_single_capability_module_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	elysia::loading::AnimatedEntityContentLoader loader;
	const auto entities = write_file(root, "entities.json",
		R"({"entities":[{"id":"Example"}]})");
	const auto texture_layout = write_file(root, "texture_layout.json",
		R"({"portrait":"portrait.png"})");
	const auto audio_layout = write_file(root, "audio_layout.json",
		R"({"select":"select.wav"})");
	std::filesystem::create_directories(root / "textures" / "Example");
	std::filesystem::create_directories(root / "audio" / "Example");

	const auto texture_module = write_file(root, "textures.json",
		"{\"entities\":\"" + json_path(entities)
		+ "\",\"key_namespace\":\"portrait\",\"capabilities\":{\"textures\":{\"texture_root\":\""
		+ json_path(root / "textures" / "{id}") + "\",\"layout\":\""
		+ json_path(texture_layout) + "\"}}}");
	auto content = loader.load("portraits",texture_module);
	require(content
		&& content->animation_entries.empty() && content->effect_entries.empty()
		&& content->texture_entries.size() == 1 && content->audio_entries.empty(),
		"a texture-only module must load without Animation, Effect, or Audio capabilities");

	std::filesystem::remove_all(root / "textures" / "Example");
	const auto missing_root = loader.load("portraits",texture_module);
	require(!missing_root
		&& missing_root.error().code == elysia::loading::ContentLoadError::Manifest
		&& !missing_root.error().diagnostic.entries.empty()
		&& missing_root.error().diagnostic.entries.front().subject_type == "textures"
		&& missing_root.error().diagnostic.entries.front().subject_key == "Example"
		&& missing_root.error().diagnostic.message.find("portraits") != std::string::npos
		&& missing_root.error().diagnostic.message.find("textures") != std::string::npos,
		"missing additional resource roots must fail as CONTENT-MANIFEST with module and capability context");

	const auto audio_module = write_file(root, "audio.json",
		"{\"entities\":\"" + json_path(entities)
		+ "\",\"key_namespace\":\"voice\",\"capabilities\":{\"audio\":{\"audio_root\":\""
		+ json_path(root / "audio" / "{id}") + "\",\"layout\":\""
		+ json_path(audio_layout) + "\"}}}");
	content = loader.load("voices",audio_module);
	require(content
		&& content->animation_entries.empty() && content->effect_entries.empty()
		&& content->texture_entries.empty() && content->audio_entries.size() == 1,
		"an audio-only module must load without Animation, Effect, or Texture capabilities");

	std::filesystem::remove(texture_layout);
	const auto missing_layout = loader.load("portraits",texture_module);
	require(!missing_layout
		&& !missing_layout.error().diagnostic.entries.empty()
		&& missing_layout.error().diagnostic.entries.front().expected_path
			== "texture_layout.json"
		&& missing_layout.error().diagnostic.message.find("texture_layout.json")
			!= std::string::npos,
		"missing additional-module layouts must preserve their exact safe diagnostic path");

	std::filesystem::remove_all(root);
}

void test_animation_frame_prefix_template_rules()
{
	const auto root = std::filesystem::temp_directory_path()
		/ "elysia_animation_prefix_module_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	elysia::loading::AnimatedEntityContentLoader loader;

	const std::string example_frame_animation_fields =
		R"("texture_root":"textures/example/normal/{id}","config_template":"configs/example/normal/{id}_animation_info.json","layouts":{"normal":"configs/example/entity_animation_layout.json"})";
	require(!loader.load("missing_prefix",write_file(root,"missing_prefix.json",
		R"({"entities":"configs/example/entities_manifest.json","key_namespace":"","capabilities":{"animations":{)"
		+ example_frame_animation_fields + "}}}")),
		"a module containing frame-directory configs must require frame_prefix_template");
	require(!loader.load("invalid_token",write_file(root,"invalid_token.json",
		R"({"entities":"configs/example/entities_manifest.json","key_namespace":"","capabilities":{"animations":{)"
		+ example_frame_animation_fields + R"(,"frame_prefix_template":"{id}_{animation}_{unknown}"}}})")),
		"frame_prefix_template must reject tokens outside the documented three-token set");

	const std::string example_animation_fields =
		R"("texture_root":"textures/example/{id}","config_template":"configs/example/{id}/animation_info.json","layouts":{"fighter":"configs/example/layouts/character_animation_layout.json"})";
	require(!loader.load("legacy_id_token",write_file(root,"legacy_id_token.json",
		R"({"entities":"configs/example/entities_manifest.json","key_namespace":"","capabilities":{"animations":{"texture_root":"textures/example/{asset_key}","config_template":"configs/example/{id}/animation_info.json","frame_prefix_template":"{id}_{animation}{segment_suffix}","layouts":{"fighter":"configs/example/layouts/character_animation_layout.json"}}}})")),
		"module capability templates must reject the removed asset_key token");
	require(!loader.load("missing_segment_suffix",write_file(root,"missing_segment_suffix.json",
		R"({"entities":"configs/example/entities_manifest.json","key_namespace":"","capabilities":{"animations":{)"
		+ example_animation_fields + R"(,"frame_prefix_template":"{id}_{animation}"}}})")),
		"a module with segmented animations must include segment_suffix in its frame prefix template");

	const auto strip_entities = write_file(root, "strip_entities.json",
		R"({"entities":[{"id":"Example","animation_layout":"normal"}]})");
	const auto strip_layout = write_file(root, "strip_layout.json",
		R"({"animations":{"idle":{"path":"idle"}}})");
	std::filesystem::create_directories(root / "textures" / "Example");
	std::filesystem::create_directories(root / "configs");
	write_file(root / "configs", "Example.json",
		R"({"defaults":{"source_type":"horizontal_strip"},"animations":{"idle":{"frame_count":4,"fps":10,"loop":true}}})");
	const auto strip_content = loader.load("strip_only",write_file(root,"strip_only.json",
		"{\"entities\":\"" + json_path(strip_entities)
		+ "\",\"key_namespace\":\"\",\"capabilities\":{\"animations\":{\"texture_root\":\""
		+ json_path(root / "textures" / "{id}") + "\",\"config_template\":\""
		+ json_path(root / "configs" / "{id}.json") + "\",\"layouts\":{\"normal\":\""
		+ json_path(strip_layout) + "\"}}}}"));
	require(strip_content
		&& strip_content->animation_entries.size() == 1
		&& strip_content->animation_entries.front().frame_prefix_template.empty()
		&& strip_content->animation_entries.front().animation_config.source_type
			== elysia::io::AnimationSourceType::HorizontalStrip,
		"a horizontal-strip-only module must not require an unused frame prefix template");

	std::filesystem::remove_all(root);
}

void test_content_registry_still_allows_core_only()
{
	const auto root = std::filesystem::temp_directory_path() / "elysia_core_only_registry_tests";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root);
	elysia::loading::ContentManifestPipeline pipeline;
	const auto registry_path = write_registry(root, "core_only.json");
	auto registry = elysia::io::ContentRegistryLoader{}.load(registry_path);
	require(registry,
		"core-only registry must parse before it is supplied to the content pipeline");
	std::filesystem::remove(registry_path);
	auto load_result = pipeline.load(*registry);
	require(load_result
		&& load_result->additional_modules.empty(),
		"content pipeline must use the supplied registry snapshot without rereading its source file");
	std::filesystem::remove_all(root);
}

void test_config_manifest_registry_contract()
{
	auto* paths = elysia::io::PathManager::instance();
	require(paths->initialize(),"PathManager must initialize for registry contract tests");
	elysia::io::ContentRegistryLoader loader;
	auto registry = loader.load(paths->content_registry());
	require(registry
		&& registry->required.configs == paths->assets()/"configs/manifests/config_manifest.json",
		"manifests.required.configs must be a required resolved entry");

	const auto root = std::filesystem::temp_directory_path()/"elysia_config_registry_contract_tests";
	std::filesystem::remove_all(root); std::filesystem::create_directories(root);
	const std::string missing_configs = R"({"bootstrap":{"app_config":"configs/global/app_config.json","preload_manifest":"configs/manifests/preload_manifest.json"},"manifests":{"required":{"fonts":"configs/manifests/fonts_manifest.json","audio":"configs/manifests/audio_manifest.json","i18n":"configs/manifests/i18n_manifest.json","textures":"configs/manifests/textures_manifest.json","animations":"configs/manifests/animations_manifest.json","effects":"configs/manifests/effects_manifest.json"}}})";
	require(!loader.load(write_file(root,"missing_configs.json",missing_configs)),
		"content registry must reject a missing manifests.required.configs");
	const std::string legacy_bootstrap = R"({"bootstrap":{"app_config":"configs/global/app_config.json","preload_manifest":"configs/manifests/preload_manifest.json","game_config_manifest":"configs/manifests/config_manifest.json"},"manifests":{)"
		+ required_manifests()+"}}";
	require(!loader.load(write_file(root,"legacy_bootstrap.json",legacy_bootstrap)),
		"content registry must reject the removed bootstrap.game_config_manifest field");
	std::filesystem::remove_all(root);
}
}

int main()
{
	test_current_repository_loads_minimal_sample();
	test_arbitrary_and_empty_additional_module();
	test_module_schema_rejections();
	test_texture_only_and_audio_only_modules();
	test_animation_frame_prefix_template_rules();
	test_content_registry_still_allows_core_only();
	test_config_manifest_registry_contract();
	std::cout << "content load pipeline tests passed\n";
	return EXIT_SUCCESS;
}
