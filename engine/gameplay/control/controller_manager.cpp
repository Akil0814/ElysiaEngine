#include "controller_manager.h"
#include "../scene/gameplay_scene.h"
#include "../../tools/logger.h"
#include <algorithm>
#include <cmath>
namespace elysia::gameplay
{
using namespace elysia::input;
void ControllerManager::initialize()
{
    if (!_initialized)
    {
        _initialized = true;
        ++_runtime;
    }
}
void ControllerManager::shutdown()
{
    end_session();
    _initialized = false;
    ++_runtime;
}
std::expected<void, ControllerError> ControllerManager::begin_session()
{
    if (!_initialized)
        return std::unexpected(ControllerError::NotInitialized);
    if (_session)
        return std::unexpected(ControllerError::SessionAlreadyActive);
    ++_runtime;
    _session = true;
    return {};
}
void ControllerManager::end_session()
{
    Boundary boundary(*this);
    _session = false;
    std::vector<ControllerHandle> handles;
    for (auto &[id, e] : _entries)
        if (!e.removed)
            handles.push_back(e.command.controller);
    for (auto h : handles)
        remove(h);
}
ControllerManager::Entry *ControllerManager::find(ControllerHandle h)
{
    auto it = _entries.find(h.instance);
    return it != _entries.end() && !it->second.removed && it->second.command.controller == h ? &it->second
                                                                                             : nullptr;
}
SceneControlContext *ControllerManager::context(SceneControlToken token)
{
    auto it = _contexts.find(token.instance);
    return it != _contexts.end() && it->second->token() == token && !it->second->_retiring ? it->second
                                                                                           : nullptr;
}
std::expected<ControllerHandle, ControllerError> ControllerManager::add(
    ControllerCreateInfo info, std::unique_ptr<Controller> controller)
{
    if (!_session)
        return std::unexpected(ControllerError::NoSession);
    if (info.scope == ControllerScope::Scene && !context(info.scene))
        return std::unexpected(ControllerError::InvalidContext);
    if (auto *local = dynamic_cast<LocalPlayerController *>(controller.get());
        local && (!local->player().value || !local->input_map().valid()))
        return std::unexpected(ControllerError::InvalidMap);
    auto h = ControllerHandle{_runtime, _next_id++};
    controller->_handle = h;
    controller->_submit = [this, h](ActionInputResult input) {
        if (auto *e = find(h))
            ingest(*e, std::move(input));
    };
    Entry e;
    e.scope = info.scope;
    e.owner = info.scope == ControllerScope::Scene ? info.scene : SceneControlToken{};
    e.controller = std::move(controller);
    e.command.controller = h;
    e.eligible_dispatch = _dispatch + 1;
    _entries.emplace(h.instance, std::move(e));
    return h;
}
void ControllerManager::flush()
{
    if (_flushing)
        return;
    _flushing = true;
    struct Guard
    {
        bool &flag;
        ~Guard()
        {
            flag = false;
        }
    } guard{_flushing};
    while (!_pending.empty())
    {
        auto operation = std::move(_pending.front());
        _pending.pop_front();
        operation();
    }
    std::erase_if(_entries, [](auto &item) { return item.second.removed; });
}
void ControllerManager::cancel(Entry &e, InputCancelReason reason)
{
    e.command.state = {};
    e.command.events.clear();
    e.command.deltas.clear();
    if (e.cancelling)
        return;
    e.cancelling = true;
    struct Guard
    {
        bool &flag;
        ~Guard()
        {
            flag = false;
        }
    } guard{e.cancelling};
    if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
        local->_map.reset_state();
    e.controller->cancelled(reason);
    if (e.receiver)
        e.receiver->on_control_cancelled(reason);
}
void ControllerManager::detach(Entry &e, InputCancelReason reason)
{
    if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
        if (auto it = _contexts.find(e.bound_scene.instance);
            it != _contexts.end() && it->second->token() == e.bound_scene)
            it->second->_scene.local_players().clear_keyboard_requirement(local->player());
    // Detach before callbacks so reentrant operations cannot deliver to this target.
    auto *receiver = std::exchange(e.receiver, nullptr);
    e.target = nullptr;
    e.bound_scene = {};
    e.available = false;
    ++e.command.binding_generation;
    e.reserved_target = nullptr;
    e.reserved_scene = {};
    ++e.request_generation;
    cancel(e, reason);
    if (receiver)
        receiver->on_control_cancelled(reason);
}
bool ControllerManager::remove(ControllerHandle h)
{
    auto *e = find(h);
    if (!e)
        return false;
    Boundary boundary(*this);
    e->removed = true;
    detach(*e, InputCancelReason::Unbound);
    return true;
}
bool ControllerManager::unbind(ControllerHandle h)
{
    auto *e = find(h);
    if (!e)
        return false;
    if (!e->target && !e->reserved_target)
        return true;
    if (_depth)
    {
        auto request = ++e->request_generation;
        e->reserved_target = nullptr;
        e->reserved_scene = {};
        _pending.push_back([this, h, request] {
            if (auto *current = find(h); current && current->request_generation == request)
                unbind(h);
        });
        return true;
    }
    Boundary boundary(*this);
    detach(*e, InputCancelReason::Unbound);
    return true;
}
std::expected<void, ControllerError> ControllerManager::validate_binding(Entry &e, SceneControlToken token,
                                                                         elysia::core::GameObject &target)
{
    auto *ctx = context(token);
    if (!ctx || !ctx->_active || (e.scope == ControllerScope::Scene && e.owner != token))
        return std::unexpected(ControllerError::InvalidContext);
    if (!ctx->_scene.owns_object(target) || target.is_destroyed() ||
        !dynamic_cast<ControlCommandReceiver *>(&target))
        return std::unexpected(ControllerError::InvalidTarget);
    auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get());
    if (local && !ctx->_scene.local_players().contains(local->player()))
        return std::unexpected(ControllerError::InvalidPlayer);
    if (local &&
        !ctx->_scene.local_players().accepts_keyboard_map(local->player(), local->_map.keyboard_controls()))
        return std::unexpected(ControllerError::InvalidMap);
    for (auto &[id, other] : _entries)
    {
        if (&e == &other || other.removed || (!other.target && !other.reserved_target))
            continue;
        if (other.target == &target || other.reserved_target == &target)
            return std::unexpected(ControllerError::TargetBusy);
        auto *other_local = dynamic_cast<LocalPlayerController *>(other.controller.get());
        if (local && other_local && (other.bound_scene == token || other.reserved_scene == token) &&
            local->player() == other_local->player())
            return std::unexpected(ControllerError::PlayerBusy);
    }
    return {};
}
std::expected<void, ControllerError> ControllerManager::bind(ControllerHandle h, SceneControlToken token,
                                                             elysia::core::GameObject &target)
{
    auto *e = find(h);
    if (!e)
        return std::unexpected(ControllerError::InvalidHandle);
    auto result = validate_binding(*e, token, target);
    if (!result)
        return result;
    if (_depth)
    {
        auto *ptr = &target;
        auto request = ++e->request_generation;
        e->reserved_target = ptr;
        e->reserved_scene = token;
        _pending.push_back([this, h, token, ptr, request] {
            // Membership is checked without dereferencing a potentially released target.
            auto *current = find(h);
            if (!current || current->request_generation != request)
                return;
            current->reserved_target = nullptr;
            current->reserved_scene = {};
            auto *ctx = context(token);
            if (ctx && ctx->_scene.contains_control_target(ptr))
                (void)bind(h, token, *ptr);
        });
        return {};
    }
    Boundary boundary(*this);
    if (e->target == &target && e->bound_scene == token)
        return {};
    detach(*e, InputCancelReason::Unbound);
    if (e->removed || !context(token))
        return std::unexpected(ControllerError::InvalidHandle);
    e->target = &target;
    e->receiver = dynamic_cast<ControlCommandReceiver *>(&target);
    e->bound_scene = token;
    e->available = target.is_active();
    if (auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get()))
    {
        auto &scene = context(token)->_scene;
        scene.local_players().set_keyboard_requirement(local->player(), local->_map.keyboard_controls());
        local->_sources = scene.local_players().sources(local->player());
        local->_binding_version = scene.local_players().binding_version(local->player());
        scene.input_router().suppress(local->player());
    }
    return {};
}
void ControllerManager::ingest(Entry &e, ActionInputResult input)
{
    if (!e.target || !e.available || e.removed)
        return;
    auto *ctx = context(e.bound_scene);
    if (!ctx || !ctx->_active || ctx->_scene._paused || !e.target->is_active() || e.target->is_destroyed())
        return;
    bool finite = input.frame.finite();
    for (auto &[id, v] : input.deltas)
        finite = finite && std::isfinite(v.x) && std::isfinite(v.y);
    for (auto &event : input.events)
        finite = finite && std::isfinite(event.value.x) && std::isfinite(event.value.y) &&
                 std::isfinite(event.previous_value.x) && std::isfinite(event.previous_value.y);
    if (!finite || e.command.events.size() + input.events.size() > 1024)
    {
        cancel(e, InputCancelReason::Overflow);
        if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get()))
            ctx->_scene.input_router().suppress(local->player());
        ELYSIA_LOG_ERROR("input", "Controller input rejected: non-finite value or event queue overflow.");
        return;
    }
    e.command.state = std::move(input.frame);
    e.command.events.insert(e.command.events.end(), input.events.begin(), input.events.end());
    for (auto &[id, v] : input.deltas)
    {
        auto &sum = e.command.deltas[id];
        sum.x += v.x;
        sum.y += v.y;
        if (!std::isfinite(sum.x) || !std::isfinite(sum.y))
        {
            cancel(e, InputCancelReason::Overflow);
            return;
        }
    }
}
void ControllerManager::input(SceneControlContext &ctx, const InputSnapshot &input)
{
    Boundary boundary(*this);
    std::vector<ControllerHandle> handles;
    for (auto &[id, e] : _entries)
        if (!e.removed && e.bound_scene == ctx.token())
            handles.push_back(e.command.controller);
    for (auto h : handles)
    {
        auto *e = find(h);
        if (!e)
            continue;
        auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get());
        if (!local)
            continue;
        auto &players = ctx._scene.local_players();
        auto player = local->player();
        bool available = ctx._active && !ctx._scene._paused && e->target && e->target->is_active() &&
                         !e->target->is_destroyed() && players.contains(player);
        if (!available || !e->available)
        {
            e->available = available;
            cancel(*e, InputCancelReason::Unavailable);
            ctx._scene.input_router().suppress(player);
            continue;
        }
        auto sources = players.sources(player);
        if (sources != local->_sources || players.binding_version(player) != local->_binding_version)
        {
            bool added = sources == local->_sources || std::ranges::any_of(sources, [&](auto source) {
                             return std::ranges::find(local->_sources, source) == local->_sources.end();
                         });
            local->_sources = sources;
            local->_binding_version = players.binding_version(player);
            cancel(*e, InputCancelReason::SourceChanged);
            if (added)
            {
                ctx._scene.input_router().suppress(player);
                continue;
            }
        }
        // Consumption also discards matching deltas queued during earlier zero-tick frames.
        // Do not discard unrelated buttons or wheel/motion actions from the same player.
        std::erase_if(e->command.deltas, [&](const auto &pending) {
            for (const auto &operation : ctx._scene.input_router().consumed_operations())
            {
                if (!players.owns(player, operation))
                    continue;
                for (const auto &binding : local->_map.bindings(pending.first))
                {
                    const auto *delta = std::get_if<PointerDeltaBinding>(&binding.source);
                    if (!delta)
                        continue;
                    const bool motion =
                        delta->axis == PointerDeltaAxis::MouseX || delta->axis == PointerDeltaAxis::MouseY;
                    if ((motion && operation.type == RawInputEventType::MouseMoved) ||
                        (!motion && operation.type == RawInputEventType::MouseWheel))
                        return true;
                }
            }
            return false;
        });
        std::erase_if(e->command.events, [&](const auto &pending) {
            for (const auto &operation : ctx._scene.input_router().consumed_operations())
            {
                if (!players.owns(player, operation))
                    continue;
                for (const auto &binding : local->_map.bindings(pending.action))
                {
                    if (auto *button = std::get_if<ButtonInputBinding>(&binding.source);
                        button && button->control == operation.control &&
                        operation.control != RawInputControl::None)
                        return true;
                    if (auto *buttons = std::get_if<Button2DInputBinding>(&binding.source);
                        buttons && operation.control != RawInputControl::None)
                        for (auto key : {buttons->left, buttons->right, buttons->up, buttons->down})
                            if (key == operation.control)
                                return true;
                    if (auto *axis = std::get_if<AxisInputBinding>(&binding.source);
                        axis && axis->axis == operation.axis && operation.axis != RawInputAxis::None)
                        return true;
                    if (auto *axes = std::get_if<Axis2DInputBinding>(&binding.source);
                        axes && operation.axis != RawInputAxis::None &&
                        (axes->x_axis == operation.axis || axes->y_axis == operation.axis))
                        return true;
                }
            }
            return false;
        });
        auto filtered = players.for_player(player, input);
        local->process_input(filtered);
    }
}
void ControllerManager::cancel_local(SceneControlContext &ctx, LocalPlayerId player, InputCancelReason reason)
{
    Boundary boundary(*this);
    for (auto &[id, e] : _entries)
        if (!e.removed && e.bound_scene == ctx.token())
            if (auto *local = dynamic_cast<LocalPlayerController *>(e.controller.get());
                local && local->player() == player)
                cancel(e, reason);
}
void ControllerManager::pause(SceneControlContext &ctx)
{
    Boundary boundary(*this);
    for (auto &[id, e] : _entries)
        if (!e.removed && e.bound_scene == ctx.token())
            cancel(e, InputCancelReason::Paused);
}
void ControllerManager::leave(SceneControlContext &ctx, bool release)
{
    Boundary boundary(*this);
    for (auto &[id, e] : _entries)
    {
        if (e.removed)
            continue;
        if (release && e.scope == ControllerScope::Scene && e.owner == ctx.token())
        {
            e.removed = true;
            detach(e, InputCancelReason::Unbound);
        }
        else if (e.bound_scene == ctx.token())
            detach(e, InputCancelReason::Unbound);
    }
}
void ControllerManager::object_removed(SceneControlContext &ctx, elysia::core::GameObject &target)
{
    Boundary boundary(*this);
    for (auto &[id, e] : _entries)
        if (!e.removed && e.bound_scene == ctx.token() && e.target == &target)
            detach(e, InputCancelReason::Unbound);
}
void ControllerManager::advance(SceneControlContext &ctx, std::uint64_t tick, double delta)
{
    if (!ctx._active || ctx._scene._paused)
        return;
    Boundary boundary(*this);
    ++_dispatch;
    std::vector<ControllerHandle> handles;
    for (auto &[id, e] : _entries)
        if (!e.removed && e.bound_scene == ctx.token() && e.eligible_dispatch <= _dispatch)
            handles.push_back(e.command.controller);
    for (auto h : handles)
    {
        if (!ctx._active || ctx._scene._paused)
            break;
        auto *e = find(h);
        if (!e)
            continue;
        if (!e->target || e->target->is_destroyed() || !e->target->is_active())
        {
            e->available = false;
            cancel(*e, InputCancelReason::Unavailable);
            continue;
        }
        if (!e->available && dynamic_cast<LocalPlayerController *>(e->controller.get()))
            continue;
        if (auto *local = dynamic_cast<LocalPlayerController *>(e->controller.get()))
        {
            auto &players = ctx._scene.local_players();
            auto sources = players.sources(local->player());
            if (!players.contains(local->player()) || sources != local->_sources ||
                players.binding_version(local->player()) != local->_binding_version)
            {
                local->_sources = std::move(sources);
                local->_binding_version = players.binding_version(local->player());
                cancel(*e, InputCancelReason::SourceChanged);
                ctx._scene.input_router().suppress(local->player());
                continue;
            }
        }
        e->available = true;
        e->controller->produce_intent(tick, delta);
        if (!ctx._active || ctx._scene._paused || e->removed || !e->receiver || !e->target->is_active() ||
            e->target->is_destroyed())
            continue;
        e->command.tick = tick;
        ++e->command.sequence;
        auto command = e->command;
        e->command.events.clear();
        e->command.deltas.clear();
        e->command.state.clear_edges();
        e->receiver->on_control_command(command, delta);
    }
}
std::expected<void, ControllerError> ControllerManager::replace_map(ControllerHandle h, InputActionMap map)
{
    auto *e = find(h);
    if (!e)
        return std::unexpected(ControllerError::InvalidHandle);
    if (!map.valid() || !dynamic_cast<LocalPlayerController *>(e->controller.get()))
        return std::unexpected(ControllerError::InvalidMap);
    if (auto *ctx = context(e->bound_scene))
        if (!ctx->_scene.local_players().accepts_keyboard_map(
                static_cast<LocalPlayerController *>(e->controller.get())->player(), map.keyboard_controls()))
            return std::unexpected(ControllerError::InvalidMap);
    if (_depth)
    {
        _pending.push_back(
            [this, h, map = std::move(map)]() mutable { (void)replace_map(h, std::move(map)); });
        return {};
    }
    Boundary boundary(*this);
    cancel(*e, InputCancelReason::SourceChanged);
    auto *local = static_cast<LocalPlayerController *>(e->controller.get());
    map.reset_state();
    local->_map = std::move(map);
    if (auto *ctx = context(e->bound_scene))
    {
        ctx->_scene.local_players().set_keyboard_requirement(local->player(),
                                                             local->_map.keyboard_controls());
        ctx->_scene.input_router().suppress(local->player());
    }
    return {};
}
SceneControlContext::SceneControlContext(GameplayScene &scene) : _scene(scene)
{
    static std::uint64_t next = 1;
    _instance = next++;
    ControllerManager::instance()->_contexts.emplace(_instance, this);
}
SceneControlContext::~SceneControlContext()
{
    reset();
    ControllerManager::instance()->_contexts.erase(_instance);
}
void SceneControlContext::activate()
{
    _active = true;
}
void SceneControlContext::deactivate()
{
    _active = false;
    ControllerManager::instance()->leave(*this, false);
}
void SceneControlContext::reset()
{
    _active = false;
    _retiring = true;
    ControllerManager::instance()->leave(*this, true);
    ++_generation;
    _retiring = false;
}
} // namespace elysia::gameplay
