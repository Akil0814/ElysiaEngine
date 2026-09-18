#pragma once
#include "controller.h"
#include "../../tools/singleton.h"
#include "scene_control_context.h"
#include <expected>
#include <memory>
#include <map>
#include <deque>
namespace elysia::scene
{
class SceneManager;
}
namespace elysia::gameplay
{
enum class ControllerScope
{
    Scene,
    Session
};
enum class ControllerError
{
    NotInitialized,
    NoSession,
    SessionAlreadyActive,
    InvalidHandle,
    InvalidContext,
    InvalidTarget,
    TargetBusy,
    PlayerBusy,
    InvalidPlayer,
    InvalidMap
};
struct ControllerCreateInfo
{
    ControllerScope scope = ControllerScope::Scene;
    SceneControlToken scene;
};
struct ControllerDescription
{
    ControllerHandle handle;
    ControllerScope scope;
    SceneControlToken owner, bound_scene;
    std::uint64_t binding_generation;
    bool bound;
};
class ControllerService;
class ControllerManager final : public elysia::tools::Singleton<ControllerManager>
{
    friend class elysia::tools::Singleton<ControllerManager>;

  private:
    ControllerManager() = default;
    friend class ControllerService;
    friend class SceneControlContext;
    friend class GameplayScene;
    friend class elysia::scene::SceneManager;
    struct Entry
    {
        std::unique_ptr<Controller> controller;
        ControllerScope scope;
        SceneControlToken owner, bound_scene;
        elysia::core::GameObject *target = nullptr;
        ControlCommandReceiver *receiver = nullptr;
        ControlCommand command;
        bool removed = false, available = false, cancelling = false;
        elysia::core::GameObject *reserved_target = nullptr;
        SceneControlToken reserved_scene;
        std::uint64_t request_generation = 0;
        std::uint64_t eligible_dispatch = 0;
    };
    struct Boundary
    {
        ControllerManager &m;
        explicit Boundary(ControllerManager &manager) : m(manager)
        {
            ++m._depth;
        }
        ~Boundary()
        {
            if (--m._depth == 0)
                m.flush();
        }
    };
    void initialize();
    void shutdown();
    std::expected<void, ControllerError> begin_session();
    void end_session();
    std::expected<ControllerHandle, ControllerError> add(ControllerCreateInfo, std::unique_ptr<Controller>);
    Entry *find(ControllerHandle);
    SceneControlContext *context(SceneControlToken);
    std::expected<void, ControllerError> bind(ControllerHandle, SceneControlToken,
                                              elysia::core::GameObject &);
    std::expected<void, ControllerError> validate_binding(Entry &, SceneControlToken,
                                                          elysia::core::GameObject &);
    bool remove(ControllerHandle);
    bool unbind(ControllerHandle);
    void detach(Entry &, InputCancelReason);
    void cancel(Entry &, InputCancelReason);
    void ingest(Entry &, elysia::input::ActionInputResult);
    void input(SceneControlContext &, const elysia::input::InputSnapshot &);
    void cancel_local(SceneControlContext &, elysia::input::LocalPlayerId, InputCancelReason);
    void pause(SceneControlContext &);
    void leave(SceneControlContext &, bool release);
    void object_removed(SceneControlContext &, elysia::core::GameObject &);
    void advance(SceneControlContext &, std::uint64_t, double);
    std::expected<void, ControllerError> replace_map(ControllerHandle, elysia::input::InputActionMap);
    void flush();
    bool _initialized = false, _session = false, _flushing = false;
    std::uint64_t _runtime = 1, _next_id = 1, _dispatch = 0;
    unsigned _depth = 0;
    std::map<std::uint64_t, Entry> _entries;
    std::map<std::uint64_t, SceneControlContext *> _contexts;
    std::deque<std::function<void()>> _pending;
};
} // namespace elysia::gameplay
