#define SDL_MAIN_HANDLED

#include "engine/animation/animation_service.h"
#include "engine/animation/runtime/animation_manager.h"
#include "engine/application/lifecycle/application_termination_logging.h"
#include "engine/io/loaders/content_registry_loader.h"
#include "engine/io/path/path_manager.h"
#include "engine/loading/content_manifest_pipeline.h"
#include "engine/resources/pipeline/resource_request_builder.h"
#include "engine/resources/resource_service.h"
#include "engine/resources/runtime/resource_manager.h"
#include "engine/tools/logger.h"
#include "tests/support/test_assertions.h"

#include <SDL.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using elysia::tests::require;

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return { std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>() };
}

std::size_t count_occurrences(std::string_view text,std::string_view needle)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(needle,position)) != std::string_view::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}

void remove_test_path(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(path,error);
    require(!error,"logger test cleanup must succeed");
}

class CapturedConsoleLogs final : public std::streambuf
{
public:
    std::vector<std::string> messages;

protected:
    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character,traits_type::eof()))
            return traits_type::not_eof(character);
        append_character(traits_type::to_char_type(character));
        return character;
    }

    std::streamsize xsputn(const char* text,std::streamsize count) override
    {
        for (std::streamsize index = 0;index < count;++index)
            append_character(text[index]);
        return count;
    }

private:
    void append_character(char character)
    {
        if (character == '\n')
        {
            messages.push_back(std::move(_current_line));
            _current_line.clear();
            return;
        }
        if (character != '\r')
            _current_line.push_back(character);
    }

    std::string _current_line;
};

