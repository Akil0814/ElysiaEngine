#include "resource_load_plan_preflight.h"

#include "resource_load_plan.h"
#include "../io/path/path_manager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace elysia::loading
{
namespace
{
std::filesystem::path relative_to_project(const std::filesystem::path& path)
{
    if (path.empty() || !path.is_absolute()) return path.lexically_normal();
    auto* paths = elysia::io::PathManager::instance();
    if (!paths || !paths->is_initialized()) return path.filename();
    std::error_code error;
    auto relative = std::filesystem::relative(path,paths->root(),error);
    if (error || relative.empty() || relative == ".."
        || relative.generic_string().starts_with("../"))
        return path.filename();
    return relative.lexically_normal();
}

void add_missing(
    std::vector<elysia::core::FailureDiagnosticEntry>& missing,
    std::string type,std::string key,const std::filesystem::path& path,
    const elysia::resources::ResourceOrigin& resource_origin,
    std::string reason,
    std::source_location origin = std::source_location::current())
{
    missing.push_back(elysia::core::make_failure_diagnostic_entry(
        std::move(type),std::move(key),relative_to_project(path),
        relative_to_project(resource_origin.config_path),resource_origin.json_pointer,
        std::move(reason),origin));
}

bool regular_file(const std::filesystem::path& path,std::string& reason)
{
    std::error_code error;
    if (std::filesystem::is_regular_file(path,error)) return true;
    reason = error ? error.message() : "file does not exist or is not a regular file";
    return false;
}

bool directory(const std::filesystem::path& path,std::string& reason)
{
    std::error_code error;
    if (std::filesystem::is_directory(path,error)) return true;
    reason = error ? error.message() : "directory does not exist or is not a directory";
    return false;
}

std::filesystem::path frame_path(
    const elysia::resources::AtlasBuildRequest& request,std::size_t index)
{
    std::ostringstream name;
    name << request.frame_filename_prefix << '_' << std::setw(3)
        << std::setfill('0') << index << ".png";
    return request.source_path / name.str();
}
}

std::expected<void,ContentLoadFailure> ResourceLoadPlanPreflight::validate(
    const ResourceLoadPlan& plan,
    std::vector<elysia::core::FailureDiagnosticEntry> missing) const
{
    for (auto& entry : missing)
    {
        entry.expected_path = relative_to_project(entry.expected_path);
        entry.declaration_path = relative_to_project(entry.declaration_path);
    }
    std::string reason;
    for (const auto& request : plan.texture_requests())
        if (!regular_file(request.file_path,reason))
            add_missing(missing,"texture",request.key,request.file_path,request.origin,reason);
    for (const auto& request : plan.font_requests())
        if (!regular_file(request.file_path,reason))
            add_missing(missing,"font",request.key,request.file_path,request.origin,reason);
    for (const auto& request : plan.sound_requests())
        if (!regular_file(request.file_path,reason))
            add_missing(missing,"sound",request.key,request.file_path,request.origin,reason);
    for (const auto& request : plan.music_requests())
        if (!regular_file(request.file_path,reason))
            add_missing(missing,"music",request.key,request.file_path,request.origin,reason);
    for (const auto& request : plan.atlas_build_requests())
    {
        if (request.source_type == elysia::resources::AtlasSourceType::HorizontalStrip)
        {
            if (!regular_file(request.source_path,reason))
                add_missing(missing,"atlas-strip",request.atlas_key,
                    request.source_path,request.origin,reason);
            continue;
        }
        if (!directory(request.source_path,reason))
        {
            add_missing(missing,"atlas-directory",request.atlas_key,
                request.source_path,request.origin,reason);
        }
        for (std::size_t index = 0; index < request.frame_count; ++index)
        {
            const auto expected = frame_path(request,index);
            if (!regular_file(expected,reason))
                add_missing(missing,"atlas-frame",request.atlas_key,
                    expected,request.origin,reason);
        }
    }
    if (missing.empty()) return {};

    std::sort(missing.begin(),missing.end(),[](const auto& left,const auto& right)
    {
        return std::tie(left.subject_type,left.subject_key,left.expected_path)
            < std::tie(right.subject_type,right.subject_key,right.expected_path);
    });
    return std::unexpected(make_content_load_failure(
        ContentLoadError::MissingResource,
        elysia::core::make_failure_diagnostic(
            "One or more required content resources are missing.",std::move(missing))));
}
}
