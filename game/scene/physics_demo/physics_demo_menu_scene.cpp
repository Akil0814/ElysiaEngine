#include "physics_demo_menu_scene.h"
#include "physics_demo_layout.h"

#include "../example_scene_keys.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/text/ui_text_content.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/window/ui_window.h"

#include <memory>
#include <stdexcept>

namespace example::scene
{
namespace
{
std::unique_ptr<elysia::ui::UiButton> button(const char* text_key)
{
    auto result = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{0, 0, 420, 58});
    result->set_text_content(elysia::ui::ui_text_key(text_key));
    result->set_sounds({
        .press = "system.button_click_down",
        .click = "system.button_click_up"});
    return result;
}
}

void PhysicsDemoMenuScene::on_enter(
    const elysia::scene::ScenePayload& payload)
{
    const DemoScenePayload* demo_payload =
        elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (!demo_payload
        || !elysia::scene::SceneKeys::is_supported(
            demo_payload->return_route.target))
    {
        throw std::logic_error(
            "PhysicsDemoMenuScene requires DemoScenePayload with a valid return route.");
    }
    _return_route = demo_payload->return_route;
    if (!_window || _window->is_destroyed())
        build_ui();
    if (_window)
    {
        _window->set_active(true);
        _window->set_visible(true);
        _window->focus_first_available_scope();
    }
}

void PhysicsDemoMenuScene::on_exit()
{
    if (_window)
    {
        _window->set_active(false);
        _window->set_visible(false);
    }
}

void PhysicsDemoMenuScene::reset()
{
    if (_window)
        _window->destroy();
    _window = nullptr;
    _return_route = {};
}

void PhysicsDemoMenuScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            return_to_caller();
            return;
        }
    elysia::scene::Scene::on_input(input, events);
}

void PhysicsDemoMenuScene::build_ui()
{
    const PhysicsDemoLayout layout = make_physics_demo_layout(
        static_cast<float>(runtime_context().logical_width()),
        static_cast<float>(runtime_context().logical_height()));
    _window = create_and_add_object<elysia::ui::UiWindow>(
        layout.viewport, 100);
    if (!_window)
        return;
    _window->set_on_cancel([this] { return_to_caller(); });
    auto list = std::make_unique<elysia::ui::UiListContainer>(
        layout.menu_list);
    list->set_item_spacing(18.0f);
    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{0, 0, 420, 72}, 0,
        elysia::ui::ui_text_key("physics_demo_menu.title"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    list->add_back(std::move(title));

    auto collider = button("physics_demo_menu.collider");
    collider->set_on_click([this] {
        request_scene_switch(
            ExampleSceneKeys::PhysicsCollisionTest,
            DemoScenePayload{.return_route = make_menu_route()},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(collider));
    auto platform = button("physics_demo_menu.platform_tile");
    platform->set_on_click([this] {
        request_scene_switch(
            ExampleSceneKeys::PlatformTilePhysicsTest,
            DemoScenePayload{.return_route = make_menu_route()},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(platform));
    auto top_down = button("physics_demo_menu.top_down_tile");
    top_down->set_on_click([this] {
        request_scene_switch(
            ExampleSceneKeys::TopDownTilePhysicsTest,
            DemoScenePayload{.return_route = make_menu_route()},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(top_down));
    auto back = button("physics_demo_menu.back");
    back->set_on_click([this] { return_to_caller(); });
    list->add_back(std::move(back));

    auto* list_ptr = list.get();
    _window->add_child(
        std::move(list), physics_demo_layout_options(layout.menu_list));
    _window->register_focus_scope(*list_ptr);
}

void PhysicsDemoMenuScene::return_to_caller()
{
    if (elysia::scene::SceneKeys::is_supported(_return_route.target))
        request_scene_switch(_return_route);
}

elysia::scene::SceneRoute PhysicsDemoMenuScene::make_menu_route() const
{
    return elysia::scene::SceneRoute{
        .target = ExampleSceneKeys::PhysicsDemoMenu,
        .payload = DemoScenePayload{.return_route = _return_route},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse};
}
}
