#include "application_failure_presentation.h"

#include <filesystem>
#include <sstream>

namespace elysia::builtin
{
namespace
{
std::string display_resource_path(const std::filesystem::path& path)
{
    if (path.empty())
        return {};
    if (!path.is_absolute())
        return path.generic_string();

    std::filesystem::path relative;
    bool found_assets = false;
    for (const auto& part : path)
    {
        if (!found_assets && part == "assets")
            found_assets = true;
        if (found_assets)
            relative /= part;
    }
    return found_assets ? relative.generic_string() : path.filename().generic_string();
}
}

std::string_view application_failure_summary_key(
    ApplicationFailureReason reason) noexcept
{
    using enum ApplicationFailureReason;
    switch (reason)
    {
    case Config: return "engine.application_failure.stage.config";
    case Manifest: return "engine.application_failure.stage.manifest";
    case Plan: return "engine.application_failure.stage.plan";
    case Texture: return "engine.application_failure.stage.texture";
    case Atlas: return "engine.application_failure.stage.atlas";
    case Font: return "engine.application_failure.stage.font";
    case Audio: return "engine.application_failure.stage.audio";
    case Animation: return "engine.application_failure.stage.animation";
    case Effect: return "engine.application_failure.stage.effect";
    case Startup: return "engine.application_failure.startup.message";
    case RuntimeFatal: return "engine.application_failure.runtime.message";
    }
    return "engine.application_failure.runtime.message";
}

ApplicationFailurePresentationModel build_application_failure_presentation(
    const ApplicationFailureScenePayload& payload,
    std::string log_file_name,
    bool include_diagnostics)
{
    ApplicationFailurePresentationModel model;
    model.summary_key = application_failure_summary_key(payload.reason);
    model.error_code = payload.error_code.empty() ? "APPLICATION-UNKNOWN" : payload.error_code;
    model.category = payload.category.empty() ? "application" : payload.category;
    model.log_available = !log_file_name.empty();
    model.log_file_name = model.log_available ? std::move(log_file_name) : std::string{};

    if (include_diagnostics)
    {
        std::ostringstream details;
        if (!payload.diagnostic.resource_key.empty())
            details << "Resource key: " << payload.diagnostic.resource_key << '\n';
        const std::string resource_path = display_resource_path(payload.diagnostic.resource_path);
        if (!resource_path.empty())
            details << "Resource path: " << resource_path << '\n';
        if (!payload.diagnostic.message.empty())
            details << "Cause: " << payload.diagnostic.message << '\n';
        details << "Source: " << payload.diagnostic.origin.file_name()
            << ':' << payload.diagnostic.origin.line();
        model.diagnostic_details = details.str();
    }

    std::ostringstream report;
    report << "Error code: " << model.error_code << '\n'
        << "Category: " << model.category << '\n'
        << "Log: " << (model.log_available ? model.log_file_name : "unavailable");
    if (include_diagnostics && !model.diagnostic_details.empty())
        report << "\n\n" << model.diagnostic_details;
    model.copy_report = report.str();
    return model;
}
}
