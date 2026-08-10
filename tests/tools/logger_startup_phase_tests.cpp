#define SDL_MAIN_HANDLED

#include "engine/io/path/path_manager.h"
#include "engine/tools/logger.h"
#include "tests/support/test_assertions.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using elysia::tests::require;

class CapturedConsole final : public std::streambuf
{
public:
    std::vector<std::string> lines;

protected:
    int_type overflow(int_type character) override
    {
        if (traits_type::eq_int_type(character,traits_type::eof()))
            return traits_type::not_eof(character);
        append(traits_type::to_char_type(character));
        return character;
    }

    std::streamsize xsputn(const char* text,std::streamsize count) override
    {
        for (std::streamsize index = 0;index < count;++index)
            append(text[index]);
        return count;
    }

private:
    void append(char character)
    {
        if (character == '\n')
        {
            lines.push_back(std::move(_current));
            _current.clear();
        }
        else if (character != '\r')
        {
            _current.push_back(character);
        }
    }

    std::string _current;
};

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary);
    return { std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>() };
}

void remove_path(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::remove_all(path,error);
    require(!error,"logger startup phase test cleanup must succeed");
}

void test_console_precedes_paths_and_file_follows()
{
    using namespace elysia;

    auto* logger = tools::Logger::instance();
    auto* paths = io::PathManager::instance();
    logger->shutdown();
    require(!paths->is_initialized(),
        "the standalone startup logger test must begin before PathManager initialization");

    CapturedConsole captured;
    std::streambuf* previous = std::clog.rdbuf(&captured);

    tools::LoggerConfig config;
    config.file_mode = tools::LogFileMode::Append;
    config.append_file_name = "logger-startup-phase.log";
    config.console_color_mode = tools::ConsoleColorMode::Always;
    require(logger->configure(config),
        "startup logger must accept configuration before either phase starts");

    logger->initialize_console();
    logger->initialize_console();
    require(logger->is_initialized() && !logger->active_file_path().has_value(),
        "console initialization must start the lifecycle without opening a file");
    require(!logger->configure(config),
        "configuration must lock as soon as the console phase starts");

    const std::filesystem::path missing_root =
        std::filesystem::temp_directory_path() / "elysia-logger-missing-project-root";
    remove_path(missing_root);
    std::filesystem::create_directories(missing_root);
    const auto root_result = paths->initialize(missing_root);
    require(!root_result,
        "a directory without an Elysia root must fail PathManager initialization");
    logger->error(
        "bootstrap",
        core::format_failure_diagnostic(
            root_result.error().diagnostic,
            root_result.error().error_code(),"bootstrap"),
        root_result.error().diagnostic.origin);
    logger->terminating("startup-test","early terminating marker");
    const auto error_lines = std::count_if(
        captured.lines.begin(),captured.lines.end(),[](const std::string& line)
        { return line.find("\x1b[31m[ERROR]\x1b[0m") != std::string::npos; });
    require(captured.lines.size() >= 2
            && error_lines == 1
            && captured.lines.front().find("[bootstrap]") != std::string::npos
            && captured.lines.front().find("BOOTSTRAP-PROJECT-ROOT") != std::string::npos
            && captured.lines.back().find("\x1b[38;5;88m[TERMINATING]\x1b[0m") != std::string::npos,
        "PathManager and terminating failures before Bootstrap must use colored level tags");

    logger->initialize_file();
    require(!logger->active_file_path().has_value(),
        "file initialization before PathManager must remain pending and retryable");

    require(paths->initialize(ELYSIA_SOURCE_DIR) && paths->ensure_runtime_dirs(),
        "the test must restore the real project paths before attaching the file sink");
    const std::filesystem::path log_path = paths->logs() / config.append_file_name;
    remove_path(log_path);

    logger->initialize_file();
    const auto active_path = logger->active_file_path();
    require(active_path.has_value() && *active_path == log_path,
        "file initialization must attach after PathManager becomes available");
    logger->initialize_file();
    require(logger->active_file_path() == active_path,
        "repeated file initialization must preserve the existing sink");
    logger->info("startup-test","post-bootstrap file marker");
    logger->shutdown();

    const std::string contents = read_file(log_path);
    require(contents.find("post-bootstrap file marker") != std::string::npos
            && contents.find("early terminating marker") == std::string::npos
            && contents.find("project root was not found") == std::string::npos,
        "the file sink must contain later logs without backfilling early console entries");
    remove_path(log_path);
    remove_path(missing_root);

    std::clog.rdbuf(previous);
}

void test_file_failure_can_retry_without_disabling_console()
{
    using namespace elysia;

    auto* logger = tools::Logger::instance();
    auto* paths = io::PathManager::instance();
    const std::filesystem::path blocked_path = paths->logs() / "logger-startup-blocked";
    remove_path(blocked_path);
    std::filesystem::create_directory(blocked_path);

    CapturedConsole captured;
    std::streambuf* previous = std::clog.rdbuf(&captured);

    tools::LoggerConfig config;
    config.file_mode = tools::LogFileMode::Append;
    config.append_file_name = blocked_path.filename().string();
    config.console_color_mode = tools::ConsoleColorMode::Always;
    require(logger->configure(config),
        "logger must allow reconfiguration after the previous shutdown");
    logger->initialize_console();
    logger->initialize_file();
    require(!logger->active_file_path().has_value(),
        "a blocked file path must leave the file phase retryable");
    logger->error("startup-test","console survives file failure");
    require(captured.lines.size() == 1
            && captured.lines.front().find("\x1b[31m[ERROR]\x1b[0m") != std::string::npos,
        "file initialization failure must not disable colored console output");

    remove_path(blocked_path);
    logger->initialize_file();
    require(logger->active_file_path() == blocked_path,
        "file initialization must succeed when an earlier blocking condition is removed");
    logger->warn("startup-test","retried file marker");
    logger->shutdown();
    require(read_file(blocked_path).find("retried file marker") != std::string::npos,
        "a retried file sink must receive subsequent log entries");
    remove_path(blocked_path);

    require(logger->configure(config),
        "shutdown must reset the split lifecycle and permit configuration again");
    logger->shutdown();
    std::clog.rdbuf(previous);
}
}

int main()
{
    test_console_precedes_paths_and_file_follows();
    test_file_failure_can_retry_without_disabling_console();
    return EXIT_SUCCESS;
}
