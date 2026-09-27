#include "scene_manager.h"
#include "detail/scene_failure_log.h"

#include "../application/lifecycle/shutdown_boundary.h"
#include "../camera/camera_manager.h"
#include "../effects/runtime/effect_manager.h"
#include "../gameplay/control/controller_manager.h"
#include "../object_query/runtime/game_object_query_manager.h"
#include "../tools/debug_draw.h"
#include "../tools/logger.h"

#include <exception>
#include <stdexcept>
#include <utility>

namespace elysia::scene
{
SceneManager::~SceneManager()
{
    (void)shutdown();
}

void SceneManager::initialize(
    const SceneRuntimeContext& context,
    SceneFailureRouteFactory failure_route_factory)
{
    if (_state != SceneManagerState::Constructed && _state != SceneManagerState::Stopped)
        throw std::logic_error("SceneManager::initialize requires a constructed or stopped manager.");

    elysia::gameplay::ControllerManager::instance()->initialize();
    _runtime_context = &context;
    _failure_route_factory = std::move(failure_route_factory);
    _shutdown_succeeded = true;
    _shutdown_performed = false;
    _recovering_failure = false;
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = false;

    const elysia::core::Vector2 viewport_size{
        static_cast<float>(context.logical_width()),
        static_cast<float>(context.logical_height())};
    auto* cameras = elysia::camera::CameraManager::instance();
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(elysia::camera::CameraSlot::Count);
         ++index)
    {
        cameras->set_viewport_size(
            static_cast<elysia::camera::CameraSlot>(index), viewport_size);
    }
    _state = SceneManagerState::Ready;
}

void SceneManager::ensure_ready_for_registration() const
{
    if (_state != SceneManagerState::Ready)
        throw std::logic_error("Scene registration is only allowed while SceneManager is ready.");
}

void SceneManager::start(const SceneRoute& route)
{
    if (_state != SceneManagerState::Ready)
        throw std::logic_error("SceneManager::start requires the ready state.");
    if (_current_scene)
        throw std::logic_error("SceneManager::start called while a scene is already active.");

    _state = SceneManagerState::Running;
    try
    {
        if (auto result = switch_to_registered_scene(route); !result)
            recover_from_failure(result.error());
    }
    catch (...)
    {
        _state = SceneManagerState::Ready;
        throw;
    }
}

void SceneManager::on_input(const elysia::input::InputSnapshot& input)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        _current_scene->lifecycle_input(input);
    }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Input));
        return;
    }
    process_pending_request();
}

void SceneManager::on_update(double delta)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        _current_scene->lifecycle_update(delta);
    }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Update));
        return;
    }
    process_pending_request();
}

void SceneManager::on_render(SDL_Renderer* renderer)
{
    if (_state != SceneManagerState::Running || !_current_scene)
        return;
    try
    {
        _current_scene->lifecycle_render(renderer);
    }
    catch (...)
    {
        recover_from_failure(make_failure(_current_scene_key, SceneBoundary::Render));
    }
}

void SceneManager::on_scene_request(const SceneRequest& request)
{
    if (_state != SceneManagerState::Running)
        throw std::logic_error("Scene requests require a running SceneManager.");
    if (_is_processing_request)
        throw std::logic_error("SceneManager received a reentrant scene request.");
    if (_has_pending_request)
        throw std::logic_error("SceneManager received multiple scene requests in one processing cycle.");
    _pending_request = request;
    _has_pending_request = true;
}

void SceneManager::notify_quit_requested()
{
    notify_observers([](SceneManagerObserver& observer) {
        observer.on_scene_manager_quit_requested();
    });
}

void SceneManager::notify_fault(const SceneBoundaryFailure& failure) noexcept
{
    _state = SceneManagerState::Faulted;
    try
    {
        notify_observers([&](SceneManagerObserver& observer) {
            observer.on_scene_manager_fault(failure);
        });
    }
    catch (...)
    {
    }
}

