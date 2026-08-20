#include "demo_gallery_scene.h"

#include "../example_scene_keys.h"
#include "../main_menu_scene.h"
#include "../../../engine/builtin/scenes/application_failure_scene_payload.h"
#include "../../../engine/elysia/elysia_scene_payload.h"
#include "../../../engine/input/raw_input_types.h"
#include "../../../engine/ui/composites/ui_confirmation_dialog.h"
#include "../../../engine/ui/containers/ui_list_container.h"
#include "../../../engine/ui/layout/ui_layout_types.h"
#include "../../../engine/ui/text/ui_text_content.h"
#include "../../../engine/ui/widgets/label/ui_label.h"
#include "../../../engine/ui/widgets/ui_button.h"
#include "../../../engine/ui/window/ui_window.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace example::scene
{
namespace
{
[[nodiscard]] bool is_valid_return_route(
    const elysia::scene::SceneRoute& route) noexcept
{
    return elysia::scene::SceneKeys::is_supported(route.target);
}

std::unique_ptr<elysia::ui::UiButton> make_button(
    const char* text_key,
    elysia::ui::UiButtonVisualRole role =
        elysia::ui::UiButtonVisualRole::Default)
{
    auto button = std::make_unique<elysia::ui::UiButton>(
        elysia::core::Rect{0.0f, 0.0f, 420.0f, 56.0f});
    button->set_text_content(elysia::ui::ui_text_key(text_key));
    button->set_visual_role(role);
    button->set_sounds({
        .press = "system.button_click_down",
        .click = "system.button_click_up"});
    return button;
}
}

void DemoGalleryScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const elysia::input::RawInputEvent& event : events)
    {
        if (event.control == elysia::input::RawInputControl::KeyEscape
            && event.type
                == elysia::input::RawInputEventType::ControlPressed)
        {
            if (_root_window && _failure_confirmation
                && _root_window->is_overlay_open(*_failure_confirmation))
            {
                _failure_confirmation->close();
            }
            else
            {
                return_to_caller();
            }
            return;
        }
    }

    elysia::scene::Scene::on_input(input, events);
}

void DemoGalleryScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const DemoScenePayload* demo_payload =
        elysia::scene::try_scene_payload<DemoScenePayload>(payload);
    if (!demo_payload || !is_valid_return_route(demo_payload->return_route))
    {
        throw std::logic_error(
            "DemoGalleryScene requires DemoScenePayload with a valid return route.");
    }

    _return_route = demo_payload->return_route;
    _paused = false;

    if (!_root_window || _root_window->is_destroyed())
        build_ui();

    reset_failure_confirmation();
    if (_root_window)
    {
        _root_window->set_visible(true);
        _root_window->set_active(true);
        _root_window->focus_first_available_scope();
    }
}

void DemoGalleryScene::on_exit()
{
    _paused = false;
    reset_failure_confirmation();
    if (_root_window && !_root_window->is_destroyed())
    {
        _root_window->set_active(false);
        _root_window->set_visible(false);
    }
}

void DemoGalleryScene::reset()
{
    _paused = false;
    _return_route = {};
    destroy_ui();
}

