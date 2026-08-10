#include "engine/core/diagnostics/failure_diagnostic.h"
#include "engine/io/json/json_loader.h"
#include "engine/io/json/strict_json.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using elysia::tests::require;

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
    test_typed_json_failures();
    test_diagnostic_path_normalization();
    std::cout << "json file failure tests passed\n";
    return EXIT_SUCCESS;
}
