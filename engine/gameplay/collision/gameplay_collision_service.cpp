#include "gameplay_collision_service.h"

#include "../../tools/logger.h"

namespace elysia::gameplay::collision
{
bool GameplayCollisionService::attach_runtime(IGameplayCollisionRuntime& runtime) noexcept
{
    if (!_active_runtime)
    {
        _active_runtime = &runtime;
        return true;
    }

    if (_active_runtime == &runtime)
        return true;

    ELYSIA_LOG_ERROR(
        "collision",
        "Attach gameplay collision runtime failed: another runtime is already active."
    );
    return false;
}

bool GameplayCollisionService::detach_runtime(const IGameplayCollisionRuntime& runtime) noexcept
{
    if (!_active_runtime)
        return false;

    if (_active_runtime != &runtime)
    {
        ELYSIA_LOG_ERROR(
            "collision",
            "Detach gameplay collision runtime failed: runtime is not active."
        );
        return false;
    }

    _active_runtime = nullptr;
    return true;
}

bool GameplayCollisionService::has_active_runtime() const noexcept
{
    return _active_runtime != nullptr;
}

bool GameplayCollisionService::bind_actor(const ActorCollisionRig& rig)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind actor");
    return runtime && runtime->bind_actor(rig);
}

bool GameplayCollisionService::bind_collider(const ColliderBinding& binding)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind collider");
    return runtime && runtime->bind_collider(binding);
}

bool GameplayCollisionService::bind_hit_box(const HitBoxBinding& binding)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("bind hit box");
    return runtime && runtime->bind_hit_box(binding);
}

bool GameplayCollisionService::unbind_collider(elysia::physics::ColliderId collider)
{
    IGameplayCollisionRuntime* runtime = runtime_or_log("unbind collider");
    return runtime && runtime->unbind_collider(collider);
}

bool GameplayCollisionService::request_drop_through(const DropThroughRequest& request)
{
    if (request.actor == elysia::physics::InvalidColliderId
        || request.target == elysia::physics::InvalidColliderId
        || request.actor == request.target)
    {
        ELYSIA_LOG_ERROR(
            "collision",
            "Drop-through request failed: actor and target must be distinct valid collider IDs."
        );
        return false;
    }

    IGameplayCollisionRuntime* runtime = runtime_or_log("request drop-through");
    return runtime && runtime->request_drop_through(request);
}

IGameplayCollisionRuntime* GameplayCollisionService::runtime_or_log(
    std::string_view operation
) const noexcept
{
    if (_active_runtime)
        return _active_runtime;

    ELYSIA_LOG_ERROR(
        "collision",
        "Gameplay collision operation failed without an active runtime: " << operation
    );
    return nullptr;
}
}
