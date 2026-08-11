#include "engine/gameplay/collision/gameplay_collision_service.h"
#include "tests/support/test_assertions.h"

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

using elysia::tests::require;

namespace
{
class FakeGameplayCollisionRuntime final
    : public elysia::gameplay::collision::IGameplayCollisionRuntime
{
public:
    [[nodiscard]] bool bind_actor(
        const elysia::gameplay::collision::ActorCollisionRig& rig
    ) override
    {
        ++bind_actor_calls;
        last_actor = rig;
        return operation_result;
    }

    [[nodiscard]] bool bind_collider(
        const elysia::gameplay::collision::ColliderBinding& binding
    ) override
    {
        ++bind_collider_calls;
        last_collider = binding;
        return operation_result;
    }

    [[nodiscard]] bool bind_hit_box(
        const elysia::gameplay::collision::HitBoxBinding& binding
    ) override
    {
        ++bind_hit_box_calls;
        last_hit_box = binding;
        return operation_result;
    }

    [[nodiscard]] bool unbind_collider(elysia::physics::ColliderId collider) override
    {
        ++unbind_collider_calls;
        last_unbound_collider = collider;
        return operation_result;
    }

    [[nodiscard]] bool request_drop_through(
        const elysia::gameplay::collision::DropThroughRequest& request
    ) override
    {
        ++drop_through_calls;
        last_drop_through = request;
        return operation_result;
    }

    [[nodiscard]] bool add_listener(
        elysia::gameplay::collision::GameplayCollisionListener& listener
    ) override
    {
        ++add_listener_calls;
        last_listener = &listener;
        return operation_result;
    }

    [[nodiscard]] bool remove_listener(
        const elysia::gameplay::collision::GameplayCollisionListener& listener
    ) override
    {
        ++remove_listener_calls;
        last_listener = &listener;
        return operation_result;
    }

    void end_attack_instance(
        elysia::gameplay::collision::AttackInstanceId attack_instance) override
    {
        ++end_attack_calls;
        last_ended_attack = attack_instance;
    }

    bool operation_result = true;
    int bind_actor_calls = 0;
    int bind_collider_calls = 0;
    int bind_hit_box_calls = 0;
    int unbind_collider_calls = 0;
    int drop_through_calls = 0;
    int add_listener_calls = 0;
    int remove_listener_calls = 0;
    int end_attack_calls = 0;
    elysia::gameplay::collision::ActorCollisionRig last_actor{};
    elysia::gameplay::collision::ColliderBinding last_collider{};
    elysia::gameplay::collision::HitBoxBinding last_hit_box{};
    elysia::physics::ColliderId last_unbound_collider = elysia::physics::InvalidColliderId;
    elysia::gameplay::collision::DropThroughRequest last_drop_through{};
    const elysia::gameplay::collision::GameplayCollisionListener* last_listener = nullptr;
    elysia::gameplay::collision::AttackInstanceId last_ended_attack =
        elysia::gameplay::collision::InvalidAttackInstanceId;
};

std::string capture_logs(const std::function<void()>& action)
{
    std::ostringstream captured;
    std::streambuf* previous_buffer = std::clog.rdbuf(captured.rdbuf());
    action();
    std::clog.rdbuf(previous_buffer);
    return captured.str();
}
}

