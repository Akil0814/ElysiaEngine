#pragma once

#include "engine/scene/scene.h"

namespace elysia::scene
{
// Narrow test-only access to the same non-virtual lifecycle entrypoints used by
// SceneManager. Tests can exercise a scene in isolation without making those
// entrypoints part of the engine's public API.
class SceneTestAccess final
{
public:
    static void bind(Scene& scene, const SceneRuntimeContext& context) noexcept
    {
        scene.bind_runtime_context(context);
    }

    static void enter(Scene& scene, const ScenePayload& payload = {})
    {
        scene.lifecycle_enter(payload);
    }

    static void exit(Scene& scene)
    {
        scene.lifecycle_exit();
    }

    static void reset(Scene& scene)
    {
        scene.lifecycle_reset();
    }

    static void input(Scene& scene, const elysia::input::InputSnapshot& input)
    {
        scene.lifecycle_input(input);
    }

    static void route_input(Scene& scene, const elysia::input::InputSnapshot& input)
    {
        scene._input_router.route(input);
    }

    static void update(Scene& scene, double delta)
    {
        scene.lifecycle_update(delta);
    }
};
} // namespace elysia::scene
