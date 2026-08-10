#pragma once

#include <filesystem>
#include <source_location>
#include <string>
#include <utility>

namespace elysia::core
{
struct FailureDiagnostic
{
    std::string message;
    std::string resource_key;
    std::filesystem::path resource_path;
    std::source_location origin = std::source_location::current();
};

[[nodiscard]] inline FailureDiagnostic make_failure_diagnostic(
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return FailureDiagnostic{
        .message = std::move(message),
        .resource_key = std::move(resource_key),
        .resource_path = std::move(resource_path),
        .origin = origin
    };
}
}