void SceneManager::process_pending_request()
{
    if (!_has_pending_request || _state != SceneManagerState::Running)
        return;

    const SceneRequest request = _pending_request;
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = true;
    struct Guard
    {
        bool& flag;
        ~Guard() { flag = false; }
    } guard{_is_processing_request};

    switch (request.type)
    {
    case SceneRequestType::Switch:
        if (auto result = switch_to_registered_scene(request.route); !result)
            recover_from_failure(result.error());
        break;
    case SceneRequestType::Quit:
        notify_quit_requested();
        break;
    case SceneRequestType::None:
    default:
        break;
    }
}

std::expected<void, SceneBoundaryFailure>
SceneManager::switch_to_registered_scene(const SceneRoute& route)
{
    if (!SceneKeys::is_supported(route.target))
        throw_invalid_route_key(route.target);
    const auto builder = _scene_builders.find(route.target);
    if (builder == _scene_builders.end())
    {
        if (SceneKeys::is_game(route.target))
            throw std::logic_error("SceneManager received a valid but unregistered game SceneKey.");
        throw std::logic_error("SceneManager received a valid but unregistered engine-owned SceneKey.");
    }

    ELYSIA_LOG("scene", "Switching to SceneKey " << route.target
        << " with reload mode " << static_cast<int>(route.reload_mode));

    Scene* next_scene = nullptr;
    std::unique_ptr<Scene> staged_scene;
    if (route.reload_mode != SceneReloadMode::Recreate)
        next_scene = _scene_factory.find(route.target);
    if (!next_scene)
    {
        try
        {
            staged_scene = builder->second();
            next_scene = staged_scene.get();
        }
        catch (...)
        {
            return std::unexpected(make_failure(route.target, SceneBoundary::Enter));
        }
    }
    if (!next_scene)
        return std::unexpected(SceneBoundaryFailure{route.target, SceneBoundary::Enter,
                                    "Scene builder returned a null scene."});

    return switch_to_scene(next_scene, std::move(staged_scene), route);
}

std::expected<void, SceneBoundaryFailure> SceneManager::leave_current_scene()
{
    if (!_current_scene)
        return {};

    Scene* leaving = _current_scene;
    const SceneKey leaving_key = _current_scene_key;
    std::optional<SceneBoundaryFailure> first_failure;
    try
    {
        detach_from_scene(*leaving);
    }
    catch (...)
    {
        first_failure = make_failure(leaving_key, SceneBoundary::Detach);
    }
    try
    {
        leaving->lifecycle_exit();
    }
    catch (...)
    {
        if (!first_failure)
            first_failure = make_failure(leaving_key, SceneBoundary::Exit);
        else
            detail::log_cleanup_exception("Exit after detach failure");
    }
    _current_scene = nullptr;
    _current_scene_key = SceneKeys::Invalid;
    if (first_failure)
        return std::unexpected(std::move(*first_failure));
    return {};
}