void test_logger_console_sink()
{
    using namespace elysia;
    auto* path_manager = io::PathManager::instance();
    require(path_manager->init() && path_manager->ensure_runtime_dirs(),
        "console logger test must initialize runtime paths");
    auto* logger = tools::Logger::instance();
    logger->shutdown();

    CapturedConsoleLogs captured;
    captured.messages.reserve(8);
    std::streambuf* previous_console_buffer = std::clog.rdbuf(&captured);

    tools::LoggerConfig console_config;
    console_config.file_mode = tools::LogFileMode::Disabled;
    console_config.console_color_mode = tools::ConsoleColorMode::Never;
    require(logger->configure(console_config),"logger must accept console configuration");
    logger->initialize();
    const unsigned int console_call_line = __LINE__ + 1;
    ELYSIA_LOG("console-test","console marker");
    require(captured.messages.size() == 1,"enabled console sink must emit exactly once");
    require(captured.messages.front().find("INFO:") == std::string::npos
            && captured.messages.front().find("\x1b[") == std::string::npos
            && captured.messages.front().find("[INFO]") != std::string::npos
            && captured.messages.front().find("[console-test]") != std::string::npos
            && captured.messages.front().find("console marker") != std::string::npos
            && captured.messages.front().find("logging_tests.cpp:" + std::to_string(console_call_line)) != std::string::npos
            && captured.messages.front().find("tests/tools/logging_tests.cpp") == std::string::npos
            && captured.messages.front().find("test_logger_console_sink") == std::string::npos
            && captured.messages.front().find("__cdecl") == std::string::npos,
        "console sink must emit one structured level without an SDL priority prefix");

    captured.messages.clear();
    const unsigned int terminating_call_line = __LINE__ + 1;
    ELYSIA_LOG_TERMINATING("console-test","termination marker");
    require(captured.messages.size() == 1
            && captured.messages.front().find("[TERMINATING]") != std::string::npos
            && captured.messages.front().find("termination marker") != std::string::npos
            && captured.messages.front().find("logging_tests.cpp:" + std::to_string(terminating_call_line)) != std::string::npos,
        "terminating macro must emit its level and original call site through the console sink");

    captured.messages.clear();
    resources::ResourceRequestBuilder request_builder;
    io::TextureManifest texture_manifest;
    std::vector<resources::TextureLoadRequest> texture_requests;
    require(!request_builder.append_texture_manifest_requests(
            texture_manifest,{},texture_requests),
        "invalid resource request input must remain a recoverable failure");
    require(captured.messages.size() == 1
            && captured.messages.front().find("[WARN]") != std::string::npos,
        "recoverable resource request failures must log at Warn level");

    resources::ResourceManager* resource_manager = resources::ResourceManager::instance();
    resources::ResourceService* resource_service = ELYSIA_RESOURCES;
    resource_manager->clear();
    auto require_missing_resource_logs = [&](const std::string& resource_type,auto&& find_resource)
    {
        captured.messages.clear();
        require(find_resource("") == nullptr,
            "an empty resource key must return nullptr");
        require(captured.messages.size() == 1
                && captured.messages.front().find("[WARN]") != std::string::npos
                && captured.messages.front().find("[resource]") != std::string::npos
                && captured.messages.front().find(
                    "Find " + resource_type + " failed: key is empty.") != std::string::npos,
            "an empty resource key must emit exactly one resource warning");

        captured.messages.clear();
        const std::string missing_key = "missing." + resource_type;
        require(find_resource(missing_key) == nullptr,
            "a missing resource key must return nullptr");
        require(captured.messages.size() == 1
                && captured.messages.front().find("[WARN]") != std::string::npos
                && captured.messages.front().find("[resource]") != std::string::npos
                && captured.messages.front().find(
                    "Find " + resource_type + " failed: resource does not exist: "
                    + missing_key) != std::string::npos,
            "a missing resource key must emit exactly one resource warning containing its key");
    };
    require_missing_resource_logs("texture",[&](const std::string_view key)
    {
        return resource_service->find_texture(key);
    });
    require_missing_resource_logs("font",[&](const std::string_view key)
    {
        return resource_service->find_font(key);
    });
    require_missing_resource_logs("sound",[&](const std::string_view key)
    {
        return resource_service->find_sound(key);
    });
    require_missing_resource_logs("music",[&](const std::string_view key)
    {
        return resource_service->find_music(key);
    });
    require_missing_resource_logs("atlas",[&](const std::string_view key)
    {
        return resource_service->find_atlas(key);
    });

    animation::AnimationManager::instance()->clear();
    captured.messages.clear();
    require(ELYSIA_ANIMATIONS->create_animation("missing.animation") == nullptr,
        "AnimationService must reject an animation key without a registered definition");
    require(captured.messages.size() == 2
            && captured.messages[0].find("Find animation failed: definition does not exist: missing.animation")
                != std::string::npos
            && captured.messages[1].find("Create animation failed: definition does not exist: missing.animation")
                != std::string::npos,
        "missing animation creation must preserve its Find and Create warnings");

    captured.messages.clear();
    io::ContentRegistry content_registry;
    require(!io::ContentRegistryLoader{}.load(path_manager->assets() / "missing-assets-structure.json",content_registry),
        "missing top-level registry input must fail the registry loader");
    bool saw_loader_warning = false;
    for (const std::string& message : captured.messages)
        saw_loader_warning = saw_loader_warning || message.find("[WARN]") != std::string::npos;
    require(saw_loader_warning, "registry loader failure must retain its warning diagnostic");

    captured.messages.clear();
    loading::ContentManifestPipeline content_manifest_pipeline;
    loading::ContentManifestResult config_result;
    require(!content_manifest_pipeline.load(content_registry,config_result),
        "invalid supplied registry must fail the content manifest pipeline");
    bool saw_pipeline_error = false;
    for (const std::string& message : captured.messages)
    {
        saw_pipeline_error = saw_pipeline_error || message.find("[ERROR]") != std::string::npos;
    }
    require(saw_pipeline_error,
        "content manifest pipeline failure must record an error diagnostic");

    captured.messages.clear();
    const std::source_location root_failure_location = std::source_location::current();
    const tools::TerminationInfo termination_info{
        tools::TerminationReason::UnhandledException,
        "input",
        "termination root cause",
        root_failure_location,
        false,
        false
    };
    const unsigned int termination_decision_line = __LINE__ + 1;
    elysia::application::log_fault_exit_if_needed(
        elysia::application::ApplicationExitDecision::FaultExit,termination_info);
    require(captured.messages.size() == 2
            && captured.messages[0].find("[ERROR]") != std::string::npos
            && captured.messages[0].find("[input]") != std::string::npos
            && captured.messages[0].find("termination root cause") != std::string::npos
            && captured.messages[1].find("[TERMINATING]") != std::string::npos
            && captured.messages[1].find("Application terminating after an unhandled exception") != std::string::npos
            && captured.messages[1].find("logging_tests.cpp:" + std::to_string(termination_decision_line)) != std::string::npos,
        "published termination logging must emit the concrete error before one terminating event");

    captured.messages.clear();
    auto* termination_manager = tools::TerminationManager::instance();
    termination_manager->reset_for_testing();
    const unsigned int startup_termination_line = __LINE__ + 1;
    termination_manager->request_termination(
        tools::TerminationReason::FatalRuntimeFailure,
        "startup",
        "Startup content loading failed");
    const auto startup_termination_info = termination_manager->termination_info();
    require(startup_termination_info.has_value()
            && startup_termination_info->reason == tools::TerminationReason::FatalRuntimeFailure
            && startup_termination_info->category == "startup"
            && startup_termination_info->message == "Startup content loading failed"
            && startup_termination_info->location.line() == startup_termination_line,
        "startup content failures must publish a complete fatal termination request at the scene call site");
    require(elysia::application::resolve_application_exit(false,*termination_manager)
            == elysia::application::ApplicationExitDecision::FaultExit,
        "startup content failures must select a fault exit instead of a normal scene quit");
    const unsigned int startup_exit_decision_line = __LINE__ + 1;
    elysia::application::log_fault_exit_if_needed(
        elysia::application::ApplicationExitDecision::FaultExit,startup_termination_info);
    require(captured.messages.size() == 2
            && captured.messages[0].find("[ERROR]") != std::string::npos
            && captured.messages[0].find("[startup]") != std::string::npos
            && captured.messages[0].find("Startup content loading failed") != std::string::npos
            && captured.messages[1].find("[TERMINATING]") != std::string::npos
            && captured.messages[1].find("logging_tests.cpp:" + std::to_string(startup_exit_decision_line)) != std::string::npos,
        "startup content failures must emit the startup error followed by one terminating event");
    termination_manager->reset_for_testing();

    captured.messages.clear();
    elysia::application::log_fault_exit_if_needed(
        elysia::application::ApplicationExitDecision::NormalExit,std::nullopt);
    require(captured.messages.empty(),"normal exits must not emit a terminating event");
    logger->shutdown();

    captured.messages.clear();
    tools::LoggerConfig colored_console_config;
    colored_console_config.file_mode = tools::LogFileMode::Disabled;
    colored_console_config.console_color_mode = tools::ConsoleColorMode::Always;
    require(logger->configure(colored_console_config),"logger must accept forced console colors");
    logger->initialize();
    logger->debug("color-test","debug color marker");
    logger->info("color-test","info color marker");
    logger->warn("color-test","warn color marker");
    logger->error("color-test","error color marker");
    logger->terminating("color-test","terminating color marker");
    logger->shutdown();
    const std::array<std::string_view,5> expected_colored_levels{
        "\x1b[90m[DEBUG]\x1b[0m",
        "\x1b[36m[INFO]\x1b[0m",
        "\x1b[33m[WARN]\x1b[0m",
        "\x1b[31m[ERROR]\x1b[0m",
        "\x1b[38;5;88m[TERMINATING]\x1b[0m"
    };
    require(captured.messages.size() == expected_colored_levels.size(),
        "forced console colors must emit all five log levels");
    for (size_t index = 0;index < expected_colored_levels.size();++index)
    {
        require(captured.messages[index].find(expected_colored_levels[index]) != std::string::npos
                && count_occurrences(captured.messages[index],"\x1b[") == 2,
            "console colors must wrap only the level tag and reset immediately");
    }

    captured.messages.clear();
    const std::filesystem::path dual_sink_path = path_manager->logs() / "ui-lifecycle-console-dual.log";
    remove_test_path(dual_sink_path);
    tools::LoggerConfig dual_sink_config;
    dual_sink_config.file_mode = tools::LogFileMode::Append;
    dual_sink_config.append_file_name = dual_sink_path.filename().string();
    dual_sink_config.console_color_mode = tools::ConsoleColorMode::Always;
    require(logger->configure(dual_sink_config),"logger must accept dual-sink configuration");
    logger->initialize();
    const unsigned int dual_sink_call_line = __LINE__ + 1;
    logger->warn("console-test","dual sink marker");
    const unsigned int dual_terminating_call_line = __LINE__ + 1;
    logger->terminating("console-test","dual terminating marker");
    logger->shutdown();
    require(captured.messages.size() == 2
            && captured.messages[0].find("\x1b[33m[WARN]\x1b[0m") != std::string::npos
            && captured.messages[0].find("dual sink marker") != std::string::npos
            && captured.messages[0].find("logging_tests.cpp:" + std::to_string(dual_sink_call_line)) != std::string::npos
            && captured.messages[0].find("test_logger_console_sink") == std::string::npos
            && captured.messages[1].find("\x1b[38;5;88m[TERMINATING]\x1b[0m") != std::string::npos
            && captured.messages[1].find("dual terminating marker") != std::string::npos
            && captured.messages[1].find("logging_tests.cpp:" + std::to_string(dual_terminating_call_line)) != std::string::npos,
        "enabled console sink must receive each dual-sink entry exactly once");
    const std::string dual_sink_contents = read_text_file(dual_sink_path);
    require(count_occurrences(dual_sink_contents,"dual sink marker") == 1
            && count_occurrences(dual_sink_contents,"dual terminating marker") == 1
            && dual_sink_contents.find("logging_tests.cpp:" + std::to_string(dual_sink_call_line)) != std::string::npos
            && dual_sink_contents.find("logging_tests.cpp:" + std::to_string(dual_terminating_call_line)) != std::string::npos
            && dual_sink_contents.find("[TERMINATING]") != std::string::npos
            && dual_sink_contents.find("\x1b[") == std::string::npos
            && dual_sink_contents.find("test_logger_console_sink") == std::string::npos
            && dual_sink_contents.find("__cdecl") == std::string::npos,
        "enabled file sink must receive the same compact source location exactly once");
    remove_test_path(dual_sink_path);

    captured.messages.clear();
    const std::filesystem::path silent_path = path_manager->logs() / "ui-lifecycle-console-silent.log";
    remove_test_path(silent_path);
    tools::LoggerConfig silent_config;
    silent_config.file_mode = tools::LogFileMode::Append;
    silent_config.append_file_name = silent_path.filename().string();
    silent_config.console_enabled = false;
    silent_config.console_color_mode = tools::ConsoleColorMode::Always;
    require(logger->configure(silent_config),"logger must accept silent console configuration");
    logger->info("console-test","preinit silent marker");
    logger->initialize();
    logger->warn("console-test","initialized silent marker");
    require(captured.messages.empty(),"disabled console sink must suppress preinit and initialized output");
    logger->shutdown();
    require(read_text_file(silent_path).find("initialized silent marker") != std::string::npos,
        "disabled console sink must not disable the configured file sink");
    remove_test_path(silent_path);

    const std::filesystem::path blocked_path = path_manager->logs() / "ui-lifecycle-console-blocked";
    remove_test_path(blocked_path);
    std::error_code directory_error;
    std::filesystem::create_directory(blocked_path,directory_error);
    require(!directory_error,"console logger test must create a blocking directory");
    captured.messages.clear();
    tools::LoggerConfig fallback_config;
    fallback_config.file_mode = tools::LogFileMode::Append;
    fallback_config.append_file_name = blocked_path.filename().string();
    fallback_config.console_enabled = true;
    fallback_config.console_color_mode = tools::ConsoleColorMode::Never;
    require(logger->configure(fallback_config),"logger must accept file-fallback configuration");
    logger->initialize();
    const unsigned int fallback_call_line = __LINE__ + 1;
    logger->error("console-test","file fallback marker");
    require(captured.messages.size() == 1
            && captured.messages.front().find("file fallback marker") != std::string::npos
            && captured.messages.front().find("logging_tests.cpp:" + std::to_string(fallback_call_line)) != std::string::npos
            && captured.messages.front().find("test_logger_console_sink") == std::string::npos
            && captured.messages.front().find("__cdecl") == std::string::npos,
        "file failure must retain compact source context through the enabled console sink");
    logger->shutdown();
    remove_test_path(blocked_path);

    std::clog.rdbuf(previous_console_buffer);
}

