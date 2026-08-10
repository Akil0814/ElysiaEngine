#include "scene.h"

#include "detail/scene_input_order.h"

#include "../core/interface/updatable.h"
#include "../core/render/debug_draw_projection.h"
#include "../core/render/render_command_projection.h"
#include "../core/render/sdl_render_command_executor.h"
#include "../input/contracts/raw_input_event_receiver.h"
#include "../tools/debug_draw.h"
#include "../input/contracts/raw_input_frame_receiver.h"
#include "../ui/core/ui_render_command_range_utils.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace elysia::scene
{
namespace
{
template <typename Entry>
void erase_destroyed_entries(std::vector<Entry>& entries)
{
    std::erase_if(entries, [](const Entry& entry)
    {
        return !entry.object || entry.object->is_destroyed();
    });
}
}

void Scene::on_input(const elysia::input::RawInputFrame& input, const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const RawInputFrameReceiverEntry& entry : _frame_receivers)
    {
        elysia::core::SceneObject* object = entry.object;

        if (!object || object->is_destroyed() || !object->is_active())
            continue;

        if (_paused && !object->receive_input_when_paused())
            continue;

        entry.receiver->on_raw_input_frame(input);
    }

    for (const elysia::input::RawInputEvent& input_event : events)
    {
        for (const RawInputEventReceiverEntry& entry : _event_receivers)
        {
            elysia::core::SceneObject* object = entry.object;

            if (!object || object->is_destroyed() || !object->is_active())
                continue;

            if (_paused && !object->receive_input_when_paused())
                continue;

            if (entry.receiver->on_raw_input_event(input_event))
                break;
        }
    }

    const elysia::ui::UiInputFrame ui_input = _ui_input_router.route_frame(input);
    dispatch_ui_frame(ui_input);

    for (const elysia::input::RawInputEvent& raw_event : events)
    {
        dispatch_ui_events(_ui_input_router.route_event(raw_event));
    }

    dispatch_ui_events(_ui_input_router.synthesize_events(input));
}

void Scene::on_update(double delta)
{
    for (const UpdatableEntry& entry : _updatables)
    {
        elysia::core::SceneObject* object = entry.object;

        if (!object || object->is_destroyed() || !object->is_active())
            continue;

        if (_paused && !object->update_when_paused())
            continue;

        entry.updatable->update(delta);
    }

    for (const std::unique_ptr<elysia::ui::UiElement>& ui_root : _ui_roots)
    {
        if (!ui_root || ui_root->is_destroyed() || !ui_root->is_active())
            continue;
        if (_paused && !ui_root->update_when_paused())
            continue;
        ui_root->update_presentation_animations(delta);
    }

    if (!_paused)
    {
        _physics_system.step(_physics_body_entries, delta);
        _collision_system.dispatch_events(_collider_entries, delta);
    }

    auto* camera_manager = elysia::camera::CameraManager::instance();
    camera_manager->set_focus_rect(
        elysia::camera::CameraSlot::Main,
        resolve_camera_focus_rect()
    );
    camera_manager->update(delta);

    remove_destroyed_objects();
}

void Scene::on_render(SDL_Renderer* renderer)
{
    if (!renderer)
        return;

    std::vector<elysia::core::RenderCommand> render_commands;
    std::vector<elysia::core::ScreenRenderCommand> projected_render_commands;
    std::vector<elysia::core::UiRenderCommand> ui_render_commands;
    render_commands.reserve(256);
    projected_render_commands.reserve(256);
    ui_render_commands.reserve(256);

    for (const auto& layer : _object_layers)
    {
        render_commands.clear();
        projected_render_commands.clear();

        for (const std::unique_ptr<elysia::core::GameObject>& obj : layer)
        {
            if (!obj || obj->is_destroyed() || !obj->is_visible())
                continue;

            obj->submit_render_commands(render_commands);
        }

        elysia::core::project_render_commands_to_screen(
            render_commands,
            camera(),
            projected_render_commands
        );
        elysia::core::execute_render_commands(renderer, projected_render_commands);
    }

    elysia::tools::DebugDraw* debug_draw =
        elysia::tools::DebugDraw::instance();
    if (debug_draw->enabled())
    {
        std::vector<elysia::core::UiRenderCommand> debug_draw_commands;
        debug_draw_commands.reserve(debug_draw->commands().size());
        elysia::core::append_projected_debug_draw_commands(
            debug_draw->commands(),
            debug_draw->enabled_categories(),
            camera(),
            debug_draw_commands
        );
        elysia::core::execute_render_commands(renderer, debug_draw_commands);
    }

    ui_render_commands.clear();
    for (const auto& ui_root : _ui_roots)
    {
        if (!ui_root || ui_root->is_destroyed() || !ui_root->is_visible())
            continue;

        const std::size_t begin = ui_render_commands.size();
        ui_root->submit_ui_render_commands(ui_render_commands);
        elysia::ui::render_command_range_utils::apply_translation_to_range(
            ui_render_commands,begin,ui_root->presentation_translation());
    }

    elysia::core::execute_render_commands(renderer, ui_render_commands);
}

