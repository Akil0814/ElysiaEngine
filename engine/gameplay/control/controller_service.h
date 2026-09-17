#pragma once
#include "controller_manager.h"
#include <optional>
namespace elysia::gameplay
{
class ControllerService
{
  public:
    static ControllerService *instance()
    {
        static ControllerService service;
        return &service;
    }
    std::expected<void, ControllerError> begin_session()
    {
        return manager().begin_session();
    }
    void end_session()
    {
        manager().end_session();
    }
    bool session_active() const
    {
        return manager()._session;
    }
    template <class T, class... Args>
    std::expected<ControllerHandle, ControllerError> create(ControllerCreateInfo info, Args &&...args)
    {
        static_assert(std::is_base_of_v<Controller, T>);
        if (!manager()._session)
            return std::unexpected(ControllerError::NoSession);
        return manager().add(info, std::make_unique<T>(std::forward<Args>(args)...));
    }
    template <class T = Controller> T *get(ControllerHandle handle) const
    {
        auto *entry = manager().find(handle);
        return entry ? dynamic_cast<T *>(entry->controller.get()) : nullptr;
    }
    std::optional<ControllerDescription> describe(ControllerHandle handle) const
    {
        auto *e = manager().find(handle);
        if (!e)
            return {};
        return ControllerDescription{
            handle, e->scope, e->owner, e->bound_scene, e->command.binding_generation, e->target != nullptr};
    }
    bool remove(ControllerHandle handle)
    {
        return manager().remove(handle);
    }
    bool unbind_target(ControllerHandle handle)
    {
        return manager().unbind(handle);
    }
    std::expected<void, ControllerError> bind_target(ControllerHandle handle, SceneControlContext &context,
                                                     elysia::core::GameObject &target)
    {
        return manager().bind(handle, context.token(), target);
    }
    std::expected<void, ControllerError> replace_input_map(ControllerHandle handle,
                                                           elysia::input::InputActionMap map)
    {
        return manager().replace_map(handle, std::move(map));
    }

  private:
    ControllerService() = default;
    static ControllerManager &manager()
    {
        return *ControllerManager::instance();
    }
};
} // namespace elysia::gameplay