void test_logger_file_modes_and_noexcept()
{
    using namespace elysia;
    auto* path_manager = io::PathManager::instance();
    require(path_manager->init(),"logger test must initialize PathManager");
    require(path_manager->ensure_runtime_dirs(),"logger test must create runtime directories");

    auto* logger = tools::Logger::instance();
    logger->shutdown();

    tools::LoggerConfig disabled_config;
    disabled_config.file_mode = tools::LogFileMode::Disabled;
    require(logger->configure(disabled_config),"logger must accept configuration before initialization");
    logger->initialize();
    logger->debug("logger-test","disabled marker");
    require(!logger->active_file_path().has_value(),"disabled logger must not open a file");
    require(!logger->configure(disabled_config),"logger configuration must be fixed after initialization");
    logger->shutdown();

    const std::filesystem::path append_path = path_manager->logs() / "ui-lifecycle-logger-append.log";
    remove_test_path(append_path);
    tools::LoggerConfig append_config;
    append_config.file_mode = tools::LogFileMode::Append;
    append_config.append_file_name = append_path.filename().string();
    require(logger->configure(append_config),"logger must accept append configuration");
    logger->initialize();
    const unsigned int append_call_line = __LINE__ + 1;
    logger->info("logger-test","append first marker");
    const auto active_append_path = logger->active_file_path();
    require(active_append_path.has_value() && *active_append_path == append_path,
        "append logger must expose its active file path");
    logger->shutdown();
    require(read_text_file(append_path).find("append first marker") != std::string::npos,
        "append logger must create and write its configured file");
    require(read_text_file(append_path).find("logging_tests.cpp:" + std::to_string(append_call_line)) != std::string::npos,
        "logger must retain the call-site source location");

    require(logger->configure(append_config),"logger must allow reconfiguration after shutdown");
    logger->initialize();
    logger->warn("logger-test","append second marker");
    logger->shutdown();
    const std::string append_contents = read_text_file(append_path);
    require(append_contents.find("append first marker") != std::string::npos
            && append_contents.find("append second marker") != std::string::npos,
        "append logger must preserve earlier content");
    remove_test_path(append_path);

    const std::filesystem::path filtered_path = path_manager->logs() / "ui-lifecycle-logger-filtered.log";
    remove_test_path(filtered_path);
    tools::LoggerConfig filtered_config;
    filtered_config.minimum_level = tools::LogLevel::Warn;
    filtered_config.file_mode = tools::LogFileMode::Append;
    filtered_config.append_file_name = filtered_path.filename().string();
    require(logger->configure(filtered_config),"logger must accept level filtering configuration");
    logger->initialize();
    logger->debug("logger-test","filtered debug marker");
    logger->warn("logger-test","retained warn marker");
    logger->shutdown();
    const std::string filtered_contents = read_text_file(filtered_path);
    require(filtered_contents.find("filtered debug marker") == std::string::npos
            && filtered_contents.find("retained warn marker") != std::string::npos,
        "logger must filter entries below its configured minimum level");
    remove_test_path(filtered_path);

    const std::filesystem::path error_filtered_path = path_manager->logs() / "ui-lifecycle-logger-error-filtered.log";
    remove_test_path(error_filtered_path);
    tools::LoggerConfig error_filtered_config;
    error_filtered_config.minimum_level = tools::LogLevel::Error;
    error_filtered_config.file_mode = tools::LogFileMode::Append;
    error_filtered_config.append_file_name = error_filtered_path.filename().string();
    require(logger->configure(error_filtered_config),"logger must accept error-level filtering configuration");
    logger->initialize();
    logger->warn("logger-test","filtered warn marker");
    logger->error("logger-test","retained error marker");
    logger->terminating("logger-test","retained terminating marker");
    logger->shutdown();
    const std::string error_filtered_contents = read_text_file(error_filtered_path);
    require(error_filtered_contents.find("filtered warn marker") == std::string::npos
            && error_filtered_contents.find("retained error marker") != std::string::npos
            && error_filtered_contents.find("retained terminating marker") != std::string::npos,
        "error-level filtering must retain errors and terminating events");
    remove_test_path(error_filtered_path);

    const std::filesystem::path terminating_filtered_path = path_manager->logs() / "ui-lifecycle-logger-terminating-filtered.log";
    remove_test_path(terminating_filtered_path);
    tools::LoggerConfig terminating_filtered_config;
    terminating_filtered_config.minimum_level = tools::LogLevel::Terminating;
    terminating_filtered_config.file_mode = tools::LogFileMode::Append;
    terminating_filtered_config.append_file_name = terminating_filtered_path.filename().string();
    require(logger->configure(terminating_filtered_config),"logger must accept terminating-level filtering configuration");
    logger->initialize();
    logger->error("logger-test","filtered error marker");
    logger->terminating("logger-test","terminating-only marker");
    logger->shutdown();
    const std::string terminating_filtered_contents = read_text_file(terminating_filtered_path);
    require(terminating_filtered_contents.find("filtered error marker") == std::string::npos
            && terminating_filtered_contents.find("terminating-only marker") != std::string::npos,
        "terminating-level filtering must retain only terminating events");
    remove_test_path(terminating_filtered_path);

    tools::LoggerConfig new_run_config;
    new_run_config.file_mode = tools::LogFileMode::NewRunFile;
    require(logger->configure(new_run_config),"logger must accept new-run configuration");
    logger->initialize();
    const auto first_run_path = logger->active_file_path();
    require(first_run_path.has_value() && first_run_path->filename().string().starts_with("Elysia-"),
        "new-run logger must use a timestamped filename");
    logger->error("logger-test","new run marker");
    logger->shutdown();
    require(logger->configure(new_run_config),"logger must allow a second new-run configuration");
    logger->initialize();
    const auto second_run_path = logger->active_file_path();
    require(second_run_path.has_value() && *second_run_path != *first_run_path,
        "new-run logger must avoid reusing an existing run file");
    logger->shutdown();
    require(read_text_file(*first_run_path).find("new run marker") != std::string::npos,
        "new-run logger must write its active file");
    remove_test_path(*first_run_path);
    remove_test_path(*second_run_path);

    const std::filesystem::path blocked_path = path_manager->logs() / "ui-lifecycle-logger-blocked";
    remove_test_path(blocked_path);
    std::error_code directory_error;
    std::filesystem::create_directory(blocked_path,directory_error);
    require(!directory_error,"logger test must create a blocking directory");
    tools::LoggerConfig blocked_config;
    blocked_config.file_mode = tools::LogFileMode::Append;
    blocked_config.append_file_name = blocked_path.filename().string();
    require(logger->configure(blocked_config),"logger must accept blocking-file configuration");
    logger->initialize();
    require(!logger->active_file_path().has_value(),"failed file open must disable the file sink");
    logger->log(tools::LogLevel::Debug,"logger-test","file failure log marker");
    logger->debug("logger-test","file failure debug marker");
    logger->info("logger-test","file failure info marker");
    logger->warn("logger-test","file failure warn marker");
    logger->error("logger-test","file failure error marker");
    logger->terminating("logger-test","file failure terminating marker");
    logger->shutdown();
    remove_test_path(blocked_path);
}