void Scene::register_scene_object_interfaces(elysia::core::SceneObject* object)
{
    if (!object)
        return;

    if (elysia::core::Updatable* updatable = dynamic_cast<elysia::core::Updatable*>(object))
        _updatables.push_back(UpdatableEntry{ object, updatable });

    if (elysia::input::RawInputFrameReceiver* receiver = dynamic_cast<elysia::input::RawInputFrameReceiver*>(object))
        _frame_receivers.push_back(RawInputFrameReceiverEntry{ object, receiver });

    if (elysia::input::RawInputEventReceiver* receiver = dynamic_cast<elysia::input::RawInputEventReceiver*>(object))
    {
        scene_input_order::insert_receiver_entry_sorted(
            _event_receivers,
            RawInputEventReceiverEntry{ object, receiver }
        );
    }

    if (elysia::ui::UiInputFrameReceiver* receiver = dynamic_cast<elysia::ui::UiInputFrameReceiver*>(object))
    {
        scene_input_order::insert_receiver_entry_sorted(
            _ui_frame_receivers,
            UiInputFrameReceiverEntry{ object, receiver }
        );
    }

    if (elysia::ui::UiInputEventReceiver* receiver = dynamic_cast<elysia::ui::UiInputEventReceiver*>(object))
    {
        scene_input_order::insert_receiver_entry_sorted(
            _ui_event_receivers,
            UiInputEventReceiverEntry{ object, receiver }
        );
    }

    elysia::core::GameObject* game_object = dynamic_cast<elysia::core::GameObject*>(object);
    if (game_object)
    {
        if (elysia::physics::PhysicsBodyProvider* body_provider =
            dynamic_cast<elysia::physics::PhysicsBodyProvider*>(object))
        {
            _physics_body_entries.push_back(PhysicsBodyEntry{
                object,
                game_object,
                body_provider
            });
        }

        if (elysia::physics::ColliderProvider* collider_provider =
            dynamic_cast<elysia::physics::ColliderProvider*>(object))
        {
            _collider_entries.push_back(ColliderEntry{
                object,
                game_object,
                collider_provider
            });
        }
    }

    on_scene_object_registered(*object);
}

void Scene::visit_game_objects(
    const elysia::object_query::GameObjectVisitor& visitor) const
{
    for (const auto& layer : _object_layers)
    {
        for (const std::unique_ptr<elysia::core::GameObject>& object : layer)
        {
            if (object && !visitor(*object))
                return;
        }
    }
}

void Scene::dispatch_ui_frame(const elysia::ui::UiInputFrame& input)
{
    for (const UiInputFrameReceiverEntry& entry : _ui_frame_receivers)
    {
        elysia::core::SceneObject* object = entry.object;

        if (!object || object->is_destroyed() || !object->is_active())
            continue;

        if (_paused && !object->receive_input_when_paused())
            continue;

        entry.receiver->on_ui_input_frame(input);
    }
}