std::expected<void, SceneBoundaryFailure> SceneManager::switch_to_scene(
    Scene* next_scene,
    std::unique_ptr<Scene> staged_scene,
    const SceneRoute& route)
{
    if (!next_scene)
        return std::unexpected(SceneBoundaryFailure{route.target, SceneBoundary::Enter,
                                    "SceneManager received a null candidate scene."});

    Scene* old_scene = _current_scene;
    const SceneKey old_key = _current_scene_key;
    if (auto result = leave_current_scene(); !result)
    {
        discard_scene(old_key, old_scene);
        if (staged_scene)
        {
            try { staged_scene->prepare_for_destruction(); }
            catch (...) { detail::log_cleanup_exception("Discard staged scene after leave failure"); }
        }
        return result;
    }

    elysia::tools::DebugDraw::instance()->clear();
    if (old_scene != next_scene || route.reload_mode != SceneReloadMode::Reuse)
        elysia::camera::CameraManager::instance()->reset(next_scene->render_camera_slot());

    if (route.reload_mode == SceneReloadMode::Recreate)
    {
        try
        {
            (void)_scene_factory.destroy(route.target);
        }
        catch (...)
        {
            if (staged_scene)
            {
                try { staged_scene->prepare_for_destruction(); }
                catch (...) { detail::log_cleanup_exception("Discard staged scene after recreate failure"); }
            }
            return std::unexpected(make_failure(route.target, SceneBoundary::ObjectRemoval));
        }
    }

    next_scene->set_local_players(_local_players);
    next_scene->set_ui_device_access(_ui_device_access);
    if (_runtime_context)
        next_scene->bind_runtime_context(*_runtime_context);

    auto fail_candidate = [&](SceneBoundary boundary) -> std::expected<void, SceneBoundaryFailure> {
        const auto failure = make_failure(route.target, boundary);
        try { detach_from_scene(*next_scene); }
        catch (...) { detail::log_cleanup_exception("Candidate detach"); }
        if (next_scene->lifecycle_state() == SceneLifecycleState::Active)
        {
            try { next_scene->lifecycle_exit(); }
            catch (...) { detail::log_cleanup_exception("Candidate exit"); }
        }
        if (staged_scene)
        {
            try { staged_scene->prepare_for_destruction(); }
            catch (...) { detail::log_cleanup_exception("Candidate destruction"); }
        }
        else
        {
            discard_scene(route.target, next_scene);
        }
        return std::unexpected(failure);
    };

    if (route.reload_mode == SceneReloadMode::Reset)
    {
        try
        {
            next_scene->lifecycle_reset();
        }
        catch (...)
        {
            return fail_candidate(SceneBoundary::Reset);
        }
    }

    try
    {
        attach_to_scene(*next_scene);
    }
    catch (...)
    {
        return fail_candidate(SceneBoundary::Attach);
    }

    try
    {
        next_scene->lifecycle_enter(route.payload);
    }
    catch (...)
    {
        return fail_candidate(SceneBoundary::Enter);
    }

    if (staged_scene)
    {
        try
        {
            _scene_factory.store(route.target, staged_scene);
        }
        catch (...)
        {
            return fail_candidate(SceneBoundary::Attach);
        }
    }
    _current_scene = next_scene;
    _current_scene_key = route.target;
    return {};
}

void SceneManager::attach_to_scene(Scene& scene)
{
    bool effect_bound = false;
    bool query_bound = false;
    bool runtime_bound = false;
    bool observer_bound = false;
    try
    {
        elysia::effects::EffectManager::instance()->bind_active_scene(scene);
        effect_bound = true;
        elysia::object_query::GameObjectQueryManager::instance()->bind_active_runtime(scene);
        query_bound = true;
        scene.attach_runtime_services();
        runtime_bound = true;
        scene.attach(this);
        observer_bound = true;
    }
    catch (...)
    {
        if (observer_bound)
            scene.detach(this);
        if (runtime_bound)
        {
            try { scene.detach_runtime_services(); }
            catch (...) { detail::log_cleanup_exception("Attach rollback runtime detach"); }
        }
        if (query_bound)
            elysia::object_query::GameObjectQueryManager::instance()->unbind_active_runtime(scene);
        if (effect_bound)
            elysia::effects::EffectManager::instance()->unbind_active_scene(scene);
        throw;
    }
}

void SceneManager::detach_from_scene(Scene& scene)
{
    std::exception_ptr failure;
    scene.detach(this);
    try
    {
        scene.reset_input_routing();
    }
    catch (...)
    {
        failure = std::current_exception();
    }
    try
    {
        scene.detach_runtime_services();
    }
    catch (...)
    {
        if (!failure)
            failure = std::current_exception();
        else
            detail::log_cleanup_exception("Runtime detach after input reset failure");
    }
    elysia::object_query::GameObjectQueryManager::instance()->unbind_active_runtime(scene);
    elysia::effects::EffectManager::instance()->unbind_active_scene(scene);
    if (failure)
        std::rethrow_exception(failure);
}

