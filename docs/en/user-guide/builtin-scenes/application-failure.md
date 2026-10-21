# Application Failure Scene

This document explains when the built-in error interface can be displayed, how to pass diagnostics, and how confirmation exits. It applies when gameplay cannot continue but the window, rendering, and required built-in resources still work.

`ApplicationFailure` displays diagnostics for startup resource failures or fatal runtime errors and provides exit flow. It is not general retry, save rollback, or gameplay recovery; recoverable business failures belong to game flows.

## Requesting Error Display from a Scene

This derived scene has no business state. Call `show_fatal_error()` only during normal input/update, after engine and built-in UI resources are available. Routes accept `FailureDiagnostic`; explicitly convert strings to diagnostics.

```cpp
#include "engine/scene/scene.h"
#include "engine/builtin/scenes/application_failure_scene_payload.h"

class FailureExampleScene final : public elysia::scene::Scene
{
public:
    // This only shows failure routing without additional business objects.
    void on_enter(const elysia::scene::ScenePayload&) override
    {
    }

    void on_exit() override
    {
    }

    void on_reset() override
    {
    }

    void show_fatal_error()
    {
        // Use only while rendering/built-ins remain available; pass complete diagnostics to the failure scene.
        request_scene_switch(elysia::builtin::make_application_failure_route(
            elysia::builtin::ApplicationFailurePresentation::RuntimeFatal,
            "gameplay",
            elysia::core::make_failure_diagnostic(
                "Required game state could not be restored.")));
    }
};
```

The `ContentLoadFailure` helper overload creates startup loading failure routes preserving `category`, code, and diagnostics. Do not pass strings directly instead of `ApplicationFailureScenePayload`.

## Diagnostic Contents

Payload contains `presentation`, `reason`, `error_code`, `category`, and `diagnostic`. `StartupLoading` and `RuntimeFatal` are `presentation` modes determining wording and appearance, not promises that either class of error can recover.

Entry validates Payload type and mode. It deactivates project fonts and uses built-in `presentation` resources to reduce reliance on failed project content. Diagnostics should retain traceable categories/reasons rather than a generic failure message without location information.

Pass complete `FailureDiagnostic` with message, source, and additional failures; do not reconstruct it from a message substring. Convert engine scene boundary errors with `make_application_failure_route(failure)` and startup content errors with the matching `ContentLoadFailure` overload.

UI and full logs provide different detail: Release hides raw diagnostics onscreen, but complete diagnostics are logged before exit confirmation. Investigate logs rather than inferring causes solely from short UI messages.

## Exit and Capability Boundaries

User confirmation submits a termination request to the termination manager for application-wide exit; it does not return to the previous scene. The scene has no success return route or general reload-and-continue button.

The interface needs working rendering, window, required built-in resources, and error UI. Early initialization failure, error-interface construction failure, or unsafe rendering cannot be solved through routing. Do not assume all exceptions automatically become this scene; follow application error boundaries, preserve logs, and handle final exit results.

Do not recursively request switching in `on_enter()`, `on_exit()`, `on_reset()`, or rendering. After requesting fatal error handling, stop submitting business operations that depend on damaged state.

## Difference from Scene Exception Recovery

After game logic exceptions, the manager can clean up the faulted scene and enter another through a failure route. Recovery here means reaching a flow that can handle errors, not undoing damage, restoring saves, or resuming the original scene.

Failed scene cleanup or recovery enters `Faulted` without recursively constructing more recovery scenes. A throwing `on_enter()` receives no compensating `on_exit()`; detach external subscriptions/callbacks through RAII or local rollback. See [scene system](../scene/scene.md#entry-exit-failure-and-shutdown-order).

## Underlying Rendering Failures

SDL backend failures reach the application through `RenderBackendError` and directly cause fault exit, without attempting an error UI on the same renderer. The application stops drawing and closes resources. Games must not catch the error and keep submitting original commands.

Ordinary game code submits commands through scene objects rather than driving the executor. Custom rendering maintainers must check `expected<void, RenderFailure>` and retain original failures and state-restoration errors. See [render_failure.h](../../../../engine/core/render/render_failure.h).

## References

- [Diagnostic Payload and helpers](../../../../engine/builtin/scenes/application_failure_scene_payload.h)
- [Failure scene implementation](../../../../engine/builtin/scenes/application_failure_scene.cpp)
- [Startup loading](startup-loading.md), [logging](../tools/utilities.md), [back to user guide](../README.md)
