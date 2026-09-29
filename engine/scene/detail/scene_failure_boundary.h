#pragma once

#include "../scene_boundary_failure.h"
#include "../../core/render/render_failure.h"
#include <exception>
#include <expected>
#include <vector>

namespace elysia::scene::detail
{
inline void append_scene_failure_context(elysia::core::FailureDiagnostic& diagnostic,
    const SceneBoundaryFailure& failure,std::string_view stage)
{
    diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
        "scene-cleanup",std::to_string(failure.scene),{},{},{},
        std::string(stage) + " (" + std::string(scene_boundary_name(failure.boundary))
            + "): " + failure.diagnostic.message,failure.diagnostic.origin));
    diagnostic.entries.insert(diagnostic.entries.end(),
        failure.diagnostic.entries.begin(),failure.diagnostic.entries.end());
}

struct CapturedSceneFailure
{
    SceneKey scene;
    SceneBoundary boundary;
    std::string_view stage;
    std::source_location origin;
    std::exception_ptr exception;
};

// Private transport between nested cleanup helpers. Formatting happens only after cleanup.
class SceneFailureTransport
{
public:
    explicit SceneFailureTransport(std::vector<CapturedSceneFailure> failures) : failures(std::move(failures)) {}
    virtual ~SceneFailureTransport() = default;
    std::vector<CapturedSceneFailure> failures;
};

template<typename Error>
class SceneFailureException final : public Error,public SceneBoundaryTagged,public SceneFailureTransport
{
public:
    explicit SceneFailureException(std::vector<CapturedSceneFailure> failures)
        : Error(primary_message(failures.front().exception)),
          SceneBoundaryTagged(primary_boundary(failures.front()),primary_origin(failures.front())),
          SceneFailureTransport(std::move(failures)) {}
private:
    static std::string primary_message(const std::exception_ptr& exception)
    {
        try { std::rethrow_exception(exception); }
        catch (const std::exception& error) { return error.what(); }
        catch (...) { return "Unknown scene boundary exception."; }
    }
    static SceneBoundary primary_boundary(const CapturedSceneFailure& failure)
    {
        try { std::rethrow_exception(failure.exception); }
        catch (const SceneBoundaryTagged& error) { return error.scene_boundary(); }
        catch (...) { return failure.boundary; }
    }
    static std::source_location primary_origin(const CapturedSceneFailure& failure)
    {
        try { std::rethrow_exception(failure.exception); }
        catch (const SceneBoundaryTagged& error) { return error.origin(); }
        catch (...) { return failure.origin; }
    }
};

class SceneFailureCollector
{
public:
    void capture(SceneKey scene,SceneBoundary boundary,std::string_view stage,
        std::exception_ptr exception = std::current_exception(),
        std::source_location origin = std::source_location::current())
    {
        try { if (exception) std::rethrow_exception(exception); }
        catch (const SceneFailureTransport& error)
        {
            for (auto captured : error.failures)
            {
                if (captured.scene == SceneKeys::Invalid) captured.scene = scene;
                _failures.push_back(std::move(captured));
            }
            return;
        }
        catch (...) {}
        if (exception) _failures.push_back({scene,boundary,stage,origin,std::move(exception)});
    }

    template<typename Callable>
    void attempt(SceneKey scene,SceneBoundary boundary,std::string_view stage,Callable&& callable,
        std::source_location origin = std::source_location::current())
    {
        try { std::forward<Callable>(callable)(); }
        catch (...) { capture(scene,boundary,stage,std::current_exception(),origin); }
    }

    [[nodiscard]] bool empty() const noexcept { return _failures.empty(); }

    void rethrow_if_failed()
    {
        if (_failures.empty()) return;
        try { std::rethrow_exception(_failures.front().exception); }
        catch (const std::logic_error&) { throw SceneFailureException<std::logic_error>(std::move(_failures)); }
        catch (...) { throw SceneFailureException<std::runtime_error>(std::move(_failures)); }
    }

    [[nodiscard]] std::expected<void,SceneBoundaryFailure> finish() const
    {
        if (_failures.empty()) return {};
        for (std::size_t index = 0; index < _failures.size(); ++index)
        {
            try { if (_failures[index].exception) std::rethrow_exception(_failures[index].exception); }
            catch (const elysia::core::RenderBackendError& error)
            {
                auto backend = error.failure();
                backend.diagnostic = to_failure_diagnostic({
                    _failures[index].scene,_failures[index].boundary,std::move(backend.diagnostic)});
                for (std::size_t other = 0; other < _failures.size(); ++other)
                    if (other != index) append(backend.diagnostic,_failures[other]);
                throw elysia::core::RenderBackendError(std::move(backend));
            }
            catch (...) {}
        }
        auto primary = describe(_failures.front());
        for (std::size_t index = 1; index < _failures.size(); ++index)
            append(primary.diagnostic,_failures[index]);
        return std::unexpected(std::move(primary));
    }

private:
    static SceneBoundaryFailure describe(const CapturedSceneFailure& captured)
    {
        auto boundary = captured.boundary;
        auto origin = captured.origin;
        std::string message = "Unknown scene boundary exception.";
        try { if (captured.exception) std::rethrow_exception(captured.exception); }
        catch (const elysia::core::RenderBackendError& error)
        {
            auto diagnostic = error.failure().diagnostic;
            diagnostic.message = error.failure().operation + ": " + diagnostic.message;
            return {captured.scene,boundary,std::move(diagnostic)};
        }
        catch (const SceneBoundaryTagged& error)
        {
            boundary = error.scene_boundary();
            origin = error.origin();
            if (const auto* standard = dynamic_cast<const std::exception*>(&error)) message = standard->what();
        }
        catch (const std::exception& error) { message = error.what(); }
        catch (...) {}
        return make_scene_boundary_failure(captured.scene,boundary,std::move(message),origin);
    }

    static void append(elysia::core::FailureDiagnostic& diagnostic,const CapturedSceneFailure& captured)
    {
        append_scene_failure_context(diagnostic,describe(captured),captured.stage);
    }

    std::vector<CapturedSceneFailure> _failures;
};
}
