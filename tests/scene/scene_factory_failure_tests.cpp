#define SDL_MAIN_HANDLED
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/object_query/game_object_query_service.h"
#include "tests/support/test_assertions.h"

#include <cstdlib>
#include <new>

// Isolated executable: inject one failed allocation without production hooks.
namespace { thread_local bool fail_next_allocation = false; }
void* operator new(std::size_t size)
{
    if (fail_next_allocation)
    {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using namespace elysia;
using tests::require;
struct State { int destroyed = 0, exited = 0, detached = 0, removed = 0; };
class ProbeScene final : public scene::Scene
{
public:
    explicit ProbeScene(State& state, bool fail_commit = false) : state(state), fail_commit(fail_commit) {}
    ~ProbeScene() override
    {
        ++state.destroyed;
        if (fail_commit)
            require(lifecycle_state() == scene::SceneLifecycleState::PreparedForDestruction,
                "failed cache commit must prepare scene before releasing ownership");
    }
    void on_enter(const scene::ScenePayload&) override
    {
        create_and_add_object<ui::UiElement>();
        // Next allocation belongs to the cache insertion after successful entry.
        if (fail_commit) fail_next_allocation = true;
    }
    void on_exit() override { ++state.exited; }
    void on_reset() override {}
    void on_runtime_detach() override { ++state.detached; }
    void on_scene_object_removing(core::SceneObject&) override { ++state.removed; }
    State& state;
    bool fail_commit;
};

void factory_ownership()
{
    State state;
    scene::SceneFactory factory;
    std::unique_ptr<scene::Scene> first = std::make_unique<ProbeScene>(state);
    const auto* original = first.get();
    fail_next_allocation = true;
    bool failed = false;
    try { factory.store(1, first); }
    catch (const std::bad_alloc&) { failed = true; }
    require(failed && first.get() == original && !factory.find(1) && state.destroyed == 0,
        "allocation failure must leave caller ownership and cache unchanged");
    factory.store(1, first);
    require(!first && factory.find(1) == original, "successful store transfers ownership");
    std::unique_ptr<scene::Scene> duplicate = std::make_unique<ProbeScene>(state);
    const auto* duplicate_address = duplicate.get();
    failed = false;
    try { factory.store(1, duplicate); }
    catch (const std::logic_error&) { failed = true; }
    require(failed && duplicate.get() == duplicate_address && factory.find(1) == original,
        "duplicate key must not consume ownership or replace the cached scene");
    duplicate.reset();
    require(factory.destroy_all_scene() && state.destroyed == 2, "each candidate is destroyed exactly once");
}

void manager_commit_failure()
{
    State state;
    io::ContentRegistry registry;
    scene::SceneRuntimeContext context(nullptr, registry, 1280, 720);
    scene::SceneManager manager;
    manager.initialize(context);
    manager.register_game_scene<ProbeScene>(1, std::ref(state), true);
    manager.start({.target = 1});
    require(!fail_next_allocation, "cache failure injection must have fired");
    require(manager.state() == scene::SceneManagerState::Faulted,
        "unrecoverable cache commit failure must fault the manager");
    require(state.destroyed == 1 && state.exited == 1 && state.detached == 1 && state.removed == 1,
        "commit failure must detach, exit, retire objects and destroy exactly once");
    require(!object_query::GameObjectQueryService::instance()->is_available(),
        "failed cache commit must leave no query runtime");
    require(manager.shutdown(), "commit failure must not leave shutdown work broken");
}
}
int main()
{
    factory_ownership();
    manager_commit_failure();
}
