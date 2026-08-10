#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

namespace elysia::resources
{
enum class ResourceError
{
    InvalidRequest,
    MissingFile,
    DecodeFailed,
    CreateFailed,
    StoreFailed,
    DuplicateResource,
    MissingBuildState,
    InvalidBuildState
};

struct ResourceFailure
{
    ResourceError code = ResourceError::InvalidRequest;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ResourceFailure make_resource_failure(
    ResourceError code,
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return ResourceFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(resource_key),
            std::move(resource_path),origin)
    };
}
}
