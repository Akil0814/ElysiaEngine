#pragma once
#include "../../tools/singleton.h"
#include "controller.h"
#include "controller_types.h"
#include "scene_control_context.h"
#include <deque>
#include <expected>
#include <map>
#include <memory>
namespace elysia::scene
{
class SceneManager;
}
namespace elysia::gameplay
{
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
        std::uint64_t cancel_generation = 0;
        std::optional<std::uint64_t> producing_generation;
        bool source_cancelled = false;
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
    ControllerOperation bind(ControllerHandle, SceneControlToken, elysia::core::GameObject &);
    std::expected<void, ControllerError> validate_binding(Entry &, SceneControlToken,
                                                          elysia::core::GameObject &);
    std::expected<void, ControllerError> remove(ControllerHandle);
    ControllerOperation unbind(ControllerHandle);
    void detach(Entry &, InputCancelReason);
    void cancel(Entry &, InputCancelReason);
    void ingest(Entry &, elysia::input::ActionInputResult);
    void input(SceneControlContext &, const elysia::input::InputSnapshot &);
    void cancel_local(SceneControlContext &, elysia::input::LocalPlayerId, InputCancelReason);
    void pause(SceneControlContext &);
    void leave(SceneControlContext &, bool release);
    void object_removed(SceneControlContext &, elysia::core::GameObject &);
    void advance(SceneControlContext &, std::uint64_t, double);
    ControllerOperation replace_map(ControllerHandle, elysia::input::InputActionMap);
    enum class RequestKind
    {
        Bind,
        Unbind,
        Map
    };
    struct Request
    {
        RequestKind kind;
        ControllerHandle handle;
        SceneControlToken scene;
        elysia::core::GameObject *target = nullptr;
        elysia::input::InputActionMap map;
        ControllerOperation operation;
    };
    ControllerOperation request(std::shared_ptr<Request>);
    void commit(const std::shared_ptr<Request> &);
    void fail_requests(ControllerHandle, ControllerError);
    void release_reservation(const Request &);
    void flush();
    bool _initialized = false, _session = false, _flushing = false;
    std::uint64_t _runtime = 1, _next_id = 1, _dispatch = 0;
    unsigned _depth = 0;
    std::map<std::uint64_t, Entry> _entries;
    std::map<std::uint64_t, SceneControlContext *> _contexts;
    std::deque<std::shared_ptr<Request>> _pending;
    std::vector<std::shared_ptr<Request>> _requests;
};
} // namespace elysia::gameplay
