#include "sandbox_scene.h"

#include "example_scene_keys.h"
#include "main_menu_scene.h"

#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/layout/ui_layout_types.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/window/ui_window.h"

#include <memory>

namespace example::scene
{
void SandboxScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    (void)payload;

    if (!_window || _window->is_destroyed())
        build_ui();

    if (_window && !_window->is_destroyed())
    {
        _window->set_active(true);
        _window->set_visible(true);
        _window->focus_first_available_scope();
    }
}

void SandboxScene::on_exit()
{
}

void SandboxScene::reset()
{
}

void SandboxScene::build_ui()
{
    _window = Scene::create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 0, 0, 1280, 720 }, 10);
    if (!_window)
        return;

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0, 0, 720, 360 });

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 680, 100 }, 0,
        elysia::ui::ui_text_key("sandbox_scene.title"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_typography_role(elysia::typography::UiTypographyRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    title->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    content->add_back(std::move(title));

    auto description = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 680, 120 }, 0,
        elysia::ui::ui_text_key("sandbox_scene.description"));
    description->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    description->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    content->add_back(std::move(description));

    auto back = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0, 0, 260, 75 });
    back->set_text_content(elysia::ui::ui_text_key("sandbox_scene.back"));
    back->set_sounds({
        .press = "system.button_click_down",
        .click = "system.button_click_up"
    });
    back->set_on_click([this]() { return_to_main_menu(); });
    content->add_back(std::move(back));

    elysia::ui::UiElement* added = _window->add_child(
        std::move(content), { elysia::ui::UiLayoutAnchor::Center });
    if (auto* list = dynamic_cast<elysia::ui::UiListContainer*>(added))
        _window->register_focus_scope(*list);

    _window->set_on_cancel([this]() { return_to_main_menu(); });
}

void SandboxScene::return_to_main_menu()
{
    Scene::request_scene_switch(
        ExampleSceneKeys::MainMenu,
        MainMenuEnterPayload{ .replay_theme_music = false },
        elysia::scene::SceneReloadMode::Reuse);
}
}
