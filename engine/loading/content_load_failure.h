#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

#include <string_view>

namespace elysia::loading
{
enum class ContentLoadError
{
    Config,
    Manifest,
    Plan,
    Texture,
    Atlas,
    Font,
    Audio,
    Animation,
    Effect
};

struct ContentLoadFailure
{
    ContentLoadError code = ContentLoadError::Manifest;
    elysia::core::FailureDiagnostic diagnostic;

    [[nodiscard]] std::string_view error_code() const noexcept
    {
        switch (code)
        {
        case ContentLoadError::Config: return "CONTENT-CONFIG";
        case ContentLoadError::Manifest: return "CONTENT-MANIFEST";
        case ContentLoadError::Plan: return "CONTENT-PLAN";
        case ContentLoadError::Texture: return "CONTENT-TEXTURE";
        case ContentLoadError::Atlas: return "CONTENT-ATLAS";
        case ContentLoadError::Font: return "CONTENT-FONT";
        case ContentLoadError::Audio: return "CONTENT-AUDIO";
        case ContentLoadError::Animation: return "CONTENT-ANIMATION";
        case ContentLoadError::Effect: return "CONTENT-EFFECT";
        }
        return "CONTENT-MANIFEST";
    }
};

[[nodiscard]] inline ContentLoadFailure make_content_load_failure(
    ContentLoadError code,
    elysia::core::FailureDiagnostic diagnostic)
{
    return ContentLoadFailure{ code,std::move(diagnostic) };
}

[[nodiscard]] inline ContentLoadFailure make_content_load_failure(
    ContentLoadError code,
    std::string message,
    std::string resource_key = {},
    std::filesystem::path resource_path = {},
    std::source_location origin = std::source_location::current())
{
    return ContentLoadFailure{
        code,
        elysia::core::make_failure_diagnostic(
            std::move(message),std::move(resource_key),
            std::move(resource_path),origin)
    };
}
}
