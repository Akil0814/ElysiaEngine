#include "sandbox_scene.h"

#include "example_scene_keys.h"
#include "main_menu_scene.h"

#include "../../engine/tools/logger.h"
#include "../../engine/ui/containers/ui_list_container.h"
#include "../../engine/ui/layout/ui_layout_types.h"
#include "../../engine/ui/widgets/image/ui_animation.h"
#include "../../engine/ui/widgets/label/ui_label.h"
#include "../../engine/ui/widgets/ui_button.h"
#include "../../engine/ui/window/ui_window.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace example::scene
{
namespace
{
constexpr std::string_view kIdleAnimation = "RyougiShiki.idle";
constexpr std::string_view kRunAnimation = "RyougiShiki.run_loop";
constexpr std::array<std::string_view, 6> kAttackAnimations{
    "RyougiShiki.attack_normal.0",
    "RyougiShiki.attack_normal.1",
    "RyougiShiki.attack_normal.2",
    "RyougiShiki.attack_normal.3",
    "RyougiShiki.attack_normal.4",
    "RyougiShiki.attack_normal.5"
};

std::unique_ptr<elysia::ui::UiButton> make_button(
    std::string_view text_key,
    float width)
{
    auto button = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{ 0, 0, width, 56 });
    button->set_text_content(elysia::ui::ui_text_key(std::string{text_key}));
    button->set_sounds({
        .press = "system.button_click_down",
        .click = "system.button_click_up"
    });
    return button;
}
}

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

    _attack_segment_index = 0;
    update_attack_segment_label();
    play_idle();
}

void SandboxScene::on_exit()
{
    if (_window && !_window->is_destroyed())
    {
        _window->set_active(false);
        _window->set_visible(false);
    }
}

void SandboxScene::reset()
{
    _attack_segment_index = 0;
    update_attack_segment_label();
    if (_animation_preview && !_animation_preview->is_destroyed())
        _animation_preview->reset();
}

void SandboxScene::build_ui()
{
    _window = Scene::create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{ 0, 0, 1280, 720 }, 10);
    if (!_window)
        return;

    auto content = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0, 0, 900, 680 });
    content->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    content->set_item_spacing(8.0f);

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 880, 68 }, 0,
        elysia::ui::ui_text_key("sandbox_scene.title"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_typography_role(elysia::typography::UiTypographyRole::Title);
    title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    title->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    content->add_back(std::move(title));

    auto description = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 880, 52 }, 0,
        elysia::ui::ui_text_key("sandbox_scene.description"));
    description->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    description->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    content->add_back(std::move(description));

    auto animation_preview = std::make_unique<elysia::ui::UiAnimation>(
        elysia::core::Rect{ 0, 0, 256, 256 });
    _animation_preview = animation_preview.get();
    content->add_back(std::move(animation_preview));

    auto animation_controls = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0, 0, 512, 56 });
    animation_controls->set_direction(elysia::ui::UiListDirection::Horizontal);
    animation_controls->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    animation_controls->set_item_spacing(12.0f);

    auto idle = make_button("sandbox_scene.animation_idle", 250.0f);
    idle->set_on_click([this]() { play_idle(); });
    animation_controls->add_back(std::move(idle));

    auto run = make_button("sandbox_scene.animation_run", 250.0f);
    run->set_on_click([this]() { play_run(); });
    animation_controls->add_back(std::move(run));
    content->add_back(std::move(animation_controls));

    auto segment_status = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0, 0, 420, 40 });
    segment_status->set_direction(elysia::ui::UiListDirection::Horizontal);
    segment_status->set_cross_align(elysia::ui::UiLayoutAlign::Center);

    auto segment_title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 280, 40 }, 0,
        elysia::ui::ui_text_key("sandbox_scene.attack_segment"));
    segment_title->set_horizontal_align(elysia::ui::TextHorizontalAlign::Right);
    segment_title->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    segment_status->add_back(std::move(segment_title));

    auto segment_number = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{ 0, 0, 140, 40 });
    segment_number->set_horizontal_align(elysia::ui::TextHorizontalAlign::Center);
    segment_number->set_vertical_align(elysia::ui::TextVerticalAlign::Center);
    _attack_segment_label = segment_number.get();
    segment_status->add_back(std::move(segment_number));
    content->add_back(std::move(segment_status));

    auto attack_controls = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{ 0, 0, 774, 56 });
    attack_controls->set_direction(elysia::ui::UiListDirection::Horizontal);
    attack_controls->set_cross_align(elysia::ui::UiLayoutAlign::Center);
    attack_controls->set_item_spacing(12.0f);

    auto previous = make_button("sandbox_scene.attack_previous", 250.0f);
    previous->set_on_click([this]() { select_previous_attack(); });
    attack_controls->add_back(std::move(previous));

    auto replay = make_button("sandbox_scene.attack_replay", 250.0f);
    replay->set_on_click([this]() { play_current_attack(); });
    attack_controls->add_back(std::move(replay));

    auto next = make_button("sandbox_scene.attack_next", 250.0f);
    next->set_on_click([this]() { select_next_attack(); });
    attack_controls->add_back(std::move(next));
    content->add_back(std::move(attack_controls));

    auto back = make_button("sandbox_scene.back", 260.0f);
    back->set_on_click([this]() { return_to_main_menu(); });
    content->add_back(std::move(back));

    elysia::ui::UiElement* added = _window->add_child(
        std::move(content), { elysia::ui::UiLayoutAnchor::Center });
    if (auto* list = dynamic_cast<elysia::ui::UiListContainer*>(added))
        _window->register_focus_scope(*list);

    _window->set_on_cancel([this]() { return_to_main_menu(); });
}

void SandboxScene::play_animation(std::string_view animation_key)
{
    if (!_animation_preview || _animation_preview->is_destroyed())
        return;

    if (!_animation_preview->set_animation_key(animation_key))
    {
        _animation_preview->set_visible(false);
        ELYSIA_LOG_WARN(
            "sandbox",
            "Could not bind Ryougi sample animation: " << animation_key);
        return;
    }

    _animation_preview->set_visible(true);
    _animation_preview->play();
}

void SandboxScene::play_idle()
{
    play_animation(kIdleAnimation);
}

void SandboxScene::play_run()
{
    play_animation(kRunAnimation);
}

void SandboxScene::play_current_attack()
{
    play_animation(kAttackAnimations[_attack_segment_index]);
}

void SandboxScene::select_previous_attack()
{
    _attack_segment_index = _attack_segment_index == 0
        ? kAttackAnimations.size() - 1
        : _attack_segment_index - 1;
    update_attack_segment_label();
    play_current_attack();
}

void SandboxScene::select_next_attack()
{
    _attack_segment_index =
        (_attack_segment_index + 1) % kAttackAnimations.size();
    update_attack_segment_label();
    play_current_attack();
}

void SandboxScene::update_attack_segment_label()
{
    if (!_attack_segment_label || _attack_segment_label->is_destroyed())
        return;

    _attack_segment_label->set_text_content(elysia::ui::ui_raw_text(
        std::to_string(_attack_segment_index + 1)
        + " / " + std::to_string(kAttackAnimations.size())));
}

void SandboxScene::return_to_main_menu()
{
    Scene::request_scene_switch(
        ExampleSceneKeys::MainMenu,
        MainMenuEnterPayload{ .replay_theme_music = false },
        elysia::scene::SceneReloadMode::Reuse);
}
}