void test_path_manager_failure_logging()
{
    using namespace elysia;
    auto* path_manager = io::PathManager::instance();
    auto* logger = tools::Logger::instance();
    const std::filesystem::path original_working_directory = std::filesystem::current_path();
    const std::filesystem::path temporary_root = std::filesystem::temp_directory_path()
        / ("elysia-path-manager-log-test-" + std::to_string(SDL_GetTicks64()));
    remove_test_path(temporary_root);
    std::filesystem::create_directories(temporary_root);

    logger->shutdown();
    tools::LoggerConfig logger_config;
    logger_config.file_mode = tools::LogFileMode::Disabled;
    logger_config.console_color_mode = tools::ConsoleColorMode::Never;
    require(logger->configure(logger_config),"path logger test must configure the console sink");
    logger->initialize();

    CapturedConsoleLogs captured;
    std::streambuf* previous_console_buffer = std::clog.rdbuf(&captured);

    std::filesystem::current_path(temporary_root);
    require(!path_manager->init(),"a directory without an Elysia project root must fail initialization");
    require(captured.messages.size() == 1
            && captured.messages.front().find("[ERROR]") != std::string::npos
            && captured.messages.front().find("[path]") != std::string::npos
            && captured.messages.front().find("project root was not found") != std::string::npos,
        "missing project roots must emit a path-specific error");

    captured.messages.clear();
    const std::filesystem::path assets = temporary_root / "assets";
    std::filesystem::create_directories(assets / "audio");
    std::filesystem::create_directories(assets / "textures");
    std::filesystem::create_directories(assets / "fonts");
    std::filesystem::create_directories(assets / "configs");
    std::ofstream(assets / "content_registry.json") << "{}";
    require(path_manager->init(),"the controlled temporary project root must initialize");
    std::ofstream(temporary_root / "player_data") << "blocking file";
    require(!path_manager->ensure_runtime_dirs(),"a file at the runtime data path must fail directory setup");
    require(captured.messages.size() == 1
            && captured.messages.front().find("[ERROR]") != std::string::npos
            && captured.messages.front().find("[path]") != std::string::npos
            && captured.messages.front().find("runtime directory setup failed") != std::string::npos,
        "runtime directory setup failures must emit a path-specific error");

    std::filesystem::current_path(original_working_directory);
    require(path_manager->init() && path_manager->ensure_runtime_dirs(),
        "path logger test must restore the workspace path manager state");
    std::clog.rdbuf(previous_console_buffer);
    logger->shutdown();
    remove_test_path(temporary_root);
}
}

int main()
{
    test_logger_file_modes_and_noexcept();
    test_logger_console_sink();
    test_path_manager_failure_logging();
    std::cout << "logging tests passed\n";
    return EXIT_SUCCESS;
}
