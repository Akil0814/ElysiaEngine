#include "game/showcase/animation/animation_preview_scene.h"

#include "game/navigation/showcase_scene_keys.h"
#include "game/showcase/shared/showcase_enter_payload.h"

#include "engine/tools/logger.h"
#include "engine/ui/containers/ui_list_container.h"
#include "engine/ui/layout/ui_layout_types.h"
#include "engine/ui/widgets/image/ui_animation.h"
#include "engine/ui/widgets/label/ui_label.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"

#include <array>
#include <memory>
#include <stdexcept>
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

}

void AnimationPreviewScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const ShowcaseEnterPayload* demo_payload =
        elysia::scene::try_scene_payload<ShowcaseEnterPayload>(payload);
    if (!demo_payload
        || !elysia::scene::SceneKeys::is_supported(
            demo_payload->return_route.target))
    {
        throw std::logic_error(
            "AnimationPreviewScene requires ShowcaseEnterPayload with a valid return route.");
    }
    _return_route = demo_payload->return_route;

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

void AnimationPreviewScene::on_exit()
{
    if (_window && !_window->is_destroyed())
    {
        _window->set_active(false);
        _window->set_visible(false);
    }
}

void AnimationPreviewScene::on_reset()
{
    _attack_segment_index = 0;
    update_attack_segment_label();
    if (_view.animation && !_view.animation->is_destroyed())
        _view.animation->reset();
}

void AnimationPreviewScene::build_ui()
{
    _window=create_and_add_object<elysia::ui::UiWindow>(elysia::core::Rect{0,0,float(runtime_context().logical_width()),float(runtime_context().logical_height())},10);
    _view.build(*_window,{[this]{play_idle();},[this]{play_run();},[this]{select_previous_attack();},[this]{play_current_attack();},[this]{select_next_attack();}},[this]{return_to_caller();});
}

void AnimationPreviewScene::play_animation(std::string_view animation_key)
{
    if (!_view.animation || _view.animation->is_destroyed())
        return;

    if (!_view.animation->set_animation_key(animation_key))
    {
        _view.animation->set_visible(false);
        ELYSIA_LOG_WARN(
            "sandbox",
            "Could not bind Ryougi sample animation: " << animation_key);
        return;
    }

    _view.animation->set_visible(true);
    _view.animation->play();
}

void AnimationPreviewScene::play_idle()
{
    play_animation(kIdleAnimation);
}

void AnimationPreviewScene::play_run()
{
    play_animation(kRunAnimation);
}

void AnimationPreviewScene::play_current_attack()
{
    play_animation(kAttackAnimations[_attack_segment_index]);
}

void AnimationPreviewScene::select_previous_attack()
{
    _attack_segment_index = _attack_segment_index == 0
        ? kAttackAnimations.size() - 1
        : _attack_segment_index - 1;
    update_attack_segment_label();
    play_current_attack();
}

void AnimationPreviewScene::select_next_attack()
{
    _attack_segment_index =
        (_attack_segment_index + 1) % kAttackAnimations.size();
    update_attack_segment_label();
    play_current_attack();
}

void AnimationPreviewScene::update_attack_segment_label()
{
    if (!_view.segment || _view.segment->is_destroyed())
        return;

    _view.segment->set_text_content(elysia::ui::ui_raw_text(
        std::to_string(_attack_segment_index + 1)
        + " / " + std::to_string(kAttackAnimations.size())));
}

void AnimationPreviewScene::return_to_caller()
{
    if (elysia::scene::SceneKeys::is_supported(_return_route.target))
        Scene::request_scene_switch(_return_route);
}
}