void DemoGalleryScene::build_ui()
{
    const float logical_width = static_cast<float>(
        std::max(0, runtime_context().logical_width()));
    const float logical_height = static_cast<float>(
        std::max(0, runtime_context().logical_height()));
    _root_window = create_and_add_object<elysia::ui::UiWindow>(
        elysia::core::Rect{0.0f, 0.0f, logical_width, logical_height}, 100);
    if (!_root_window)
        throw std::runtime_error(
            "DemoGalleryScene could not create its UiWindow.");

    _root_window->set_on_cancel([this]() { return_to_caller(); });

    _failure_confirmation =
        _root_window->create_child<elysia::ui::UiConfirmationDialog>(
            elysia::core::Rect{0.0f, 0.0f, 460.0f, 250.0f}, 10);
    if (_failure_confirmation)
    {
        _failure_confirmation->set_config(
            elysia::ui::UiConfirmationDialogConfig{
                .title = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.title"),
                .message = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.message"),
                .confirm = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.confirm"),
                .cancel = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.cancel"),
                .close = elysia::ui::ui_text_key(
                    "demo_gallery.failure_confirm.close"),
                .confirm_visual_role =
                    elysia::ui::UiButtonVisualRole::Danger});
        _failure_confirmation->set_on_confirm([this]() {
            request_scene_switch(
                elysia::builtin::make_application_failure_route(
                    elysia::builtin::ApplicationFailurePresentation::
                        RuntimeFatal,
                    "demo_gallery",
                    "Injected runtime failure from the Demo Gallery."));
        });
        (void)_failure_confirmation->register_with_window(*_root_window);
    }

    const float list_width = std::min(
        420.0f, std::max(0.0f, logical_width - 40.0f));
    const float list_height = std::min(
        620.0f, std::max(0.0f, logical_height - 40.0f));
    auto list = std::make_unique<elysia::ui::UiListContainer>(
        elysia::core::Rect{0.0f, 0.0f, list_width, list_height});
    list->set_item_spacing(12.0f);

    auto title = std::make_unique<elysia::ui::UiLabel>(
        elysia::core::Rect{0.0f, 0.0f, list_width, 72.0f}, 0,
        elysia::ui::ui_text_key("demo_gallery.title"));
    title->set_visual_role(elysia::ui::UiLabelVisualRole::Title);
    title->set_horizontal_align(
        elysia::ui::TextHorizontalAlign::Center);
    list->add_back(std::move(title));

    auto physics = make_button("demo_gallery.physics");
    physics->set_on_click([this]() {
        request_scene_switch(
            example::scene_keys::PhysicsCombatGallery,
            DemoScenePayload{.return_route = make_gallery_route()},
            elysia::scene::SceneReloadMode::Reuse);
    });
    list->add_back(std::move(physics));

    auto animation = make_button("demo_gallery.animation_preview");
    animation->set_on_click([this]() {
        request_scene_switch(
            example::scene_keys::AnimationPreview,
            DemoScenePayload{.return_route = make_gallery_route()},
            elysia::scene::SceneReloadMode::Reuse);
    });
    list->add_back(std::move(animation));

    auto ui_gallery = make_button("demo_gallery.ui_gallery");
    ui_gallery->set_on_click([this]() {
        request_scene_switch(
            example::scene_keys::UiComponentGallery,
            DemoScenePayload{.return_route = make_gallery_route()},
            elysia::scene::SceneReloadMode::Reuse);
    });
    list->add_back(std::move(ui_gallery));

    auto engine_features = make_button("demo_gallery.engine_features");
    engine_features->set_on_click([this]() {
        request_scene_switch(
            example::scene_keys::EngineFeatureLab,
            DemoScenePayload{.return_route = make_gallery_route()},
            elysia::scene::SceneReloadMode::Reuse);
    });
    list->add_back(std::move(engine_features));

    auto elysia_realm = make_button("demo_gallery.elysia_realm");
    elysia_realm->set_on_click([this]() {
        request_scene_switch(
            elysia::scene::SceneKeys::ElysiaRealm,
            elysia::realm::ElysiaScenePayload{
                .return_route = make_gallery_route()});
    });
    list->add_back(std::move(elysia_realm));

    auto failure = make_button(
        "demo_gallery.failure_test",
        elysia::ui::UiButtonVisualRole::Danger);
    failure->set_on_click([this]() {
        if (_failure_confirmation)
            _failure_confirmation->open();
    });
    list->add_back(std::move(failure));

    auto back = make_button("demo_gallery.back");
    back->set_on_click([this]() { return_to_caller(); });
    list->add_back(std::move(back));

    elysia::ui::UiListContainer* list_ptr = list.get();
    elysia::ui::UiLayoutChildOptions centered;
    centered._anchor = elysia::ui::UiLayoutAnchor::Center;
    centered._size_override = {list_width, list_height};
    centered._use_size_override = true;
    _root_window->add_child(std::move(list), centered);
    _root_window->register_focus_scope(*list_ptr);
}

void DemoGalleryScene::destroy_ui() noexcept
{
    if (_root_window)
        _root_window->destroy();
    _root_window = nullptr;
    _failure_confirmation = nullptr;
}

void DemoGalleryScene::reset_failure_confirmation() noexcept
{
    if (_failure_confirmation && !_failure_confirmation->is_destroyed())
        _failure_confirmation->close();
}

void DemoGalleryScene::return_to_caller()
{
    if (is_valid_return_route(_return_route))
        request_scene_switch(_return_route);
}

elysia::scene::SceneRoute DemoGalleryScene::make_gallery_route() const
{
    return elysia::scene::SceneRoute{
        .target = example::scene_keys::DemoGallery,
        .payload = DemoScenePayload{.return_route = _return_route},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse};
}
}