int main()
{
    using namespace elysia::gameplay::collision;

    GameplayCollisionService* service = GameplayCollisionService::instance();
    require(!service->has_active_runtime(),
        "Gameplay collision service must start without an active runtime");

    ActorCollisionRig actor_rig;
    actor_rig.owner = 17;
    actor_rig.body = 101;

    bool no_runtime_result = true;
    const std::string no_runtime_log = capture_logs([&]
    {
        no_runtime_result = service->bind_actor(actor_rig);
    });
    require(!no_runtime_result,
        "Gameplay collision operations must fail without an active runtime");
    require(no_runtime_log.find("[ERROR]") != std::string::npos
            && no_runtime_log.find("[collision]") != std::string::npos
            && no_runtime_log.find("without an active runtime") != std::string::npos,
        "Missing runtimes must produce a collision error log");

    FakeGameplayCollisionRuntime first_runtime;
    FakeGameplayCollisionRuntime second_runtime;
    require(service->attach_runtime(first_runtime),
        "The first gameplay collision runtime must attach");
    require(service->attach_runtime(first_runtime),
        "Attaching the active gameplay collision runtime must be idempotent");
    require(service->has_active_runtime(),
        "An attached gameplay collision runtime must be observable");

    bool conflicting_attach_result = true;
    const std::string conflicting_attach_log = capture_logs([&]
    {
        conflicting_attach_result = service->attach_runtime(second_runtime);
    });
    require(!conflicting_attach_result
            && conflicting_attach_log.find("another runtime is already active") != std::string::npos,
        "A conflicting runtime must be rejected and logged");

    ColliderBinding collider_binding;
    collider_binding.collider = 102;
    collider_binding.owner = 17;
    collider_binding.team = teams::Player;
    collider_binding.role = ColliderRole::Body;

    HitBoxBinding hit_box_binding;
    hit_box_binding.collider.collider = 103;
    hit_box_binding.instigator = 17;
    hit_box_binding.attack_instance = 23;
    hit_box_binding.attack_definition = 29;

    require(service->bind_actor(actor_rig),
        "Actor bindings must be forwarded to the active runtime");
    require(service->bind_collider(collider_binding),
        "Collider bindings must be forwarded to the active runtime");
    require(service->bind_hit_box(hit_box_binding),
        "Hit-box bindings must be forwarded to the active runtime");
    require(service->unbind_collider(104),
        "Collider unbinds must be forwarded to the active runtime");
    require(first_runtime.bind_actor_calls == 1
            && first_runtime.last_actor.owner == 17
            && first_runtime.bind_collider_calls == 1
            && first_runtime.last_collider.collider == 102
            && first_runtime.bind_hit_box_calls == 1
            && first_runtime.last_hit_box.attack_instance == 23
            && first_runtime.unbind_collider_calls == 1
            && first_runtime.last_unbound_collider == 104,
        "The active runtime must receive exact binding and unbind data");
    require(second_runtime.bind_actor_calls == 0,
        "A rejected runtime must not receive forwarded operations");

    const DropThroughRequest drop_through{
        101,
        elysia::physics::CollisionTarget::from_collider(201)
    };
    require(service->request_drop_through(drop_through),
        "Valid drop-through requests must reach the active runtime");
    require(first_runtime.drop_through_calls == 1
            && first_runtime.last_drop_through.actor == 101
            && first_runtime.last_drop_through.target
                == elysia::physics::CollisionTarget::from_collider(201),
        "Drop-through forwarding must preserve the requested collider pair");

    GameplayCollisionListener listener;
    require(service->add_listener(listener)
            && service->remove_listener(listener),
        "Gameplay listeners must be forwarded to the active runtime");
    require(first_runtime.add_listener_calls == 1
            && first_runtime.remove_listener_calls == 1
            && first_runtime.last_listener == &listener,
        "Listener forwarding must preserve listener identity");
    service->end_attack_instance(23);
    require(first_runtime.end_attack_calls == 1
            && first_runtime.last_ended_attack == 23,
        "Ending an attack instance must be forwarded exactly once");

    const int valid_drop_through_calls = first_runtime.drop_through_calls;
    capture_logs([&]
    {
        require(!service->request_drop_through(DropThroughRequest{}),
            "Invalid collider IDs must be rejected");
        require(!service->request_drop_through(DropThroughRequest{
                101,
                elysia::physics::CollisionTarget::from_collider(101)
            }),
            "A collider must not request drop-through against itself");
    });
    require(first_runtime.drop_through_calls == valid_drop_through_calls,
        "Invalid drop-through requests must not reach the runtime");

    first_runtime.operation_result = false;
    require(!service->bind_actor(actor_rig),
        "Runtime operation failures must propagate through the singleton facade");

    bool mismatched_detach_result = true;
    capture_logs([&]
    {
        mismatched_detach_result = service->detach_runtime(second_runtime);
    });
    require(!mismatched_detach_result && service->has_active_runtime(),
        "Detaching a non-active runtime must preserve the active runtime");
    require(service->detach_runtime(first_runtime),
        "The active runtime must detach successfully");
    require(!service->has_active_runtime(),
        "Detaching the active runtime must clear the facade");

    bool detached_request_result = true;
    const std::string detached_request_log = capture_logs([&]
    {
        detached_request_result = service->request_drop_through(drop_through);
    });
    require(!detached_request_result
            && detached_request_log.find("without an active runtime") != std::string::npos,
        "Operations after detach must fail and log the missing runtime");

    std::cout << "gameplay collision service tests passed\n";
    return 0;
}
