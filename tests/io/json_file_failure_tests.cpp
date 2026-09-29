#include "engine/core/diagnostics/failure_diagnostic.h"
#include "engine/io/json/json_loader.h"
#include "engine/io/json/strict_json.h"
#include "engine/io/loaders/i18n_manifest_loader.h"
#include "tests/support/test_assertions.h"
#include "engine/config/content/config_load_utils.h"
#include "engine/io/loaders/detail/content_registry_json_failure.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using elysia::tests::require;

void test_read_failure_projection()
{
    using namespace elysia;
    const auto origin = std::source_location::current();
    const io::JsonFileFailure failure{io::JsonFileError::ReadFailed,"read interrupted",
        "assets/config.json",{},"/field",origin};
    for (const auto name : {"", "game"})
    {
        const auto projected = config::config_failure_from_json(failure,
            config::ConfigOrigin{failure.file_path.generic_string(),{},name,{}});
        require(projected.error == config::ConfigLoadError::FilesystemAccess
            && projected.message == failure.message && projected.origin.line() == origin.line()
            && projected.first.json_pointer == failure.json_pointer
            && projected.first.config_path == failure.file_path.generic_string(),
            "manifest and document conversions retain read error category and origin");
    }
    const auto registry = io::detail::registry_failure_from_json(failure);
    require(registry.code == io::ContentRegistryError::FilesystemAccess
        && registry.diagnostic.message == failure.message && registry.diagnostic.origin.line() == origin.line()
        && registry.diagnostic.entries.front().declaration_pointer == failure.json_pointer
        && registry.diagnostic.entries.front().declaration_path == failure.file_path,
        "registry conversion retains read error category and complete diagnostic");
}

void test_typed_json_failures()
{
    const auto root = std::filesystem::temp_directory_path()
        / "elysia_json_file_failure_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto empty = elysia::io::load_strict_json({});
    require(!empty && empty.error().code == elysia::io::JsonFileError::EmptyPath,
        "an empty JSON path must have a dedicated typed failure");

    const auto missing_path = root / "missing.json";
    const auto missing = elysia::io::load_strict_json(missing_path);
    require(!missing && missing.error().code == elysia::io::JsonFileError::FileMissing
        && missing.error().file_path == missing_path
        && missing.error().message.find(missing_path.string()) == std::string::npos,
        "missing JSON must keep its path out of the summary");

    const auto malformed_path = root / "malformed.json";
    std::ofstream(malformed_path) << R"({"value":)";
    const auto malformed = elysia::io::load_strict_json(malformed_path);
    require(!malformed && malformed.error().code == elysia::io::JsonFileError::ParseFailed
        && malformed.error().message.find(malformed_path.string()) == std::string::npos,
        "JSON parse failures must be typed and path-free");

    const auto duplicate_path = root / "duplicate.json";
    std::ofstream(duplicate_path) << R"({"value":1,"value":2})";
    const auto duplicate = elysia::io::load_strict_json(duplicate_path);
    require(!duplicate
        && duplicate.error().code == elysia::io::JsonFileError::DuplicateProperty
        && duplicate.error().duplicate_property == "value"
        && duplicate.error().json_pointer == "/value",
        "strict JSON must report duplicate property metadata");

    elysia::io::JsonLoader loader;
    const auto opened = loader.open_file(duplicate_path);
    require(!opened && !loader.is_loaded()
        && opened.error().code == elysia::io::JsonFileError::DuplicateProperty,
        "JsonLoader must reuse strict typed parsing and publish no partial state");

    using elysia::io::ManifestLoadError;
    const auto manifest_path = root / "i18n.json";
    const auto check = [&](elysia::io::json document,ManifestLoadError code,std::string pointer)
    {
        std::ofstream(manifest_path) << document.dump();
        const auto result = elysia::io::I18nManifestLoader{}.load(manifest_path);
        require(!result && result.error().code == code
            && result.error().diagnostic.entries.front().declaration_pointer == pointer,
            "schema failures must retain a typed reason and exact JSON pointer");
    };
    const elysia::io::json valid = {{"default_language","en"},{"languages",{"en"}},{"file",{"en.json"}}};
    auto document = valid; document.erase("default_language");
    check(document,ManifestLoadError::MissingField,"/default_language");
    document = valid; document["default_language"] = 42;
    check(document,ManifestLoadError::InvalidField,"/default_language");
    document = valid; document["default_language"] = "";
    check(document,ManifestLoadError::InvalidValue,"/default_language");
    document = valid; document["languages"] = 42;
    check(document,ManifestLoadError::InvalidField,"/languages");
    document = valid; document.erase("languages");
    check(document,ManifestLoadError::MissingField,"/languages");
    document = valid; document["languages"] = nullptr;
    check(document,ManifestLoadError::InvalidField,"/languages");
    document = valid; document["languages"] = {"en",42};
    check(document,ManifestLoadError::InvalidField,"/languages/1");
    document = valid; document["languages"] = {"en",""};
    check(document,ManifestLoadError::InvalidValue,"/languages/1");
    document = valid; document["languages"] = elysia::io::json::array();
    check(document,ManifestLoadError::MissingContent,"/languages");
    document = valid; document["file"] = {"en.json",false};
    check(document,ManifestLoadError::InvalidField,"/file/1");
    document = valid; document.erase("file");
    check(document,ManifestLoadError::MissingField,"/file");
    document = valid; document["file"] = "en.json";
    check(document,ManifestLoadError::InvalidField,"/file");
    document = valid; document["file"] = {"en.json",""};
    check(document,ManifestLoadError::InvalidValue,"/file/1");
    document = valid; document["file"] = elysia::io::json::array();
    check(document,ManifestLoadError::MissingContent,"/file");
    std::ofstream(manifest_path) << valid.dump();
    require(elysia::io::I18nManifestLoader{}.load(manifest_path),"valid i18n manifest must load");

    std::filesystem::remove_all(root);
}

void test_diagnostic_path_normalization()
{
    const std::filesystem::path root = "C:/game";
    require(elysia::core::normalize_diagnostic_path(
            root / "assets/textures/a.png",root).generic_string()
            == "assets/textures/a.png",
        "project paths must become project-relative");
    require(elysia::core::normalize_diagnostic_path(
            "D:/outside/secret.json",root).filename() == "secret.json",
        "paths outside the project must degrade to basename");
}
}

int main()
{
    test_read_failure_projection();
    test_typed_json_failures();
    test_diagnostic_path_normalization();
    std::cout << "json file failure tests passed\n";
    return EXIT_SUCCESS;
}
