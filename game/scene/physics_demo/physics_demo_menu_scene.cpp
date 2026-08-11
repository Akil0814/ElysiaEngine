#include "physics_demo_menu_scene.h"

#include "../example_scene_keys.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/text/ui_text_content.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/window/ui_window.h"

#include <memory>

namespace example::scene
{
namespace
{
std::unique_ptr<elysia::ui::UiButton> button(const char* text)
{
    auto result = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{0, 0, 420, 58});
    result->set_text_content(elysia::ui::ui_raw_text(text));
    return result;
}
}

void PhysicsDemoMenuScene::on_enter(const elysia::scene::ScenePayload&)
{
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
}

void PhysicsDemoMenuScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            return_to_main_menu();
            return;
        }
    elysia::scene::Scene::on_input(input, events);
}

void PhysicsDemoMenuScene::build_ui()
{
    _window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0, 0, 1280, 720}, 100);
    if (!_window)
        return;
    _window->set_on_cancel([this] { return_to_main_menu(); });
    auto list = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{430, 100, 420, 520});
    list->set_item_spacing(18.0f);
    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{0, 0, 420, 72}, 0,
        elysia::ui::ui_raw_text("Physics & Combat Demos"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    list->add_back(std::move(title));

    auto collider = button("Collider Combat");
    collider->set_on_click([this] {
        request_scene_switch(ExampleSceneKeys::PhysicsCollisionTest, {},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(collider));
    auto platform = button("Platform Tile Combat");
    platform->set_on_click([this] {
        request_scene_switch(ExampleSceneKeys::PlatformTilePhysicsTest, {},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(platform));
    auto top_down = button("Top-Down Tile Combat");
    top_down->set_on_click([this] {
        request_scene_switch(ExampleSceneKeys::TopDownTilePhysicsTest, {},
            elysia::scene::SceneReloadMode::Recreate); });
    list->add_back(std::move(top_down));
    auto back = button("Back to Main Menu");
    back->set_on_click([this] { return_to_main_menu(); });
    list->add_back(std::move(back));

    auto* list_ptr = list.get();
    _window->add_child(std::move(list));
    _window->register_focus_scope(*list_ptr);
}

void PhysicsDemoMenuScene::return_to_main_menu()
{
    request_scene_switch(ExampleSceneKeys::MainMenu, {},
        elysia::scene::SceneReloadMode::Reuse);
}
}
