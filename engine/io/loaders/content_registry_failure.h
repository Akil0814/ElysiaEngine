#pragma once

#include "../../core/diagnostics/failure_diagnostic.h"

namespace elysia::io
{
enum class ContentRegistryError { OpenFailed,InvalidDocument,InvalidSchema,MissingReferencedFile };
struct ContentRegistryFailure
{
    ContentRegistryError code = ContentRegistryError::InvalidDocument;
    elysia::core::FailureDiagnostic diagnostic;
};
}