void Scene::dispatch_ui_events(const std::vector<elysia::ui::UiInputEvent>& events)
{
    for (const elysia::ui::UiInputEvent& ui_event : events)
    {
        for (const UiInputEventReceiverEntry& entry : _ui_event_receivers)
        {
            elysia::core::SceneObject* object = entry.object;

            if (!object || object->is_destroyed() || !object->is_active())
                continue;

            if (_paused && !object->receive_input_when_paused())
                continue;

            if (entry.receiver->on_ui_input_event(ui_event))
                break;
        }
    }
}

void Scene::remove_destroyed_objects()
{
    erase_destroyed_entries(_updatables);
    erase_destroyed_entries(_frame_receivers);
    erase_destroyed_entries(_event_receivers);
    erase_destroyed_entries(_ui_frame_receivers);
    erase_destroyed_entries(_ui_event_receivers);
    erase_destroyed_entries(_physics_body_entries);
    erase_destroyed_entries(_collider_entries);

    for (auto& layer : _object_layers)
    {
        std::erase_if(layer, [](const std::unique_ptr<elysia::core::GameObject>& object)
        {
            return !object || object->is_destroyed();
        });
    }

    std::erase_if(_ui_roots, [](const std::unique_ptr<elysia::ui::UiElement>& object)
    {
        return !object || object->is_destroyed();
    });
}

void Scene::notify_scene_request(const SceneRequest& request)
{
    notify_observers(
        [&](SceneRequestObserver& observer)
        {
            observer.on_scene_request(request);
        }
    );
}

void Scene::request_scene_switch(
    const SceneRoute& route)
{
    SceneRequest request;
    request.type = SceneRequestType::Switch;
    request.route = route;

    notify_scene_request(request);
}

void Scene::request_scene_switch(
    SceneKey target,
    const ScenePayload& payload,
    SceneReloadMode reload_mode)
{
    request_scene_switch(SceneRoute{
        .target = target,
        .payload = payload,
        .reload_mode = reload_mode
    });
}

void Scene::request_quit()
{
    SceneRequest request;
    request.type = SceneRequestType::Quit;

    notify_scene_request(request);
}

void Scene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    (void)object;
}

const SceneRuntimeContext& Scene::runtime_context() const
{
    if (!_runtime_context)
        throw std::logic_error("Scene::runtime_context called before a runtime context was bound.");

    return *_runtime_context;
}

void Scene::bind_runtime_context(const SceneRuntimeContext& context) noexcept
{
    _runtime_context = &context;
}

void Scene::clear_runtime_context() noexcept
{
    _runtime_context = nullptr;
}

const elysia::camera::Camera& Scene::camera() const noexcept
{
    return elysia::camera::CameraManager::instance()->camera(_render_camera_slot);
}

elysia::camera::CameraSlot Scene::render_camera_slot() const noexcept
{
    return _render_camera_slot;
}

void Scene::set_render_camera_slot(elysia::camera::CameraSlot slot) noexcept
{
    assert(slot != elysia::camera::CameraSlot::Count);
    if (slot == elysia::camera::CameraSlot::Count)
        return;

    _render_camera_slot = slot;
}

std::optional<elysia::core::Rect> Scene::resolve_camera_focus_rect() const
{
    return std::nullopt;
}

bool Scene::add_game_object(std::unique_ptr<elysia::core::GameObject> object)
{
    if (!object)
        return false;

    const size_t layer_index = static_cast<size_t>(object->depth_layer());

    if (layer_index >= _object_layers.size())
        return false;

    std::vector<std::unique_ptr<elysia::core::GameObject>>& layer = _object_layers[layer_index];

    auto iter = std::upper_bound(
        layer.begin(),
        layer.end(),
        object->order_in_layer(),
        [](int order, const std::unique_ptr<elysia::core::GameObject>& existing)
        {
            return order < existing->order_in_layer();
        }
    );

    layer.insert(iter, std::move(object));
    return true;
}

bool Scene::add_ui_root(std::unique_ptr<elysia::ui::UiElement> object)
{
    if (!object)
        return false;

    auto iter = std::upper_bound(
        _ui_roots.begin(),
        _ui_roots.end(),
        object->order(),
        [](int order, const std::unique_ptr<elysia::ui::UiElement>& existing)
        {
            return order < existing->order();
        }
    );

    _ui_roots.insert(iter, std::move(object));
    return true;
}
}