void SceneManager::discard_scene(SceneKey key, Scene* expected) noexcept
{
    if (key == SceneKeys::Invalid)
        return;
    Scene* cached = _scene_factory.find(key);
    if (!cached || (expected && cached != expected))
        return;
    try
    {
        (void)_scene_factory.destroy(key);
    }
    catch (...)
    {
        detail::log_cleanup_exception("Discard cached scene");
    }
}

SceneBoundaryFailure SceneManager::make_failure(SceneKey key, SceneBoundary boundary) const
{
    std::string message = "Unknown scene boundary exception.";
    try
    {
        const auto exception = std::current_exception();
        if (exception)
            std::rethrow_exception(exception);
    }
    catch (const SceneBoundaryTagged& error)
    {
        boundary = error.scene_boundary();
        if (const auto* exception = dynamic_cast<const std::exception*>(&error))
            message = exception->what();
    }
    catch (const std::exception& error)
    {
        message = error.what();
    }
    catch (...)
    {
    }
    return SceneBoundaryFailure{key, boundary, std::move(message)};
}

void SceneManager::recover_from_failure(const SceneBoundaryFailure& failure) noexcept
{
    detail::log_scene_failure(failure);
    _pending_request = {};
    _has_pending_request = false;
    if (_recovering_failure || !_failure_route_factory)
    {
        notify_fault(failure);
        return;
    }

    _recovering_failure = true;
    try
    {
        SceneRoute route = _failure_route_factory(failure);
        if (route.target == failure.scene)
        {
            notify_fault(failure);
            _recovering_failure = false;
            return;
        }
        route.reload_mode = SceneReloadMode::Recreate;
        if (auto result = switch_to_registered_scene(route); !result)
        {
            detail::log_scene_failure(result.error());
            notify_fault(result.error());
        }
        else
            discard_scene(failure.scene, nullptr);
    }
    catch (...)
    {
        const auto recovery_failure = make_failure(failure.scene, SceneBoundary::Enter);
        detail::log_scene_failure(recovery_failure);
        notify_fault(recovery_failure);
    }
    _recovering_failure = false;
}

void SceneManager::throw_invalid_route_key(SceneKey key)
{
    if (key == SceneKeys::Invalid)
        throw std::logic_error("SceneManager received SceneKeys::Invalid.");
    throw std::logic_error("SceneManager received a SceneKey in the reserved range.");
}

bool SceneManager::shutdown() noexcept
{
    if (_shutdown_performed)
        return _shutdown_succeeded;
    _shutdown_performed = true;
    const bool was_faulted = _state == SceneManagerState::Faulted;
    _state = SceneManagerState::ShuttingDown;

    auto cleanup = [this](auto&& action) {
        if (!elysia::application::run_shutdown_boundary("scene_shutdown", action))
            _shutdown_succeeded = false;
    };

    if (_current_scene)
    {
        Scene* exiting = _current_scene;
        const SceneKey exiting_key = _current_scene_key;
        cleanup([&] { detach_from_scene(*exiting); });
        cleanup([&] { exiting->lifecycle_exit(); });
        _current_scene = nullptr;
        _current_scene_key = SceneKeys::Invalid;
        (void)exiting_key;
    }
    cleanup([] { elysia::tools::DebugDraw::instance()->clear(); });
    cleanup([] { elysia::camera::CameraManager::instance()->reset_all(); });
    cleanup([&] {
        if (!_scene_factory.destroy_all_scene())
            throw std::runtime_error("One or more scenes failed to prepare for destruction.");
    });
    cleanup([] { elysia::gameplay::ControllerManager::instance()->shutdown(); });

    _scene_builders.clear();
    _local_players.reset();
    _ui_device_access = {};
    _runtime_context = nullptr;
    _failure_route_factory = {};
    _pending_request = {};
    _has_pending_request = false;
    _is_processing_request = false;
    _recovering_failure = false;

    if (was_faulted || !_shutdown_succeeded)
        _state = SceneManagerState::Faulted;
    else
        _state = SceneManagerState::Stopped;
    return _shutdown_succeeded;
}
} // namespace elysia::scene
