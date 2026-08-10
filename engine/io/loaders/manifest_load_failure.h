#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"

namespace elysia::io
{
enum class ManifestLoadError
{
    InvalidDocument,
    OpenFailed,
    InvalidSchema,
    InvalidField,
    MissingField,
    UnknownField,
    InvalidValue,
    DuplicateKey,
    InvalidResourceKey,
    MissingContent
};

struct ManifestLoadFailure
{
    ManifestLoadError code = ManifestLoadError::InvalidDocument;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline ManifestLoadFailure make_manifest_load_failure(
    ManifestLoadError code,
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return ManifestLoadFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(resource_key),
            std::move(resource_path),origin)
    };
}
}
