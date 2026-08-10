#define SDL_MAIN_HANDLED

#include "engine/bootstrap/bootstrapper.h"
#include "engine/config/config_service.h"
#include "engine/config/user_config_service.h"
#include "tests/support/test_assertions.h"

#include <filesystem>

int main()
{
    const std::filesystem::path source_root = ELYSIA_SOURCE_DIR;
    const std::filesystem::path test_root =
        std::filesystem::temp_directory_path()
        / "elysia_bootstrap_config_integration_tests";
    std::filesystem::remove_all(test_root);
    for (const char* directory : {
            "assets/audio",
            "assets/textures",
            "assets/fonts" })
    {
        std::filesystem::create_directories(test_root / directory);
    }
    std::filesystem::copy_file(
        source_root / "assets/content_registry.json",
        test_root / "assets/content_registry.json");
    std::filesystem::copy(
        source_root / "assets/configs",
        test_root / "assets/configs",
        std::filesystem::copy_options::recursive);

    const auto result =
        elysia::bootstrap::Bootstrapper::instance()
            ->parse_runtime_settings(test_root);
    elysia::tests::require(
        result.has_value(),
        "Bootstrapper must load AppConfig and UserConfig");
	elysia::tests::require(
		result->content_registry.required.configs.filename() == "config_manifest.json"
			&& result->content_registry.required.i18n.filename() == "i18n_manifest.json"
			&& result->content_registry.bootstrap.preload_manifest.filename() == "preload_manifest.json"
			&& result->content_registry.additional_module_manifests.empty(),
		"Bootstrapper must return the resolved immutable content registry snapshot");
    auto* configs = elysia::config::ConfigService::instance();
    elysia::tests::require(!configs->is_initialized(),
        "Bootstrapper must not publish gameplay configuration before content loading");
    elysia::tests::require(elysia::config::UserConfigService::instance()->is_initialized(),
        "UserConfig startup behavior must remain integrated");
    configs->shutdown();
    elysia::config::UserConfigService::instance()->shutdown();
    std::filesystem::remove_all(test_root);
    return 0;
}
