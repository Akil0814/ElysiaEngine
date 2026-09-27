#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "routing/scene_request.h"
#include "routing/scene_request_observer.h"
#include "runtime/fixed_step_runtime.h"
#include "runtime/scene_runtime_context.h"
#include "runtime/scene_runtime_features.h"
#include "scene_boundary_failure.h"

#include "../camera/camera_manager.h"
#include "../core/depth_layer.h"
#include "../core/event/subject.h"
#include "../core/game_object.h"
#include "../core/interface/updatable.h"
#include "../input/local_player_registry.h"
#include "../input/raw_input_frame.h"
#include "../input/raw_input_types.h"
#include "../input/scene_input_router.h"
#include "../object_query/runtime/game_object_query_runtime.h"
#include "../physics/physics_world.h"
#include "../ui/core/ui_element.h"
#include "../ui/input/contracts/ui_input_event_receiver.h"
#include "../ui/input/contracts/ui_input_frame_receiver.h"
#include "../ui/input/ui_input_router.h"

namespace elysia::scene
{
class SceneFactory;
class SceneManager;
class SceneTestAccess;

enum class SceneLifecycleState
{
    Inactive,
    Entering,
    Active,
    Exiting,
    Resetting,
    PreparingDestruction,
    PreparedForDestruction
};

class Scene : public elysia::core::Subject<SceneRequestObserver>,
              public elysia::object_query::IGameObjectQueryRuntime
{
public:
    Scene();
    explicit Scene(SceneRuntimeFeatures features);
    ~Scene() override;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    void set_ui_gamepad(elysia::input::InputSourceId source) { _input_router.set_ui_gamepad(source); }
    [[nodiscard]] elysia::input::InputSourceId ui_gamepad() const { return _input_router.ui_gamepad(); }
    void set_ui_interaction_mode(elysia::input::UiInteractionMode mode)
    {
        _input_router.set_ui_interaction_mode(mode);
    }
    void set_shortcut_devices(elysia::input::InputCapture devices)
    {
        _input_router.set_shortcut_devices(devices);
    }
    void set_all_gameplay_input_blocked(bool blocked);
    void consume_input(const elysia::input::RawInputEvent& event);

    void pause();
    void resume();

    [[nodiscard]] SceneLifecycleState lifecycle_state() const noexcept { return _lifecycle_state; }
    [[nodiscard]] bool has_fixed_step() const noexcept { return static_cast<bool>(_fixed_step); }
    [[nodiscard]] bool has_physics() const noexcept { return static_cast<bool>(_physics_world); }
    [[nodiscard]] const FixedStepStats* fixed_step_stats() const noexcept
    {
        return _fixed_step ? &_fixed_step->stats() : nullptr;
    }
    [[nodiscard]] const FixedStepConfig* fixed_step_config() const noexcept
    {
        return _fixed_step ? &_fixed_step->config() : nullptr;
    }
    [[nodiscard]] const elysia::camera::Camera& camera() const noexcept;
    [[nodiscard]] elysia::camera::CameraSlot render_camera_slot() const noexcept;
    [[nodiscard]] elysia::input::LocalPlayerRegistry& local_players() noexcept
    {
        return *_players;
    }

    template <typename T, typename... Args>
    T* create_and_add_object(Args&&... args)
    {
        static_assert(std::is_base_of_v<elysia::core::GameObject, T> ||
                          std::is_base_of_v<elysia::ui::UiElement, T>,
                      "T must derive from GameObject or UiElement.");
        return add_object(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <typename T>
    T* add_object(std::unique_ptr<T> object)
    {
        static_assert(std::is_base_of_v<elysia::core::SceneObject, T>);
        static_assert(std::is_base_of_v<elysia::core::GameObject, T> ||
                      std::is_base_of_v<elysia::ui::UiElement, T>);

        if (!object)
            return nullptr;
        if (_retiring_objects || _lifecycle_state == SceneLifecycleState::PreparingDestruction ||
            _lifecycle_state == SceneLifecycleState::PreparedForDestruction)
        {
            throw std::logic_error("Scene objects cannot be added while the scene is tearing down.");
        }

        T* raw_object = object.get();
        bool added = false;
        if constexpr (std::is_base_of_v<elysia::core::GameObject, T>)
            added = add_game_object(std::move(object));
        else
            added = add_ui_root(std::move(object));

        if (!added)
            return nullptr;

        try
        {
            register_scene_object_interfaces(raw_object);
        }
        catch (...)
        {
            rollback_owned_object(raw_object);
            try
            {
                throw;
            }
            catch (const SceneBoundaryTagged&)
            {
                throw;
            }
            catch (const std::logic_error& error)
            {
                throw SceneBoundaryLogicError(
                    SceneBoundary::ObjectRegistration, error.what());
            }
            catch (const std::exception& error)
            {
                throw SceneBoundaryRuntimeError(
                    SceneBoundary::ObjectRegistration, error.what());
            }
            catch (...)
            {
                throw SceneBoundaryRuntimeError(
                    SceneBoundary::ObjectRegistration,
                    "Unknown scene object registration exception.");
            }
        }
        return raw_object;
    }

protected:
    virtual void on_enter(const ScenePayload& payload) = 0;
    virtual void on_exit() = 0;
    virtual void on_reset() = 0;
    virtual void on_before_update(double) {}
    virtual void on_after_update(double) {}

    virtual void on_shortcuts(const elysia::input::RawInputFrame&,
                              const std::vector<elysia::input::RawInputEvent>&)
    {}
    virtual bool on_unassigned_input(const elysia::input::RawInputEvent&) { return false; }
    virtual void on_routed_input(const elysia::input::InputSnapshot&) {}
    virtual void on_pause_changed(bool) {}
    virtual void on_fixed_update(std::uint64_t, double) {}
    virtual void on_scene_object_registered(elysia::core::SceneObject&) {}
    virtual void on_scene_object_removing(elysia::core::SceneObject&) {}
    virtual void on_runtime_attach() {}
    virtual void on_runtime_detach() {}
    virtual void on_runtime_reset() {}
    [[nodiscard]] virtual double fixed_step_frame_delta(double delta) const { return delta; }

    elysia::input::SceneInputRouter& input_router() noexcept { return _input_router; }
    [[nodiscard]] bool owns_object(const elysia::core::SceneObject&) const;
    [[nodiscard]] bool contains_object_address(const elysia::core::SceneObject*) const;
    void clear_scene_objects();
    [[nodiscard]] std::span<const elysia::physics::ColliderId>
        registered_physics_colliders(const elysia::core::SceneObject& object) const noexcept;

    void notify_scene_request(const SceneRequest& request);
    void request_scene_switch(const SceneRoute& route);
    void request_scene_switch(SceneKey target, const ScenePayload& payload = {},
                              SceneReloadMode reload_mode = SceneReloadMode::Reuse);
    void request_quit();

    [[nodiscard]] const SceneRuntimeContext& runtime_context() const;
    [[nodiscard]] elysia::physics::PhysicsWorld* try_physics_world() noexcept
    {
        return _physics_world.get();
    }
    [[nodiscard]] const elysia::physics::PhysicsWorld* try_physics_world() const noexcept
    {
        return _physics_world.get();
    }
    [[nodiscard]] elysia::physics::PhysicsWorld& physics_world();
    [[nodiscard]] const elysia::physics::PhysicsWorld& physics_world() const;
    [[nodiscard]] virtual std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const;

    bool _paused = false;

private:
    friend class elysia::input::SceneInputRouter;
    friend class SceneFactory;
    friend class SceneManager;
    friend class SceneTestAccess;

    void lifecycle_enter(const ScenePayload& payload);
    void lifecycle_exit();
    void lifecycle_reset();
    void lifecycle_input(const elysia::input::InputSnapshot& input);
    void lifecycle_update(double delta);
    void lifecycle_render(SDL_Renderer* renderer);
    void attach_runtime_services();
    void detach_runtime_services();
    void prepare_for_destruction();

    void bind_runtime_context(const SceneRuntimeContext& context) noexcept;
    void clear_runtime_context() noexcept;
    void set_local_players(elysia::input::LocalPlayerRegistry& players) noexcept { _players = &players; }
    void set_ui_device_access(elysia::input::UiDeviceAccess& access) noexcept
    {
        _input_router.set_ui_access(access);
    }
    void reset_input_routing();

    void visit_game_objects(elysia::core::DepthLayerMask layers,
                            const elysia::object_query::GameObjectVisitor& visitor) const override;
    void dispatch_ui_frame(const elysia::ui::UiInputFrame& input);
    bool dispatch_ui_events(const std::vector<elysia::ui::UiInputEvent>& events);
    void register_scene_object_interfaces(elysia::core::SceneObject* object);
    void remove_destroyed_objects();
    void release_owned_objects_noexcept() noexcept;
    void rollback_owned_object(elysia::core::SceneObject* object) noexcept;
    bool add_game_object(std::unique_ptr<elysia::core::GameObject> object);
    bool add_ui_root(std::unique_ptr<elysia::ui::UiElement> object);

    struct UpdatableEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::core::Updatable* updatable = nullptr;
    };
    struct UiInputFrameReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::ui::UiInputFrameReceiver* receiver = nullptr;
    };
    struct UiInputEventReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::ui::UiInputEventReceiver* receiver = nullptr;
    };
    struct PhysicsRegistrationEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::physics::PhysicsObjectHandle handle{};
        std::vector<elysia::physics::ColliderId> colliders;
    };

    SceneRuntimeFeatures _features;
    std::unique_ptr<FixedStepRuntime> _fixed_step;
    std::unique_ptr<elysia::physics::PhysicsWorld> _physics_world;

    std::array<std::vector<std::unique_ptr<elysia::core::GameObject>>,
               static_cast<std::size_t>(elysia::core::DepthLayer::Count)>
        _object_layers;
    std::vector<std::unique_ptr<elysia::ui::UiElement>> _ui_roots;
    std::vector<UpdatableEntry> _updatables;
    std::vector<UiInputFrameReceiverEntry> _ui_frame_receivers;
    std::vector<UiInputEventReceiverEntry> _ui_event_receivers;
    std::vector<PhysicsRegistrationEntry> _physics_registrations;

    elysia::input::LocalPlayerRegistry _standalone_players;
    elysia::input::LocalPlayerRegistry* _players = &_standalone_players;
    elysia::input::SceneInputRouter _input_router{*this};
    const SceneRuntimeContext* _runtime_context = nullptr;
    SceneLifecycleState _lifecycle_state = SceneLifecycleState::Inactive;
    bool _retiring_objects = false;
    bool _runtime_services_attached = false;
};
} // namespace elysia::scene
